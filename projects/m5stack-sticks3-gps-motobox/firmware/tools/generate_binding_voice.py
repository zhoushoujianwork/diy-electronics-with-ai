#!/usr/bin/env python3
"""Generate 16 kHz mono PCM clips used by the StickS3 binding announcer.

This generator uses the macOS Mandarin Tingting voice. Generated clips are
checked in so firmware builds do not depend on macOS or a speech service.
"""

from pathlib import Path
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "main" / "assets"
CLIPS = {
    "binding_code": "绑定验证码",
    "digit_0": "零",
    "digit_1": "一",
    "digit_2": "二",
    "digit_3": "三",
    "digit_4": "四",
    "digit_5": "五",
    "digit_6": "六",
    "digit_7": "七",
    "digit_8": "八",
    "digit_9": "九",
}


def require(command: str) -> None:
    if shutil.which(command) is None:
        raise SystemExit(f"missing required command: {command}")


def main() -> None:
    require("say")
    require("ffmpeg")
    OUTPUT.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="sticks3-binding-voice-") as temp:
        temp_path = Path(temp)
        for name, text in CLIPS.items():
            source = temp_path / f"{name}.aiff"
            destination = OUTPUT / f"{name}.pcm"
            subprocess.run(
                ["say", "-v", "Tingting", "-r", "240", "-o", str(source), text],
                check=True,
            )
            subprocess.run(
                [
                    "ffmpeg", "-v", "error", "-y", "-i", str(source),
                    "-af",
                    "silenceremove=start_periods=1:start_duration=0.01:start_threshold=-45dB:"
                    "stop_periods=1:stop_duration=0.03:stop_threshold=-45dB,loudnorm=I=-18:TP=-2:LRA=7",
                    "-ar", "16000", "-ac", "1", "-f", "s16le", str(destination),
                ],
                check=True,
            )
            if destination.stat().st_size == 0 or destination.stat().st_size % 2:
                raise SystemExit(f"invalid generated PCM: {destination}")


if __name__ == "__main__":
    main()
