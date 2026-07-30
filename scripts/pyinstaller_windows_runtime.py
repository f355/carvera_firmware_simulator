"""Restore standard streams omitted by PyInstaller's Windows GUI bootloader."""

import os
import sys
from pathlib import Path

if sys.stdin is None:
    sys.stdin = open(os.devnull, encoding="utf-8")

if sys.stdout is None or sys.stderr is None:
    # pywebview's spawned WinForms process can hang before creating WebView2
    # when these streams are backed by NUL rather than a regular file.
    log_dir = Path(os.environ["LOCALAPPDATA"]) / "Carvera Simulator"
    log_dir.mkdir(parents=True, exist_ok=True)
    log_mode = "a" if "--multiprocessing-fork" in sys.argv else "w"
    desktop_log = open(log_dir / "desktop-startup.log", log_mode, buffering=1, encoding="utf-8")
    if sys.stdout is None:
        sys.stdout = desktop_log
    if sys.stderr is None:
        sys.stderr = desktop_log
