#!/usr/bin/env python3
"""Fail-closed review and snapshot export for explicitly approved publication units.

This tool never approves, pushes, or uploads. Normal YAML needs PyYAML; JSON-formatted
YAML needs only Python 3.9+. A Git hook protects against mistakes, not --no-verify.
"""

from __future__ import annotations

import argparse
import copy
import ctypes
import hashlib
import html
from html.parser import HTMLParser
import json
import os
from pathlib import Path, PurePosixPath
import posixpath
import re
import shutil
import stat
import subprocess
import sys
import tempfile
from typing import Any, Optional
from urllib.parse import unquote, urlsplit


STATES = {"draft", "ready", "approved", "published"}
STATUSES = {"idea", "prototype", "build-verified", "hardware-verified", "stable", "retired"}
TARGETS = {"git", "platform"}
APPROVER_RE = re.compile(r"^[A-Za-z0-9_-]{1,64}$")
EXPECTED_HOOK = b'''#!/bin/sh
# Fail closed if the publication guard is missing or cannot run.
set -eu
publication_root=$(git rev-parse --show-toplevel) || exit 1
if [ ! -x "$publication_root/scripts/pub" ]; then
    printf '%s\\n' 'Publication guard missing; push denied.' >&2
    exit 1
fi
exec "$publication_root/scripts/pub" --root "$publication_root" guard-push "$1" "$2"
'''
MAX_FILE_BYTES = 10 * 1024 * 1024
FORBIDDEN_PARTS = {
    ".git", ".svn", ".hg", ".venv", "venv", "node_modules", "__pycache__",
    "build", "dist", "private", "secrets", "credentials", "log", "logs", "raw", "dumps",
    "purchase-evidence", "order-evidence",
}
FORBIDDEN_SUFFIXES = {".log", ".elf", ".bin", ".map", ".pyc", ".p12", ".pfx", ".pem", ".key"}
SECRET_PATTERNS = [
    ("private key", re.compile(rb"-----BEGIN (?:[A-Z ]+ )?PRIVATE KEY-----")),
    ("GitHub token", re.compile(rb"\b(?:gh[pousr]_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{20,})\b")),
    ("AWS access key", re.compile(rb"\b(?:AKIA|ASIA)[A-Z0-9]{16}\b")),
    ("credential in URL", re.compile(rb"https?://[^\s/:]+:[^\s/@]+@", re.I)),
    ("credential assignment", re.compile(
        rb"(?im)\b(?:wifi_password|password|passwd|api[_-]?key|access[_-]?token|client[_-]?secret)"
        rb"[\"']?\s*[:=]\s*[\"']?([^\s\"'#,;}]+)"
    )),
]
PLACEHOLDERS = {b"", b"none", b"null", b"false", b"true", b"changeme", b"example", b"placeholder",
                b"redacted", b"your_password", b"your_api_key", b"your_token", b"secret_here"}


class PublicationError(Exception):
    pass


def git(root: Path, *args: str, allow_failure: bool = False) -> subprocess.CompletedProcess:
    result = subprocess.run(["git", "-C", str(root), *args], stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, check=False)
    if result.returncode and not allow_failure:
        raise PublicationError("Git query failed; run this command in a readable Git working tree")
    return result


def repository_root(value: Optional[str]) -> Path:
    start = Path(value).absolute() if value else Path(__file__).resolve().parents[1]
    result = git(start, "rev-parse", "--show-toplevel")
    root = Path(os.fsdecode(result.stdout.rstrip(b"\n"))).resolve()
    if value and start.resolve() != root:
        raise PublicationError("--root must identify the Git working-tree root")
    return root


