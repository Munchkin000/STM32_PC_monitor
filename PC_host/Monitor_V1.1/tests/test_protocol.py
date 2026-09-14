import unittest

from pc_monitor.models import PCMetrics
from pc_monitor.protocol import build_pc_frame, is_pong, page_frame, ping_frame


class ProtocolTests(unittest.TestCase):
    def test_pc_frame_matches_mcu_protocol(self) -> None:
        metrics = PCMetrics(
            cpu_usage=68.4,
            cpu_temp=52.1,
            ram_usage=54.0,
            gpu_usage=72.0,
            gpu_temp=61.0,
            vram_usage=45.0,
        )
        self.assertEqual(
            build_pc_frame(metrics),
            b"$PC,CPU=68,CT=52,RAM=54,GPU=72,GT=61,VRAM=45\r\n",
        )

    def test_missing_values_are_safe_for_mcu(self) -> None:
        frame = build_pc_frame(PCMetrics(cpu_usage=120, cpu_temp=None, ram_usage=-5))
        self.assertEqual(
            frame,
            b"$PC,CPU=100,CT=0,RAM=0,GPU=0,GT=0,VRAM=0\r\n",
        )

    def test_temperature_and_percent_are_clamped(self) -> None:
        frame = build_pc_frame(
            PCMetrics(cpu_temp=-100, gpu_temp=300, gpu_usage=-1, vram_usage=101)
        )
        self.assertIn(b"CT=-40", frame)
        self.assertIn(b"GPU=0", frame)
        self.assertIn(b"GT=150", frame)
        self.assertIn(b"VRAM=100", frame)

    def test_control_frames(self) -> None:
        self.assertEqual(ping_frame(), b"$PING\r\n")
        self.assertTrue(is_pong("$ACK,PONG\r\n"))
        self.assertEqual(page_frame("gpu"), b"$PAGE,GPU\r\n")
        with self.assertRaises(ValueError):
            page_frame("OTHER")


if __name__ == "__main__":
    unittest.main()
