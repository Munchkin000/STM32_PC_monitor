import unittest

from pc_monitor.app import (
    SEND_INTERVAL_MAX_MS,
    SEND_INTERVAL_MIN_MS,
    progress_fill_width,
)


class UISettingsTests(unittest.TestCase):
    def test_send_interval_limits(self) -> None:
        self.assertEqual(SEND_INTERVAL_MIN_MS, 500)
        self.assertEqual(SEND_INTERVAL_MAX_MS, 10000)

    def test_progress_bar_preserves_low_percentages(self) -> None:
        self.assertEqual(progress_fill_width(0, 180), 0)
        self.assertEqual(progress_fill_width(1, 180), 2)
        self.assertEqual(progress_fill_width(5, 180), 9)
        self.assertEqual(progress_fill_width(1, 50), 1)
        self.assertEqual(progress_fill_width(100, 180), 180)


if __name__ == "__main__":
    unittest.main()