def tracked_files(root: Path) -> dict[str, str]:
    output = git(root, "ls-files", "--stage", "-z").stdout
    result: dict[str, str] = {}
    for entry in output.split(b"\0"):
        if not entry:
            continue
        header, raw_path = entry.split(b"\t", 1)
        mode, _oid, stage = header.decode("ascii").split()
        if stage != "0":
            raise PublicationError("resolve Git index conflicts before publication review")
        path = os.fsdecode(raw_path)
        if path in result:
            raise PublicationError("duplicate Git index entry; resolve conflicts before review")
        result[path] = mode
    return result


def safe_relative(value: Any) -> str:
    if not isinstance(value, str) or not value or "\\" in value or any(ord(c) < 32 for c in value):
        raise PublicationError("scope paths must be nonempty, plain repository-relative paths")
    path = PurePosixPath(value)
    if path.is_absolute() or value != path.as_posix() or any(part in {"..", ".", ""} for part in value.split("/")):
        raise PublicationError("scope paths must be normalized and may not contain traversal")
    if any(c in value for c in "*?[]"):
        raise PublicationError("scope paths must name exact files; glob patterns are forbidden")
    return value


def reject_private_path(value: str) -> None:
    parts = [part.lower() for part in PurePosixPath(value).parts]
    filename = parts[-1]
    if any(part in FORBIDDEN_PARTS or part.startswith((".env", "sdkconfig.old", "private-", "raw-"))
           for part in parts):
        raise PublicationError(f"{value}: private, raw, or generated paths cannot be published")
    if PurePosixPath(filename).suffix in FORBIDDEN_SUFFIXES or filename in {"cookies.json", "materials.db", "inventory.db"}:
        raise PublicationError(f"{value}: sensitive or generated artifact type cannot be published")
    if re.search(r"(?:^|[-_.])(?:credential|credentials|secret|secrets|cookie|cookies|token|tokens)(?:[-_.]|$)", filename):
        raise PublicationError(f"{value}: sensitive filename cannot be published")


def read_regular(root: Path, value: str, tracked: dict[str, str]) -> tuple[bytes, str]:
    value = safe_relative(value)
    reject_private_path(value)
    if value not in tracked:
        raise PublicationError(f"{value}: scope input is not tracked by Git")
    if tracked[value] not in {"100644", "100755"}:
        raise PublicationError(f"{value}: Git scope inputs must be regular files, never symlinks or submodules")
    if git(root, "check-ignore", "--no-index", "-q", "--", value, allow_failure=True).returncode == 0:
        raise PublicationError(f"{value}: ignored inputs cannot be published")
    parts = PurePosixPath(value).parts
    current = root
    for part in parts:
        current = current / part
        if current.is_symlink():
            raise PublicationError(f"{value}: symlinks, including parent directories, cannot be published")
    parent_fd: Optional[int] = None
    try:
        # Open every ancestor with openat/O_NOFOLLOW. Merely inspecting parents
        # before a normal open would allow a rename-to-symlink race to escape root.
        directory_flags = os.O_RDONLY | getattr(os, "O_DIRECTORY", 0) | getattr(os, "O_NOFOLLOW", 0)
        parent_fd = os.open(root, directory_flags)
        for part in parts[:-1]:
            child_fd = os.open(part, directory_flags, dir_fd=parent_fd)
            os.close(parent_fd)
            parent_fd = child_fd
        before = os.stat(parts[-1], dir_fd=parent_fd, follow_symlinks=False)
        if not stat.S_ISREG(before.st_mode):
            raise PublicationError(f"{value}: scope input must be a regular file")
        if before.st_size > MAX_FILE_BYTES:
            raise PublicationError(f"{value}: file exceeds the 10 MiB publication limit; keep raw media outside Git")
        fd = os.open(parts[-1], os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0), dir_fd=parent_fd)
        with os.fdopen(fd, "rb") as stream:
            opened = os.fstat(stream.fileno())
            content = stream.read(MAX_FILE_BYTES + 1)
            after = os.fstat(stream.fileno())
        final = os.stat(parts[-1], dir_fd=parent_fd, follow_symlinks=False)
        identity = lambda item: (item.st_dev, item.st_ino, item.st_size, item.st_mtime_ns, item.st_mode)
        if identity(before) != identity(opened) or identity(opened) != identity(after) or identity(after) != identity(final):
            raise PublicationError(f"{value}: file changed during capture; retry review")
        if len(content) > MAX_FILE_BYTES:
            raise PublicationError(f"{value}: file exceeds the 10 MiB publication limit")
    except OSError as exc:
        raise PublicationError(f"{value}: cannot capture regular file") from exc
    finally:
        if parent_fd is not None:
            os.close(parent_fd)
    mode = "100755" if before.st_mode & 0o111 else "100644"
    return content, mode


