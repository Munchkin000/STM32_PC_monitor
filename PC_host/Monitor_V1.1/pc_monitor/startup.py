from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path


RUN_KEY = r"Software\Microsoft\Windows\CurrentVersion\Run"
VALUE_NAME = "STM32PCMonitor"


def startup_command() -> str:
    if getattr(sys, "frozen", False):
        arguments = [str(Path(sys.executable).resolve()), "--startup"]
    else:
        python = Path(sys.executable)
        pythonw = python.with_name("pythonw.exe")
        executable = pythonw if pythonw.exists() else python
        script = Path(__file__).resolve().parents[1] / "main.py"
        arguments = [str(executable), str(script), "--startup"]
    return subprocess.list2cmdline(arguments)


def set_startup_enabled(enabled: bool) -> None:
    if os.name != "nt":
        if enabled:
            raise OSError("开机自动启动仅支持 Windows")
        return
    import winreg

    with winreg.CreateKeyEx(
        winreg.HKEY_CURRENT_USER, RUN_KEY, 0, winreg.KEY_SET_VALUE
    ) as key:
        if enabled:
            winreg.SetValueEx(key, VALUE_NAME, 0, winreg.REG_SZ, startup_command())
        else:
            try:
                winreg.DeleteValue(key, VALUE_NAME)
            except FileNotFoundError:
                pass


def startup_is_enabled() -> bool:
    if os.name != "nt":
        return False
    import winreg

    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, RUN_KEY) as key:
            value, _kind = winreg.QueryValueEx(key, VALUE_NAME)
    except FileNotFoundError:
        return False
    return bool(value)
