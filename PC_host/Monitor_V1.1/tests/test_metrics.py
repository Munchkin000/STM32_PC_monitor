import unittest
from unittest.mock import patch

from pc_monitor.metrics import MetricsCollector


class MetricsTests(unittest.TestCase):
    def test_temperature_source_diagnostic_is_preserved(self) -> None:
        collector = MetricsCollector.__new__(MetricsCollector)
        collector._cpu_temp_cache = None
        collector._cpu_temp_source = None
        collector._cpu_temp_read_at = -100.0
        unavailable = (None, "LibreHardwareMonitor（需要管理员权限或硬件不支持）")
        with (
            patch.object(collector, "_psutil_cpu_temperature", return_value=(None, None)),
            patch.object(collector, "_bundled_lhm_temperature", return_value=unavailable),
            patch.object(collector, "_hardware_monitor_wmi_temperature", return_value=(None, None)),
            patch.object(collector, "_lhm_web_temperature", return_value=(None, None)),
            patch.object(collector, "_acpi_temperature", return_value=(None, None)),
        ):
            self.assertEqual(collector._cpu_temperature(), unavailable)

    def test_first_available_temperature_wins(self) -> None:
        collector = MetricsCollector.__new__(MetricsCollector)
        collector._cpu_temp_cache = None
        collector._cpu_temp_source = None
        collector._cpu_temp_read_at = -100.0
        with (
            patch.object(collector, "_psutil_cpu_temperature", return_value=(None, None)),
            patch.object(collector, "_bundled_lhm_temperature", return_value=(63.5, "LHM")),
        ):
            self.assertEqual(collector._cpu_temperature(), (63.5, "LHM"))


if __name__ == "__main__":
    unittest.main()