def load_metadata(content: bytes) -> dict[str, Any]:
    try:
        text = content.decode("utf-8")
        def unique_pairs(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
            result = {}
            for key, value in pairs:
                if key in result:
                    raise PublicationError("manifest contains duplicate mapping keys")
                result[key] = value
            return result
        try:
            data = json.loads(text, object_pairs_hook=unique_pairs)
        except json.JSONDecodeError:
            try:
                import yaml  # type: ignore[import-not-found]
            except ImportError as exc:
                raise PublicationError("normal YAML needs PyYAML; install it or use JSON-formatted YAML") from exc
            class UniqueLoader(yaml.SafeLoader):
                pass
            def mapping(loader: Any, node: Any, deep: bool = False) -> dict[str, Any]:
                return unique_pairs([(loader.construct_object(k, deep=deep), loader.construct_object(v, deep=deep))
                                     for k, v in node.value])
            UniqueLoader.add_constructor(yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG, mapping)
            data = yaml.load(text, Loader=UniqueLoader)
        if not isinstance(data, dict):
            raise PublicationError("manifest top level must be a mapping")
        return data
    except PublicationError:
        raise
    except Exception as exc:
        # Parser messages can quote secret-bearing input; deliberately do not echo them.
        raise PublicationError("manifest is not valid UTF-8 JSON or YAML") from exc


def discover(root: Path) -> list[str]:
    manifests = []
    for base in ("projects", "demos"):
        manifests.extend(path.relative_to(root).as_posix() for path in (root / base).glob("*/project.yaml"))
    manifests.extend(path.relative_to(root).as_posix() for path in (root / "publication").glob("*.yaml"))
    return sorted(set(manifests))


def manifest_path(root: Path, value: str) -> str:
    candidate = Path(value)
    if candidate.is_absolute():
        try:
            value = candidate.relative_to(root).as_posix()
        except ValueError as exc:
            raise PublicationError("manifest must belong to this repository") from exc
    value = safe_relative(value)
    if value not in discover(root):
        raise PublicationError("manifest must be projects/*/project.yaml, demos/*/project.yaml, or publication/*.yaml")
    return value


def validate_publication(data: dict[str, Any], manifest: str, target: str, review: bool) -> list[str]:
    publication = data.get("publication")
    if not isinstance(publication, dict):
        raise PublicationError(f"{manifest}: publication metadata is missing; denied by default")
    if not isinstance(data.get("status"), str) or data["status"] not in STATUSES:
        raise PublicationError(f"{manifest}: engineering status is missing or invalid")
    if not isinstance(publication.get("state"), str) or publication["state"] not in STATES:
        raise PublicationError(f"{manifest}: publication.state is missing or invalid")
    if not isinstance(publication.get("acceptance"), str) or publication["acceptance"] not in {"pending", "passed"}:
        raise PublicationError(f"{manifest}: publication.acceptance is missing or invalid")
    if not isinstance(publication.get("privacy"), str) or publication["privacy"] not in {"pending", "passed"}:
        raise PublicationError(f"{manifest}: publication.privacy is missing or invalid")
    if not isinstance(publication.get("rights"), str) or publication["rights"] not in {"unknown", "cleared"}:
        raise PublicationError(f"{manifest}: publication.rights is missing or invalid")
    targets = publication.get("targets")
    if not isinstance(targets, list) or any(not isinstance(item, str) or item not in TARGETS for item in targets) or len(targets) != len(set(targets)):
        raise PublicationError(f"{manifest}: publication.targets must list distinct git/platform targets")
    files = publication.get("files")
    if not isinstance(files, list):
        raise PublicationError(f"{manifest}: publication.files must be an exact file list")
    normalized = [safe_relative(value) for value in files]
    if len(normalized) != len(set(normalized)):
        raise PublicationError(f"{manifest}: publication.files contains duplicate paths")
    if data["status"] != "stable":
        raise PublicationError(f"{manifest}: work in progress cannot be published; status must be stable")
    if publication["acceptance"] != "passed" or publication["privacy"] != "passed" or publication["rights"] != "cleared":
        raise PublicationError(f"{manifest}: acceptance, privacy and rights reviews must all pass")
    if target not in targets:
        raise PublicationError(f"{manifest}: requested publication target is not explicitly allowed")
    if not normalized or manifest not in normalized:
        raise PublicationError(f"{manifest}: nonempty exact scope must include its own manifest")
    if publication["state"] not in ({"ready", "approved", "published"} if review else {"approved", "published"}):
        raise PublicationError(f"{manifest}: {'review needs ready' if review else 'publication needs approved or published'} state")
    approval = publication.get("approval")
    if approval is not None:
        if not isinstance(approval, dict) or set(approval) != {"sha256", "approved_by"}:
            raise PublicationError(f"{manifest}: approval must contain exactly sha256 and approved_by")
        if not isinstance(approval["sha256"], str) or not re.fullmatch(r"[a-f0-9]{64}", approval["sha256"]):
            raise PublicationError(f"{manifest}: approval.sha256 must be a lowercase SHA-256 digest")
        if not isinstance(approval["approved_by"], str) or not APPROVER_RE.fullmatch(approval["approved_by"]):
            raise PublicationError(f"{manifest}: approval.approved_by must be a 1–64 character ASCII role token (letters, numbers, _ or -)")
    return sorted(normalized)


def scan_secrets(path: str, content: bytes) -> None:
    for label, pattern in SECRET_PATTERNS:
        for match in pattern.finditer(content):
            if label == "credential assignment":
                candidate = match.group(1).lower()
                if candidate in PLACEHOLDERS or candidate.startswith((b"<", b"${", b"$", b"your_", b"example_")):
                    continue
            raise PublicationError(f"{path}: possible {label}; remove or redact before publication (value withheld)")


class LinkParser(HTMLParser):
    def __init__(self) -> None:
        super().__init__()
        self.links: list[str] = []
    def handle_starttag(self, tag: str, attrs: list[tuple[str, Optional[str]]]) -> None:
        self.links.extend(value for name, value in attrs if name.lower() in {"href", "src", "xlink:href"} and value)
        for name, value in attrs:
            if name.lower() == "srcset" and value:
                self.links.extend(candidate.strip().split()[0] for candidate in value.split(",") if candidate.strip())


def validate_links(path: str, content: bytes, files: set[str]) -> None:
    suffix = PurePosixPath(path).suffix.lower()
    if suffix not in {".md", ".markdown", ".html", ".htm", ".svg", ".css"}:
        return
    try:
        text = content.decode("utf-8")
    except UnicodeDecodeError as exc:
        raise PublicationError(f"{path}: public document must be UTF-8") from exc
    parser = LinkParser()
    parser.feed(text)
    links = parser.links
    # CSS dependencies occur in standalone stylesheets, HTML style tags and SVG.
    links += [match[1].strip() for match in re.findall(r"url\(\s*(['\"]?)(.*?)\1\s*\)", text, flags=re.I)]
    links += re.findall(r"@import\s+['\"]([^'\"]+)['\"]", text, flags=re.I)
    if suffix in {".md", ".markdown"}:
        # Ignore code examples, then collect inline and reference-style destinations.
        text = re.sub(r"(?ms)^\s*(`{3,}|~{3,}).*?^\s*\1\s*$", "", text)
        text = re.sub(r"`[^`\n]*`", "", text)
        links += re.findall(r"!?\[[^\]\n]*\]\(\s*(<[^>]+>|[^\s)]+)", text)
        links += re.findall(r"(?m)^\s{0,3}\[[^\]\n]+\]:\s*(<[^>]+>|[^\s]+)", text)
        links += re.findall(r"<(file:[^>]+)>", text, flags=re.I)
    for link in links:
        link = html.unescape(link.strip().strip("<>"))
        if not link or link.startswith("#"):
            continue
        try:
            parsed = urlsplit(link)
        except ValueError as exc:
            raise PublicationError(f"{path}: malformed document reference (value withheld)") from exc
        if parsed.scheme.lower() == "file":
            raise PublicationError(f"{path}: local file URL is forbidden in public documents")
        if parsed.scheme or parsed.netloc:
            continue
        destination = unquote(parsed.path)
        if not destination:
            continue
        if "\\" in destination or any(ord(c) < 32 for c in destination):
            raise PublicationError(f"{path}: local reference contains an unsafe path")
        if destination.startswith("/"):
            resolved = posixpath.normpath(destination.lstrip("/"))
        else:
            resolved = posixpath.normpath(posixpath.join(posixpath.dirname(path), destination))
        if resolved == ".." or resolved.startswith("../") or resolved.startswith("/"):
            raise PublicationError(f"{path}: local reference escapes the repository")
        choices = {resolved, posixpath.join(resolved, "README.md")}
        if not choices & files:
            raise PublicationError(f"{path}: local reference is outside the exact approved scope: {resolved}")


def canonical_metadata(data: dict[str, Any], include_mutable: bool = False) -> bytes:
    value = copy.deepcopy(data)
    if not include_mutable:
        value["publication"].pop("approval", None)
        value["publication"].pop("state", None)
    try:
        # PyYAML resolves ISO dates; preserve their semantic ISO form in the digest.
        import datetime
        def encode(obj: Any) -> str:
            if isinstance(obj, (datetime.date, datetime.datetime)):
                return obj.isoformat()
            raise TypeError("unsupported metadata value")
        return json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=False,
                          allow_nan=False, default=encode).encode("utf-8")
    except (TypeError, ValueError) as exc:
        raise PublicationError("manifest metadata must be canonical JSON-compatible values") from exc


