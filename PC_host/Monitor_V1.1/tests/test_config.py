import json
import tempfile
import unittest
from pathlib import Path

from pc_monitor.config import AppSettings, SettingsStore


class ConfigTests(unittest.TestCase):
    def test_round_trip_all_user_settings(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "settings.json"
            store = SettingsStore(path)
            expected = AppSettings(
                auto_connect=False,
                send_enabled=False,
                send_interval_ms=750,
                start_with_windows=True,
                run_in_background=True,
                last_port="COM20",
                window_geometry="1100x760+12+24",
            )
            store.save(expected)
            self.assertEqual(store.load(), expected)
            self.assertFalse(path.with_suffix(".tmp").exists())

    def test_invalid_values_are_normalized(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "settings.json"
            path.write_text(
                json.dumps({"send_interval_ms": 100, "window_geometry": "bad"}),
                encoding="utf-8",
            )
            settings = SettingsStore(path).load()
            self.assertEqual(settings.send_interval_ms, 500)
            self.assertEqual(settings.window_geometry, "1040x720")

    def test_corrupt_file_uses_defaults(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "settings.json"
            path.write_text("not json", encoding="utf-8")
            self.assertEqual(SettingsStore(path).load(), AppSettings())

    def test_windows_utf8_bom_is_accepted(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "settings.json"
            path.write_text(
                json.dumps({"send_interval_ms": 750}), encoding="utf-8-sig"
            )
            self.assertEqual(SettingsStore(path).load().send_interval_ms, 750)


if __name__ == "__main__":
    unittest.main()
