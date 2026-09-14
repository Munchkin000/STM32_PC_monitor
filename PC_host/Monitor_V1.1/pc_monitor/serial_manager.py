from __future__ import annotations

import threading
import time
from dataclasses import dataclass
from typing import Callable

from .protocol import is_pong, ping_frame

try:
    import serial
    from serial.tools import list_ports
except ImportError:  # Lets protocol/tests run before pyserial is installed.
    serial = None
    list_ports = None


@dataclass(frozen=True, slots=True)
class PortInfo:
    device: str
    description: str
    hwid: str
    vid: int | None
    pid: int | None

    @property
    def label(self) -> str:
        return f"{self.device} — {self.description or '串口设备'}"


class SerialManager:
    def __init__(self, event_callback: Callable[[str, str], None]) -> None:
        self._event_callback = event_callback
        self._serial = None
        self._lock = threading.RLock()
        self._rx_buffer = bytearray()
        self._manual_disconnect = False
        self._connected_port: str | None = None

    @property
    def dependency_available(self) -> bool:
        return serial is not None

    @property
    def connected(self) -> bool:
        with self._lock:
            return bool(self._serial and self._serial.is_open)

    @property
    def connected_port(self) -> str | None:
        return self._connected_port

    @staticmethod
    def scan_ports() -> list[PortInfo]:
        if list_ports is None:
            return []
        ports = []
        for port in list_ports.comports():
            ports.append(
                PortInfo(
                    device=port.device,
                    description=port.description or "",
                    hwid=port.hwid or "",
                    vid=port.vid,
                    pid=port.pid,
                )
            )
        return sorted(ports, key=lambda item: item.device)

    def connect(self, device: str, *, verify: bool = True) -> bool:
        if serial is None:
            self._event_callback("error", "缺少 pyserial，请先安装 requirements.txt")
            return False
        self.disconnect(manual=False)
        try:
            candidate = serial.Serial(
                port=device,
                baudrate=115200,  # CDC ignores baud rate, but pyserial requires one.
                timeout=0.08,
                write_timeout=0.3,
            )
            candidate.reset_input_buffer()
            if verify:
                candidate.write(ping_frame())
                candidate.flush()
                deadline = time.monotonic() + 0.65
                buffer = bytearray()
                accepted = False
                while time.monotonic() < deadline:
                    chunk = candidate.read(candidate.in_waiting or 1)
                    if not chunk:
                        continue
                    buffer.extend(chunk)
                    while b"\n" in buffer:
                        raw, _, rest = buffer.partition(b"\n")
                        buffer = bytearray(rest)
                        line = raw.rstrip(b"\r").decode("ascii", errors="replace")
                        if is_pong(line):
                            accepted = True
                            break
                    if accepted:
                        break
                if not accepted:
                    candidate.close()
                    return False
            with self._lock:
                self._serial = candidate
                self._connected_port = device
                self._rx_buffer.clear()
                self._manual_disconnect = False
            self._event_callback("connected", device)
            return True
        except (OSError, serial.SerialException) as exc:
            self._event_callback("debug", f"{device} 打开失败：{exc}")
            return False

    def auto_connect(self, ports: list[PortInfo] | None = None) -> bool:
        ports = ports if ports is not None else self.scan_ports()
        # USB CDC ports first; devices with no VID/PID are tried afterwards.
        ordered = sorted(ports, key=lambda port: (port.vid is None, port.device))
        for port in ordered:
            if self.connect(port.device, verify=True):
                return True
        return False

    def disconnect(self, *, manual: bool = True) -> None:
        with self._lock:
            active = self._serial
            port = self._connected_port
            self._serial = None
            self._connected_port = None
            self._manual_disconnect = manual
            self._rx_buffer.clear()
        if active:
            try:
                active.close()
            except Exception:
                pass
            self._event_callback("disconnected", port or "")

    def write(self, payload: bytes) -> bool:
        with self._lock:
            active = self._serial
            if not active or not active.is_open:
                return False
            try:
                active.write(payload)
                active.flush()
            except (OSError, serial.SerialException) as exc:
                self._event_callback("error", f"串口发送失败：{exc}")
                self.disconnect(manual=False)
                return False
        self._event_callback("tx", payload.decode("ascii", errors="replace").strip())
        return True

    def read_lines(self) -> list[str]:
        with self._lock:
            active = self._serial
            if not active or not active.is_open:
                return []
            try:
                waiting = active.in_waiting
                chunk = active.read(waiting) if waiting else b""
            except (OSError, serial.SerialException) as exc:
                self._event_callback("error", f"串口接收失败：{exc}")
                self.disconnect(manual=False)
                return []
            self._rx_buffer.extend(chunk)
            lines: list[str] = []
            while b"\n" in self._rx_buffer:
                raw, _, rest = self._rx_buffer.partition(b"\n")
                self._rx_buffer = bytearray(rest)
                line = raw.rstrip(b"\r").decode("ascii", errors="replace")
                if line:
                    lines.append(line)
            return lines