def validate_owners(files: list[str], manifest: str, target: str,
                    captured: dict[str, tuple[bytes, str]]) -> None:
    """A generic publication unit cannot reclassify another unit's unfinished files."""
    allowed_by_owner: dict[str, set[str]] = {manifest: set(files)}
    scope = set(files)
    for path in files:
        parts = PurePosixPath(path).parts
        if len(parts) < 3 or parts[0] not in {"projects", "demos"}:
            continue
        owner = f"{parts[0]}/{parts[1]}/project.yaml"
        if owner not in scope:
            raise PublicationError(f"{path}: scope must include its owning project.yaml")
        if owner not in allowed_by_owner:
            content, _mode = captured[owner]
            owner_data = load_metadata(content)
            allowed_by_owner[owner] = set(validate_publication(owner_data, owner, target, review=True))
        if path not in allowed_by_owner[owner]:
            raise PublicationError(f"{path}: file is not allowed by its owning unit's exact publication.files")


def capture_unit(root: Path, manifest: str, target: str, tracked: dict[str, str], review: bool = False) -> tuple[dict[str, Any], dict[str, tuple[bytes, str]]]:
    manifest = manifest_path(root, manifest)
    manifest_bytes, manifest_mode = read_regular(root, manifest, tracked)
    data = load_metadata(manifest_bytes)
    files = validate_publication(data, manifest, target, review)
    # The published manifest is normalized JSON, which is also valid YAML.
    # Comments and formatting are outside the semantic digest and therefore must
    # never be copied into an approved export as unreviewed payload.
    normalized_manifest = canonical_metadata(data, include_mutable=True) + b"\n"
    if len(normalized_manifest) > MAX_FILE_BYTES:
        raise PublicationError(f"{manifest}: normalized manifest exceeds the 10 MiB limit")
    captured = {manifest: (normalized_manifest, manifest_mode)}
    for path in files:
        if path != manifest:
            captured[path] = read_regular(root, path, tracked)
    # Owner checks use the exact captured bytes later hashed/exported, never a
    # second read which could observe an earlier acceptance state.
    validate_owners(files, manifest, target, captured)
    digest = hashlib.sha256()
    digest.update(b"diy-electronics-publication-v1\0")
    digest.update(target.encode("ascii") + b"\0")
    for path in files:
        content, mode = captured[path]
        scan_secrets(path, content)
        validate_links(path, content, set(files))
        bound_content = canonical_metadata(data) if path == manifest else content
        header = json.dumps([path, mode, len(bound_content)], separators=(",", ":"), ensure_ascii=False).encode("utf-8")
        digest.update(len(header).to_bytes(8, "big") + header)
        digest.update(len(bound_content).to_bytes(8, "big") + bound_content)
    sha256 = digest.hexdigest()
    approval = data["publication"].get("approval")
    valid_approval = (isinstance(approval, dict) and isinstance(approval.get("approved_by"), str)
                      and bool(approval["approved_by"].strip()) and approval.get("sha256") == sha256)
    if not review and not valid_approval:
        raise PublicationError(f"{manifest}: approval is missing or stale; obtain human approval for the current review digest")
    report = {
        "manifest": manifest, "target": target, "sha256": sha256,
        "files": [{"path": path, "mode": captured[path][1], "size": len(captured[path][0])} for path in files],
        "eligible": True, "approved": bool(valid_approval and data["publication"]["state"] in {"approved", "published"}),
    }
    return report, captured


