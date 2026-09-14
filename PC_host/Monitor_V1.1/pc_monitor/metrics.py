from __future__ import annotations

import csv
import io
import json
import os
import subprocess
import urllib.request
from dataclasses import replace
from time import monotonic, time
from typing import Protocol

import psutil

from .models import PCMetrics
from .resources import resource_path


class GPUReader(Protocol):
    name: str

    def read(self) -> tuple[float, float, float, str]: ...

    def close(self) -> None: ...


class NVMLReader:
    name = "NVML"

    def __init__(self) -> None:
        import pynvml

        self._nvml = pynvml
        pynvml.nvmlInit()
        self._handle = pynvml.nvmlDeviceGetHandleByIndex(0)

    def read(self) -> tuple[float, float, float, str]:
        utilization = self._nvml.nvmlDeviceGetUtilizationRates(self._handle)
        memory = self._nvml.nvmlDeviceGetMemoryInfo(self._handle)
        temperature = self._nvml.nvmlDeviceGetTemperature(
            self._handle, self._nvml.NVML_TEMPERATURE_GPU
        )
        raw_name = self._nvml.nvmlDeviceGetName(self._handle)
        name = raw_name.decode(errors="replace") if isinstance(raw_name, bytes) else str(raw_name)
        vram = 100.0 * memory.used / memory.total if memory.total else 0.0
        return float(utilization.gpu), float(temperature), vram, name

    def close(self) -> None:
        try:
            self._nvml.nvmlShutdown()
        except Exception:
            pass


class NvidiaSmiReader:
    name = "nvidia-smi"

    def read(self) -> tuple[float, float, float, str]:
        command = [
            "nvidia-smi",
            "--query-gpu=utilization.gpu,temperature.gpu,memory.used,memory.total,name",
            "--format=csv,noheader,nounits",
            "--id=0",
        ]
        result = subprocess.run(
            command,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=2,
            check=True,
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
        )
        row = next(csv.reader(io.StringIO(result.stdout.strip())))
        gpu, temperature, used, total = (float(item.strip()) for item in row[:4])
        name = ",".join(row[4:]).strip()
        vram = 100.0 * used / total if total else 0.0
        return gpu, temperature, vram, name

    def close(self) -> None:
        return


