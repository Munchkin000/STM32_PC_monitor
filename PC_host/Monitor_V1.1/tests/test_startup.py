import sys
import unittest
from pathlib import Path
from unittest.mock import patch

from pc_monitor.startup import startup_command


class StartupTests(unittest.TestCase):
    def test_frozen_command_targets_executable(self) -> None:
        with (
            patch.object(sys, "frozen", True, create=True),
            patch.object(sys, "executable", r"C:\Apps\STM32 PC Monitor.exe"),
        ):
            command = startup_command()
        self.assertIn("STM32 PC Monitor.exe", command)
        self.assertNotIn("main.py", command)
        self.assertIn("--startup", command)

    def test_source_command_targets_main_script(self) -> None:
        with patch.object(sys, "executable", str(Path(sys.executable))):
            command = startup_command()
        self.assertIn("main.py", command)
        self.assertIn("--startup", command)


if __name__ == "__main__":
    unittest.main()