def status_report(root: Path, tracked: dict[str, str]) -> dict[str, Any]:
    units = []
    for manifest in discover(root):
        try:
            content, _mode = read_regular(root, manifest, tracked)
            data = load_metadata(content)
            publication = data.get("publication")
            units.append({"manifest": manifest, "status": data.get("status"),
                          "state": publication.get("state") if isinstance(publication, dict) else None,
                          "publication": publication, "configured": isinstance(publication, dict)})
        except PublicationError as exc:
            units.append({"manifest": manifest, "error": str(exc), "configured": False})
    return {"units": units, "tracked_files": len(tracked), "publication_checked": False}


def check_global(root: Path, target: str, tracked: dict[str, str]) -> dict[str, Any]:
    reports = []
    covered: set[str] = set()
    errors = []
    for manifest in discover(root):
        try:
            report, captured = capture_unit(root, manifest, target, tracked)
            covered.update(captured)
            reports.append(report)
        except PublicationError as exc:
            errors.append(str(exc))
    unscoped = sorted(set(tracked) - covered)
    if unscoped:
        errors.append(f"{len(unscoped)} tracked file(s) lack an approved scope; first: {unscoped[0]}")
    if errors or not reports:
        raise PublicationError("\n".join(errors or ["no approved publication units; denied by default"]))
    return {"target": target, "units": reports, "tracked_files": len(tracked)}


