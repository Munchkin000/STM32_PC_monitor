from __future__ import annotations

import json
import os
from dataclasses import asdict, dataclass
from pathlib import Path


APP_DIR_NAME = "STM32PCMonitor"
CONFIG_FILE_NAME = "settings.json"
MIN_INTERVAL_MS = 500
MAX_INTERVAL_MS = 10000


def config_directory() -> Path:
    root = os.environ.get("LOCALAPPDATA")
    if root:
        return Path(root) / APP_DIR_NAME
    return Path.home() / ".stm32_pc_monitor"


@dataclass(slots=True)
class AppSettings:
    auto_connect: bool = True
    send_enabled: bool = True
    send_interval_ms: int = 1000
    start_with_windows: bool = False
    run_in_background: bool = False
    last_port: str = ""
    window_geometry: str = "1040x720"

    def normalize(self) -> None:
        self.auto_connect = bool(self.auto_connect)
        self.send_enabled = bool(self.send_enabled)
        self.start_with_windows = bool(self.start_with_windows)
        self.run_in_background = bool(self.run_in_background)
        try:
            interval = int(self.send_interval_ms)
        except (TypeError, ValueError):
            interval = 1000
        self.send_interval_ms = max(MIN_INTERVAL_MS, min(MAX_INTERVAL_MS, interval))
        self.last_port = str(self.last_port or "")
        geometry = str(self.window_geometry or "1040x720")
        self.window_geometry = geometry if "x" in geometry else "1040x720"


class SettingsStore:
    def __init__(self, path: Path | None = None) -> None:
        self.path = path or (config_directory() / CONFIG_FILE_NAME)

    def load(self) -> AppSettings:
        try:
            # utf-8-sig accepts both normal UTF-8 and files edited by Windows
            # tools that prepend a BOM.
            raw = json.loads(self.path.read_text(encoding="utf-8-sig"))
        except (OSError, json.JSONDecodeError):
            return AppSettings()
        if not isinstance(raw, dict):
            return AppSettings()
        defaults = AppSettings()
        allowed = asdict(defaults)
        values = {key: raw.get(key, default) for key, default in allowed.items()}
        settings = AppSettings(**values)
        settings.normalize()
        return settings

    def save(self, settings: AppSettings) -> None:
        settings.normalize()
        self.path.parent.mkdir(parents=True, exist_ok=True)
        temporary = self.path.with_suffix(".tmp")
        temporary.write_text(
            json.dumps(asdict(settings), ensure_ascii=False, indent=2),
            encoding="utf-8",
        )
        os.replace(temporary, self.path)