class MetricsCollector:
    """Collect system metrics without making the UI depend on vendor tooling."""

    def __init__(self) -> None:
        psutil.cpu_percent(interval=None)
        self._gpu_reader = self._make_gpu_reader()
        self._gpu_cache: tuple[float, float, float, str] | None = None
        self._gpu_read_at = 0.0
        self._cpu_temp_cache: float | None = None
        self._cpu_temp_source: str | None = None
        self._cpu_temp_status: str | None = None
        self._cpu_temp_read_at = 0.0

    @property
    def gpu_source(self) -> str:
        return self._gpu_reader.name if self._gpu_reader else "不可用"

    @staticmethod
    def _make_gpu_reader() -> GPUReader | None:
        try:
            return NVMLReader()
        except Exception:
            pass
        try:
            reader = NvidiaSmiReader()
            reader.read()
            return reader
        except Exception:
            return None

    @staticmethod
    def _valid_temperature(value: object) -> float | None:
        try:
            temperature = float(value)
        except (TypeError, ValueError):
            return None
        return temperature if -40 <= temperature <= 150 else None

    @classmethod
    def _psutil_cpu_temperature(cls) -> tuple[float | None, str | None]:
        try:
            temperatures = psutil.sensors_temperatures()
        except (AttributeError, OSError):
            return None, None
        candidates: list[float] = []
        preferred: list[float] = []
        for group, entries in temperatures.items():
            for entry in entries:
                current = cls._valid_temperature(getattr(entry, "current", None))
                if current is None:
                    continue
                candidates.append(current)
                label = f"{group} {getattr(entry, 'label', '')}".lower()
                if any(key in label for key in ("cpu", "core", "package", "tctl", "tdie")):
                    preferred.append(current)
        values = preferred or candidates
        return (max(values), "psutil") if values else (None, None)

    @classmethod
    def _bundled_lhm_temperature(cls) -> tuple[float | None, str | None]:
        if os.name != "nt":
            return None, None
        reader = resource_path("tools", "CpuTemperatureReader.exe")
        if not reader.is_file():
            return None, None
        try:
            result = subprocess.run(
                [str(reader)],
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
                timeout=4.0,
                check=False,
                creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
            )
        except (OSError, subprocess.SubprocessError):
            return None, None
        if result.returncode != 0 or not result.stdout.strip():
            return None, "需要安装 PawnIO 温度驱动"
        value_text, _, sensor_name = result.stdout.strip().partition("|")
        value = cls._valid_temperature(value_text)
        if value is None:
            return None, None
        source = f"LibreHardwareMonitor / {sensor_name or 'CPU'}"
        return value, source

    @staticmethod
    def _powershell_json(command: str) -> list[dict[str, object]]:
        flags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
        result = subprocess.run(
            [
                "powershell.exe",
                "-NoLogo",
                "-NoProfile",
                "-NonInteractive",
                "-ExecutionPolicy",
                "Bypass",
                "-Command",
                command,
            ],
            capture_output=True,
            text=True,
            encoding="utf-8-sig",
            errors="replace",
            timeout=2.5,
            check=False,
            creationflags=flags,
        )
        if result.returncode or not result.stdout.strip():
            return []
        try:
            parsed = json.loads(result.stdout)
        except json.JSONDecodeError:
            return []
        if isinstance(parsed, dict):
            return [parsed]
        return parsed if isinstance(parsed, list) else []

    @classmethod
    def _hardware_monitor_wmi_temperature(cls) -> tuple[float | None, str | None]:
        if os.name != "nt":
            return None, None
        for namespace, source in (
            (r"root\LibreHardwareMonitor", "LibreHardwareMonitor"),
            (r"root\OpenHardwareMonitor", "OpenHardwareMonitor"),
        ):
            # Parent/Identifier is used to exclude GPU, disk and motherboard sensors.
            command = (
                f"Get-CimInstance -Namespace '{namespace}' -ClassName Sensor "
                "-ErrorAction SilentlyContinue | "
                "Where-Object { $_.SensorType -eq 'Temperature' } | "
                "Select-Object Name,Value,Parent,Identifier | ConvertTo-Json -Compress"
            )
            rows = cls._powershell_json(command)
            preferred: list[float] = []
            cpu_values: list[float] = []
            for row in rows:
                name = str(row.get("Name", ""))
                identity = " ".join(
                    str(row.get(key, "")) for key in ("Name", "Parent", "Identifier")
                ).lower()
                if not any(key in identity for key in ("cpu", "package", "core", "tctl", "tdie")):
                    continue
                value = cls._valid_temperature(row.get("Value"))
                if value is None:
                    continue
                cpu_values.append(value)
                lowered_name = name.lower()
                if any(key in lowered_name for key in ("cpu package", "cpu (tctl/tdie)", "package")):
                    preferred.append(value)
            values = preferred or cpu_values
            if values:
                return max(values), source
        return None, None

    @classmethod
    def _walk_lhm_nodes(cls, node: object) -> list[tuple[str, float]]:
        found: list[tuple[str, float]] = []
        if isinstance(node, list):
            for child in node:
                found.extend(cls._walk_lhm_nodes(child))
            return found
        if not isinstance(node, dict):
            return found
        text = " ".join(str(node.get(key, "")) for key in ("Text", "Name", "SensorId", "id"))
        raw_value = node.get("Value")
        if isinstance(raw_value, str):
            raw_value = raw_value.replace("°C", "").replace("°", "").strip().split(" ")[0]
        value = cls._valid_temperature(raw_value)
        lowered = text.lower()
        if value is not None and any(
            key in lowered for key in ("cpu package", "cpu core", "tctl", "tdie", "/cpu/")
        ):
            found.append((lowered, value))
        for child_key in ("Children", "children"):
            if child_key in node:
                found.extend(cls._walk_lhm_nodes(node[child_key]))
        return found

    @classmethod
    def _lhm_web_temperature(cls) -> tuple[float | None, str | None]:
        # LibreHardwareMonitor exposes this endpoint when its remote web server is enabled.
        try:
            with urllib.request.urlopen("http://127.0.0.1:8085/data.json", timeout=0.35) as response:
                data = json.load(response)
        except Exception:
            return None, None
        values = cls._walk_lhm_nodes(data)
        if not values:
            return None, None
        preferred = [value for name, value in values if "package" in name or "tctl" in name]
        return max(preferred or [value for _, value in values]), "LibreHardwareMonitor Web"

    @classmethod
    def _acpi_temperature(cls) -> tuple[float | None, str | None]:
        if os.name != "nt":
            return None, None
        command = (
            "Get-CimInstance -Namespace 'root\\wmi' -ClassName MSAcpi_ThermalZoneTemperature "
            "-ErrorAction SilentlyContinue | Select-Object CurrentTemperature | ConvertTo-Json -Compress"
        )
        values: list[float] = []
        for row in cls._powershell_json(command):
            raw = row.get("CurrentTemperature")
            try:
                celsius = (float(raw) / 10.0) - 273.15
            except (TypeError, ValueError):
                continue
            valid = cls._valid_temperature(celsius)
            if valid is not None:
                values.append(valid)
        return (max(values), "Windows ACPI") if values else (None, None)

    def _cpu_temperature(self) -> tuple[float | None, str | None]:
        now = monotonic()
        retry_after = 2.0 if self._cpu_temp_cache is not None else 10.0
        if now - self._cpu_temp_read_at < retry_after:
            return self._cpu_temp_cache, self._cpu_temp_source
        readers = (
            self._psutil_cpu_temperature,
            self._bundled_lhm_temperature,
            self._hardware_monitor_wmi_temperature,
            self._lhm_web_temperature,
            self._acpi_temperature,
        )
        temperature: float | None = None
        source: str | None = None
        diagnostic: str | None = None
        for reader in readers:
            temperature, source = reader()
            if temperature is not None:
                break
            if source:
                diagnostic = source
        self._cpu_temp_cache = temperature
        self._cpu_temp_source = source or diagnostic
        self._cpu_temp_status = None if temperature is not None else (source or diagnostic)
        self._cpu_temp_read_at = now
        return self._cpu_temp_cache, self._cpu_temp_source

    def collect(self) -> PCMetrics:
        cpu_temp, cpu_temp_source = self._cpu_temperature()
        metrics = PCMetrics(
            cpu_usage=psutil.cpu_percent(interval=None),
            cpu_temp=cpu_temp,
            ram_usage=psutil.virtual_memory().percent,
            cpu_temp_source=cpu_temp_source,
            cpu_temp_status=self._cpu_temp_status,
            sampled_at=time(),
        )
        if self._gpu_reader is None:
            return metrics

        # nvidia-smi process startup is relatively expensive, so cache briefly.
        now = monotonic()
        if self._gpu_cache is None or now - self._gpu_read_at >= 0.8:
            try:
                self._gpu_cache = self._gpu_reader.read()
                self._gpu_read_at = now
            except Exception:
                self._gpu_cache = None
        if self._gpu_cache is None:
            return metrics
        gpu, gpu_temp, vram, gpu_name = self._gpu_cache
        return replace(
            metrics,
            gpu_usage=gpu,
            gpu_temp=gpu_temp,
            vram_usage=vram,
            gpu_name=gpu_name,
        )

    def close(self) -> None:
        if self._gpu_reader:
            self._gpu_reader.close()