def rename_new_directory(source: Path, destination: Path) -> None:
    """Atomically rename without replacing even a concurrently created empty dir."""
    if os.name == "nt":
        os.rename(source, destination)  # Windows rename refuses existing targets.
        return
    library = ctypes.CDLL(None, use_errno=True)
    if sys.platform == "darwin" and hasattr(library, "renamex_np"):
        rename = library.renamex_np
        rename.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint]
        rename.restype = ctypes.c_int
        result = rename(os.fsencode(source), os.fsencode(destination), 0x00000004)  # RENAME_EXCL
    elif sys.platform.startswith("linux") and hasattr(library, "renameat2"):
        rename = library.renameat2
        rename.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_uint]
        rename.restype = ctypes.c_int
        result = rename(-100, os.fsencode(source), -100, os.fsencode(destination), 1)  # RENAME_NOREPLACE
    else:
        raise PublicationError("this operating system lacks atomic exclusive directory rename; export denied")
    if result:
        error = ctypes.get_errno()
        raise OSError(error, os.strerror(error))


def export_snapshot(root: Path, output: str, report: dict[str, Any], captured: dict[str, tuple[bytes, str]]) -> dict[str, Any]:
    requested = Path(output).absolute()
    if requested.is_symlink() or requested.exists():
        raise PublicationError("export output must be a new directory; existing paths are never overwritten")
    destination = requested.resolve()
    if destination == root or root in destination.parents:
        raise PublicationError("export output must be outside the development repository")
    parent = destination.parent
    if not parent.is_dir():
        raise PublicationError("export output parent directory must already exist")
    temp = Path(tempfile.mkdtemp(prefix=".publication-export-", dir=parent))
    try:
        for path, (content, mode) in captured.items():
            target = temp / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(content)
            target.chmod(0o755 if mode == "100755" else 0o644)
        rename_new_directory(temp, destination)
    except OSError as exc:
        raise PublicationError("cannot create snapshot; destination remains unpublished") from exc
    finally:
        if temp.exists():
            shutil.rmtree(temp)
    return dict(report, output=str(destination), exported=True)


