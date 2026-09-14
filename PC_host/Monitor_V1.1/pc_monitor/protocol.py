from __future__ import annotations

from .models import PCMetrics


def _percent(value: float | None) -> int:
    if value is None:
        return 0
    return max(0, min(100, round(value)))


def _temperature(value: float | None) -> int:
    if value is None:
        return 0
    return max(-40, min(150, round(value)))


def build_pc_frame(metrics: PCMetrics) -> bytes:
    """Build the exact CRLF-terminated ASCII frame consumed by the MCU."""
    text = (
        f"$PC,CPU={_percent(metrics.cpu_usage)},"
        f"CT={_temperature(metrics.cpu_temp)},"
        f"RAM={_percent(metrics.ram_usage)},"
        f"GPU={_percent(metrics.gpu_usage)},"
        f"GT={_temperature(metrics.gpu_temp)},"
        f"VRAM={_percent(metrics.vram_usage)}\r\n"
    )
    payload = text.encode("ascii")
    if len(payload.rstrip(b"\r\n")) >= 80:
        raise ValueError("Protocol frame exceeds MCU line buffer")
    return payload


def ping_frame() -> bytes:
    return b"$PING\r\n"


def page_frame(page: str) -> bytes:
    page = page.upper()
    if page not in {"NEXT", "CPU", "GPU"}:
        raise ValueError(f"Unsupported page: {page}")
    return f"$PAGE,{page}\r\n".encode("ascii")


def is_pong(line: str) -> bool:
    return line.strip() == "$ACK,PONG"
