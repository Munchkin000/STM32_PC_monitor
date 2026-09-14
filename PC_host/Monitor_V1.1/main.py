from __future__ import annotations

import os
import shutil
import sys
import tempfile
from pathlib import Path


def _prepare_frozen_tk_runtime() -> None:
    if not getattr(sys, "frozen", False):
        return
    bundle_root = Path(getattr(sys, "_MEIPASS"))
    candidates = [
        Path(os.environ.get("LOCALAPPDATA", ""))
        / "STM32PCMonitor"
        / "runtime"
        / "tcl-tk-8.6.15",
        Path(sys.executable).resolve().parent
        / ".stm32_pc_monitor_runtime"
        / "tcl-tk-8.6.15",
        Path(tempfile.gettempdir()) / "STM32PCMonitor" / "tcl-tk-8.6.15",
    ]
    for runtime_root in candidates:
        if not str(runtime_root):
            continue
        tcl_target = runtime_root / "tcl8.6"
        tk_target = runtime_root / "tk8.6"
        try:
            if not (tcl_target / "init.tcl").is_file():
                runtime_root.mkdir(parents=True, exist_ok=True)
                shutil.copytree(bundle_root / "_tcl_data", tcl_target, dirs_exist_ok=True)
            if not (tk_target / "tk.tcl").is_file():
                runtime_root.mkdir(parents=True, exist_ok=True)
                shutil.copytree(bundle_root / "_tk_data", tk_target, dirs_exist_ok=True)
        except OSError:
            continue
        os.environ["TCL_LIBRARY"] = str(tcl_target)
        os.environ["TK_LIBRARY"] = str(tk_target)
        return
    raise RuntimeError("无法准备 Tcl/Tk 运行环境")


_prepare_frozen_tk_runtime()

from pc_monitor.app import run

if __name__ == "__main__":
    run(started_by_windows="--startup" in sys.argv)