def install_hook(root: Path) -> dict[str, Any]:
    configured = git(root, "config", "--get", "core.hooksPath", allow_failure=True).stdout.decode().strip()
    desired = root / ".githooks"
    if configured and (root / configured).resolve() != desired:
        raise PublicationError("existing core.hooksPath differs; integrate the guard with the existing hooks without overwriting them")
    hook = desired / "pre-push"
    if not hook.is_file() or hook.is_symlink() or not os.access(hook, os.X_OK):
        raise PublicationError("repository .githooks/pre-push must exist as an executable regular file")
    if hook.read_bytes() != EXPECTED_HOOK:
        raise PublicationError("existing pre-push hook differs; it will not be overwritten")
    common = Path(os.fsdecode(git(root, "rev-parse", "--git-common-dir").stdout.rstrip(b"\n")))
    if not common.is_absolute():
        common = root / common
    standard = common / "hooks"
    if standard.exists() and any(path.is_file() and not path.name.endswith(".sample") for path in standard.iterdir()):
        raise PublicationError("existing Git hook files would be bypassed; integrate the guard without replacing them")
    if any(path.name != "pre-push" and path.is_file() for path in desired.iterdir()):
        raise PublicationError("additional repository hooks exist; review integration before changing hooksPath")
    git(root, "config", "--local", "core.hooksPath", ".githooks")
    return {"installed": True, "hooks_path": ".githooks", "note": "Protects against mistakes; --no-verify can bypass Git hooks."}


def guard_push(root: Path, remote_name: str, remote_url: str) -> dict[str, Any]:
    names = git(root, "config", "--local", "--get-all", "publication.privateRemote", allow_failure=True).stdout.decode().splitlines()
    expected = git(root, "config", "--local", "--get", f"publication.privateRemoteUrl.{remote_name}", allow_failure=True)
    if remote_name in names and expected.returncode == 0 and os.fsdecode(expected.stdout.rstrip(b"\n")) == remote_url:
        return {"allowed": True, "private_backup": True,
                "note": "Local explicit private-backup name and URL match. Hooks protect against mistakes; --no-verify bypasses them."}
    raise PublicationError("direct push from the development repository is blocked: its Git history may contain drafts. "
                           "Publish an approved history-free export instead. A private backup requires an explicit local "
                           "publication.privateRemote name AND matching publication.privateRemoteUrl.NAME. "
                           "This hook protects against mistakes; --no-verify can bypass it.")


