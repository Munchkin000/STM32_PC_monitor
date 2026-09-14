from __future__ import annotations

from dataclasses import dataclass
from time import time


@dataclass(frozen=True, slots=True)
class PCMetrics:
    cpu_usage: float = 0.0
    cpu_temp: float | None = None
    ram_usage: float = 0.0
    gpu_usage: float | None = None
    gpu_temp: float | None = None
    vram_usage: float | None = None
    gpu_name: str | None = None
    cpu_temp_source: str | None = None
    cpu_temp_status: str | None = None
    sampled_at: float = 0.0

    @classmethod
    def empty(cls) -> "PCMetrics":
        return cls(sampled_at=time())
