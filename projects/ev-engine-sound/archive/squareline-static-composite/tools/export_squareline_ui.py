#!/usr/bin/env python3
"""Export the tracked SquareLine source into deterministic LVGL C files."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import plistlib
import shutil
import signal
import subprocess
import tempfile
import time


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_PROJECT = ROOT / "ui/squareline/ev-engine-sound.spj"
DEFAULT_OUTPUT = ROOT / "ui/generated"
DEFAULT_APP = Path("/Applications/SquareLine_Studio_1.6.2.app")
SUCCESS = "Export finished successfully."
FAILURES = ("Export failed", "Project Loading failed", "Exception:")


def app_version(app: Path) -> str:
    with (app / "Contents/Info.plist").open("rb") as file:
        return str(plistlib.load(file).get("CFBundleShortVersionString", ""))


def validate_project(project: Path) -> None:
    data = json.loads(project.read_text(encoding="utf-8"))
    info = data.get("info", {})
    actual = (info.get("editor_version"), info.get("lvgl_version"),
              info.get("width"), info.get("height"), info.get("BitDepth"))
    expected = ("1.6.2", "9.5", 320, 240, 16)
    if actual != expected:
        raise SystemExit(f"SquareLine project contract mismatch: {actual!r} != {expected!r}")
    screens = data.get("root", {}).get("children", [])
    if len(screens) != 1:
        raise SystemExit(f"Expected one generated screen, found {len(screens)}")


def export(app: Path, project: Path, destination: Path, timeout: float) -> None:
    if app_version(app) != "1.6.2":
        raise SystemExit(f"SquareLine 1.6.2 required, found {app_version(app)!r} at {app}")
    executable = app / "Contents/MacOS/SquareLine_Studio"
    if not executable.is_file():
        raise SystemExit(f"SquareLine executable not found: {executable}")

    with tempfile.TemporaryDirectory(prefix="ev-squareline-") as temporary:
        temporary_path = Path(temporary)
        export_root = temporary_path / "export"
        log = temporary_path / "squareline-export.log"
        export_root.mkdir()
        command = [
            str(executable), "-batchmode", "-logFile", str(log),
            "-projectfile", str(project.resolve()),
            "-exportfolder", str(export_root),
        ]
        process = subprocess.Popen(command, stdout=subprocess.DEVNULL,
                                   stderr=subprocess.STDOUT, start_new_session=True)
        deadline = time.monotonic() + timeout
        seen = ""
        try:
            while time.monotonic() < deadline:
                if log.exists():
                    seen = log.read_text(encoding="utf-8", errors="replace")
                    if SUCCESS in seen:
                        break
                    if any(marker in seen for marker in FAILURES):
                        raise RuntimeError("SquareLine reported an export failure")
                if process.poll() is not None:
                    raise RuntimeError(f"SquareLine exited before completing export: {process.returncode}")
                time.sleep(0.2)
            else:
                raise TimeoutError(f"SquareLine export did not finish within {timeout:.0f}s")
        finally:
            if process.poll() is None:
                os.killpg(process.pid, signal.SIGTERM)
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    os.killpg(process.pid, signal.SIGKILL)
                    process.wait(timeout=5)

        generated = export_root / "ev-engine-sound/ui"
        required = [
            generated / "ui.c",
            generated / "ui.h",
            generated / "screens/ui_MainScreen.c",
            generated / "screens/ui_MainScreen.h",
        ]
        missing = [str(path) for path in required if not path.is_file()]
        if missing:
            raise RuntimeError(f"SquareLine export is incomplete: {missing}")
        header = required[2].read_text(encoding="utf-8")[:300]
        if "SquareLine Studio 1.6.2" not in header or "LVGL version: 9.5" not in header:
            raise RuntimeError("Generated source has an unexpected SquareLine/LVGL version")

        metadata = generated / "project.info"
        if metadata.is_file():
            raw_metadata = json.loads(metadata.read_text(encoding="utf-8"))
            stable_metadata = {
                key: raw_metadata.get(key)
                for key in ("project_name", "editor_version", "project_version")
            }
            metadata.write_text(json.dumps(stable_metadata, indent=4) + "\n",
                                encoding="utf-8")

        staging = destination.with_name(destination.name + ".new")
        shutil.rmtree(staging, ignore_errors=True)
        shutil.copytree(generated, staging)
        shutil.rmtree(destination, ignore_errors=True)
        staging.rename(destination)
        print(f"Exported {project} -> {destination}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--app", type=Path, default=DEFAULT_APP)
    parser.add_argument("--project", type=Path, default=DEFAULT_PROJECT)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--timeout", type=float, default=90.0)
    args = parser.parse_args()
    validate_project(args.project)
    export(args.app, args.project, args.output, args.timeout)


if __name__ == "__main__":
    main()