def parse_args(argv: Optional[list[str]]) -> argparse.Namespace:
    arguments = list(sys.argv[1:] if argv is None else argv)
    # Common options work before or after subcommands.
    common = argparse.ArgumentParser(add_help=False)
    common.add_argument("--root")
    common.add_argument("--json", action="store_true")
    preliminary, arguments = common.parse_known_args(arguments)
    parser = argparse.ArgumentParser(description=__doc__, parents=[common])
    parser.add_argument("--check", action="store_true", help="validate one manifest or all tracked publication scopes")
    parser.add_argument("--target", choices=sorted(TARGETS), default="git")
    parser.add_argument("--output")
    parser.add_argument("command", nargs="?", default="status")
    parser.add_argument("parameters", nargs="*")
    args = parser.parse_args(arguments)
    args.root = preliminary.root
    args.json = preliminary.json
    if args.check:
        if args.command == "status" and not args.parameters:
            args.manifest = None
        elif not args.parameters:
            args.manifest = args.command
        else:
            parser.error("--check accepts at most one manifest")
        args.command = "check"
    return args


def main(argv: Optional[list[str]] = None) -> int:
    args = parse_args(argv)
    try:
        root = repository_root(args.root)
        if args.command == "guard-push":
            if len(args.parameters) != 2:
                raise PublicationError("guard-push requires REMOTE_NAME REMOTE_URL")
            report = guard_push(root, *args.parameters)
        elif args.command == "install-hook":
            if args.parameters:
                raise PublicationError("install-hook takes no additional arguments")
            report = install_hook(root)
        else:
            tracked = tracked_files(root)
            if args.command == "status":
                if args.parameters:
                    raise PublicationError("status takes no additional arguments")
                report = status_report(root, tracked)
            elif args.command == "check":
                report = capture_unit(root, args.manifest, args.target, tracked)[0] if args.manifest else check_global(root, args.target, tracked)
            elif args.command in {"review", "export"}:
                if len(args.parameters) != 1:
                    raise PublicationError(f"{args.command} requires one manifest")
                report, captured = capture_unit(root, args.parameters[0], args.target, tracked, review=args.command == "review")
                if args.command == "export":
                    if not args.output:
                        raise PublicationError("export requires --output PATH")
                    report = export_snapshot(root, args.output, report, captured)
            else:
                raise PublicationError("unknown command; use status, --check, review, export, install-hook, or guard-push")
        if args.json:
            print(json.dumps(report, ensure_ascii=False, indent=2, default=str))
        elif args.command == "status":
            for unit in report["units"]:
                print(f"{unit['manifest']}: {unit.get('status', 'unknown')} / {unit.get('state') or 'unconfigured'}" +
                      (f"; denied: {unit['error']}" if "error" in unit else ""))
            print(f"{len(report['units'])} unit(s); publication requires explicit approval")
        elif "sha256" in report:
            print(f"{report['manifest']}\nsha256: {report['sha256']}\n{len(report['files'])} scoped file(s); target: {report['target']}")
            if args.command == "review":
                print("Review only; no approval was written. Obtain human approval for this digest and exact scope.")
                for item in report["files"]:
                    print(f"  {item['mode']} {item['size']:>9} {item['path']}")
            elif args.command == "export":
                print(f"Approved snapshot exported to {report['output']}; no Git history was copied.")
            else:
                print("Approval matches the captured contents.")
        else:
            print(json.dumps(report, ensure_ascii=False, indent=2))
        return 0
    except PublicationError as exc:
        if args.json:
            print(json.dumps({"ok": False, "errors": str(exc).splitlines()}, ensure_ascii=False, indent=2))
        else:
            print(f"publication denied: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
