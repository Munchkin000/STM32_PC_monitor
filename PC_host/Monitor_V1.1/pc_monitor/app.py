from __future__ import annotations

import queue
import subprocess
import sys
import threading
import time
import tkinter as tk
from collections import deque
from datetime import datetime
from tkinter import messagebox, ttk

from .config import SettingsStore
from .metrics import MetricsCollector
from .models import PCMetrics
from .protocol import build_pc_frame, page_frame
from .resources import resource_path
from .serial_manager import PortInfo, SerialManager
from .startup import set_startup_enabled, startup_is_enabled


BG = "#0f172a"
PANEL = "#172033"
PANEL_2 = "#1e293b"
TEXT = "#e5edf8"
MUTED = "#8fa3bf"
ACCENT = "#38bdf8"
GREEN = "#34d399"
ORANGE = "#fbbf24"
RED = "#fb7185"
SEND_INTERVAL_MIN_MS = 500
SEND_INTERVAL_MAX_MS = 10000


def progress_fill_width(value: float, width: int) -> int:
    """Return a pixel width that preserves every non-zero 1% step."""
    if value <= 0 or width <= 0:
        return 0
    clamped = min(100.0, value)
    return max(1, round(width * clamped / 100.0))


class MetricCard(ttk.Frame):
    def __init__(self, master: tk.Misc, title: str, accent: str) -> None:
        super().__init__(master, style="Card.TFrame", padding=(18, 14))
        self.columnconfigure(0, weight=1)
        ttk.Label(self, text=title, style="CardTitle.TLabel").grid(row=0, column=0, sticky="w")
        self.value_var = tk.StringVar(value="--")
        ttk.Label(
            self, textvariable=self.value_var, style="Metric.TLabel", foreground=accent
        ).grid(row=1, column=0, sticky="w", pady=(4, 6))
        # ttk.Progressbar can round small values away on Windows themes.  Draw
        # the fill ourselves so every 1% step remains visible.
        self._bar_value = 0.0
        self._bar_color = accent
        self.bar = tk.Canvas(
            self,
            height=12,
            bg="#26344c",
            highlightthickness=0,
            borderwidth=0,
        )
        self.bar.grid(row=2, column=0, sticky="ew")
        self.bar.bind("<Configure>", self._redraw_bar)

    def _redraw_bar(self, _event: object = None) -> None:
        width = max(1, self.bar.winfo_width())
        height = max(1, self.bar.winfo_height())
        self.bar.delete("fill")
        if self._bar_value <= 0:
            return
        fill_width = progress_fill_width(self._bar_value, width)
        self.bar.create_rectangle(
            0,
            0,
            fill_width,
            height,
            fill=self._bar_color,
            outline="",
            tags="fill",
        )

    def update_value(self, value: float | None, suffix: str = "%") -> None:
        if value is None:
            self.value_var.set("不可用")
            self._bar_value = 0.0
            self._redraw_bar()
            return
        self.value_var.set(f"{value:.0f}{suffix}")
        self._bar_value = max(0.0, min(100.0, value))
        self._redraw_bar()


class ToggleCheck(tk.Frame):
    """Theme-independent check control with a real tick instead of a platform X."""

    def __init__(
        self,
        master: tk.Misc,
        text: str,
        variable: tk.BooleanVar,
        command: object | None = None,
    ) -> None:
        super().__init__(master, bg=PANEL_2, cursor="hand2", takefocus=True)
        self.variable = variable
        self.command = command
        self.box = tk.Canvas(
            self, width=18, height=18, bg=PANEL_2, highlightthickness=0, cursor="hand2"
        )
        self.box.pack(side="left")
        self.label = tk.Label(
            self,
            text=text,
            bg=PANEL_2,
            fg=TEXT,
            font=("Microsoft YaHei UI", 10),
            cursor="hand2",
        )
        self.label.pack(side="left", padx=(6, 0))
        for widget in (self, self.box, self.label):
            widget.bind("<Button-1>", self._toggle)
        self.bind("<space>", self._toggle)
        self.variable.trace_add("write", self._variable_changed)
        self._draw()

    def _toggle(self, _event: object = None) -> str:
        self.variable.set(not self.variable.get())
        if callable(self.command):
            self.command()
        return "break"

    def _variable_changed(self, *_args: object) -> None:
        self._draw()

    def _draw(self) -> None:
        self.box.delete("all")
        fill = "#0284c7" if self.variable.get() else "#111c2e"
        outline = "#38bdf8" if self.variable.get() else "#7890ad"
        self.box.create_rectangle(2, 2, 16, 16, fill=fill, outline=outline, width=1)
        if self.variable.get():
            self.box.create_line(5, 9, 8, 12, 14, 5, fill="white", width=2, capstyle="round")


class PCMonitorApp:
    def __init__(self, root: tk.Tk, *, started_by_windows: bool = False) -> None:
        self.root = root
        self._settings_store = SettingsStore()
        self._settings = self._settings_store.load()
        self.root.title("STM32 PC Monitor")
        try:
            self.root.geometry(self._settings.window_geometry)
        except tk.TclError:
            self.root.geometry("1040x720")
        self.root.minsize(900, 650)
        self.root.configure(bg=BG)
        self._configure_style()

        self._stop = threading.Event()
        self._events: queue.Queue[tuple[str, object]] = queue.Queue()
        self._commands: queue.Queue[tuple[str, object]] = queue.Queue()
        self._latest_metrics = PCMetrics.empty()
        self._metrics_lock = threading.Lock()
        self._known_ports: list[PortInfo] = []
        self._logs: deque[str] = deque(maxlen=300)
        self._tray_icon = None
        self._tray_thread: threading.Thread | None = None
        self._exiting = False

        self.connection_var = tk.StringVar(value="未连接")
        self.connection_detail_var = tk.StringVar(value="正在扫描 USB 设备…")
        self.port_var = tk.StringVar(value=self._settings.last_port)
        self.auto_connect_var = tk.BooleanVar(value=self._settings.auto_connect)
        self.send_enabled_var = tk.BooleanVar(value=self._settings.send_enabled)
        self.startup_var = tk.BooleanVar(value=self._settings.start_with_windows)
        self.background_var = tk.BooleanVar(value=self._settings.run_in_background)
        self.interval_var = tk.IntVar(value=self._settings.send_interval_ms)
        self.interval_text_var = tk.StringVar(value=f"{self._settings.send_interval_ms} ms")
        self.gpu_name_var = tk.StringVar(value="GPU 数据源：检测中")
        self.cpu_temp_source_var = tk.StringVar(value="CPU 温度来源：检测中")
        self.last_sample_var = tk.StringVar(value="等待首次采样")
        self.ack_var = tk.StringVar(value="尚未收到 MCU 应答")

        self._build_ui()
        self._create_tray_icon()
        self._sync_startup_state_on_launch()
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

        if started_by_windows and self.background_var.get():
            # Withdraw before the event loop maps the window, avoiding startup flash.
            self.root.withdraw()

        self._collector_thread = threading.Thread(
            target=self._metrics_loop, name="metrics", daemon=True
        )
        self._communication_thread = threading.Thread(
            target=self._communication_loop, name="usb-cdc", daemon=True
        )
        self._collector_thread.start()
        self._communication_thread.start()
        self._sync_settings(save=False)
        self.root.after(80, self._process_events)

    def _configure_style(self) -> None:
        style = ttk.Style(self.root)
        style.theme_use("clam")
        style.configure(".", background=BG, foreground=TEXT, font=("Microsoft YaHei UI", 10))
        style.configure("TFrame", background=BG)
        style.configure("Card.TFrame", background=PANEL)
        style.configure("Panel.TFrame", background=PANEL_2)
        style.configure("TLabel", background=BG, foreground=TEXT)
        style.configure("Title.TLabel", font=("Microsoft YaHei UI", 22, "bold"))
        style.configure("Subtitle.TLabel", foreground=MUTED)
        style.configure("CardTitle.TLabel", background=PANEL, foreground=MUTED)
        style.configure(
            "Metric.TLabel", background=PANEL, font=("Segoe UI", 27, "bold")
        )
        style.configure("Panel.TLabel", background=PANEL_2, foreground=TEXT)
        style.configure("PanelMuted.TLabel", background=PANEL_2, foreground=MUTED)
        style.configure(
            "TButton", background="#26354d", foreground=TEXT, padding=(12, 7), borderwidth=0
        )
        style.map("TButton", background=[("active", "#334663"), ("disabled", "#202b3e")])
        style.configure("Accent.TButton", background="#0284c7", foreground="white")
        style.map("Accent.TButton", background=[("active", "#0ea5e9")])
        style.configure(
            "Port.TCombobox",
            fieldbackground="#111c2e",
            background="#26354d",
            foreground=TEXT,
            arrowcolor="#f8fafc",
            bordercolor="#7890ad",
            lightcolor="#7890ad",
            darkcolor="#7890ad",
            arrowsize=18,
            padding=(5, 4),
        )
        style.map(
            "Port.TCombobox",
            fieldbackground=[("readonly", "#111c2e")],
            foreground=[("readonly", TEXT)],
            arrowcolor=[("readonly", "#f8fafc"), ("active", ACCENT)],
            background=[("readonly", "#26354d"), ("active", "#334663")],
        )
        style.configure(
            "Horizontal.TProgressbar", troughcolor="#26344c", background=ACCENT, bordercolor=PANEL
        )

    def _build_ui(self) -> None:
        outer = ttk.Frame(self.root, padding=22)
        outer.pack(fill="both", expand=True)
        outer.columnconfigure(0, weight=1)
        outer.rowconfigure(2, weight=1)

        header = ttk.Frame(outer)
        header.grid(row=0, column=0, sticky="ew")
        header.columnconfigure(0, weight=1)
        ttk.Label(header, text="STM32 PC Monitor", style="Title.TLabel").grid(
            row=0, column=0, sticky="w"
        )
        ttk.Label(
            header,
            text="实时采集主机状态 · USB CDC 自动握手 · MCU 协议发送",
            style="Subtitle.TLabel",
        ).grid(row=1, column=0, sticky="w", pady=(2, 0))
        self.status_badge = tk.Label(
            header,
            textvariable=self.connection_var,
            bg="#3f2434",
            fg=RED,
            padx=13,
            pady=7,
            font=("Microsoft YaHei UI", 10, "bold"),
        )
        self.status_badge.grid(row=0, column=1, rowspan=2, sticky="e")

        cards = ttk.Frame(outer)
        cards.grid(row=1, column=0, sticky="ew", pady=(20, 16))
        for column in range(6):
            cards.columnconfigure(column, weight=1, uniform="metric")
        definitions = [
            ("CPU 占用", ACCENT),
            ("CPU 温度", ORANGE),
            ("内存占用", GREEN),
            ("GPU 占用", ACCENT),
            ("GPU 温度", ORANGE),
            ("显存占用", GREEN),
        ]
        self.cards: list[MetricCard] = []
        for index, (title, color) in enumerate(definitions):
            card = MetricCard(cards, title, color)
            card.grid(row=0, column=index, padx=(0 if index == 0 else 5, 0), sticky="nsew")
            self.cards.append(card)

        body = ttk.Frame(outer)
        body.grid(row=2, column=0, sticky="nsew")
        body.columnconfigure(0, weight=5)
        body.columnconfigure(1, weight=4)
        body.rowconfigure(0, weight=1)

        left = ttk.Frame(body, style="Panel.TFrame", padding=18)
        left.grid(row=0, column=0, sticky="nsew", padx=(0, 8))
        left.columnconfigure(1, weight=1)
        left.rowconfigure(11, weight=1)
        ttk.Label(left, text="USB 连接与发送", style="Panel.TLabel", font=("Microsoft YaHei UI", 13, "bold")).grid(
            row=0, column=0, columnspan=3, sticky="w", pady=(0, 14)
        )
        ttk.Label(left, text="端口", style="PanelMuted.TLabel").grid(row=1, column=0, sticky="w")
        self.port_combo = ttk.Combobox(
            left, textvariable=self.port_var, state="readonly", style="Port.TCombobox"
        )
        self.port_combo.grid(row=1, column=1, sticky="ew", padx=10)
        ttk.Button(left, text="刷新", command=lambda: self._commands.put(("scan", None))).grid(
            row=1, column=2
        )
        ttk.Label(left, textvariable=self.connection_detail_var, style="PanelMuted.TLabel").grid(
            row=2, column=0, columnspan=3, sticky="w", pady=(7, 12)
        )
        buttons = ttk.Frame(left, style="Panel.TFrame")
        buttons.grid(row=3, column=0, columnspan=3, sticky="ew")
        self.connect_button = ttk.Button(
            buttons, text="连接选中设备", style="Accent.TButton", command=self._connect_selected
        )
        self.connect_button.pack(side="left")
        ttk.Button(buttons, text="断开", command=self._disconnect).pack(
            side="left", padx=8
        )
        self.auto_connect_check = ToggleCheck(
            left,
            text="自动扫描并连接",
            variable=self.auto_connect_var,
            command=self._sync_settings,
        )
        self.auto_connect_check.grid(
            row=4, column=0, columnspan=3, sticky="w", pady=(12, 0)
        )
        self.startup_check = ToggleCheck(
            left,
            text="开机自动启动上位机",
            variable=self.startup_var,
            command=self._startup_changed,
        )
        self.startup_check.grid(
            row=5, column=0, columnspan=3, sticky="w", pady=(10, 0)
        )
        self.background_check = ToggleCheck(
            left,
            text="关闭窗口后在后台继续运行",
            variable=self.background_var,
            command=self._background_changed,
        )
        self.background_check.grid(
            row=6, column=0, columnspan=3, sticky="w", pady=(10, 0)
        )

        ttk.Separator(left).grid(row=7, column=0, columnspan=3, sticky="ew", pady=14)
        ToggleCheck(
            left,
            text="持续向 MCU 发送监测数据",
            variable=self.send_enabled_var,
            command=self._sync_settings,
        ).grid(row=8, column=0, columnspan=3, sticky="w")
        ttk.Label(left, text="发送周期", style="PanelMuted.TLabel").grid(
            row=9, column=0, sticky="w", pady=(12, 0)
        )
        scale = ttk.Scale(
            left,
            from_=SEND_INTERVAL_MIN_MS,
            to=SEND_INTERVAL_MAX_MS,
            variable=self.interval_var,
            command=self._interval_changed,
        )
        scale.grid(row=9, column=1, sticky="ew", padx=10, pady=(12, 0))
        ttk.Label(left, textvariable=self.interval_text_var, style="Panel.TLabel", width=9).grid(
            row=9, column=2, sticky="e", pady=(12, 0)
        )
        page_buttons = ttk.Frame(left, style="Panel.TFrame")
        page_buttons.grid(row=10, column=0, columnspan=3, sticky="ew", pady=(16, 8))
        ttk.Label(page_buttons, text="MCU 页面：", style="PanelMuted.TLabel").grid(
            row=0, column=0, sticky="w"
        )
        for column in range(1, 4):
            page_buttons.columnconfigure(column, weight=1, uniform="mcu-page")
        for column, (label, page) in enumerate(
            (("CPU", "CPU"), ("GPU", "GPU"), ("下一页", "NEXT")), start=1
        ):
            ttk.Button(
                page_buttons,
                text=label,
                command=lambda p=page: self._send_page(p),
            ).grid(
                row=0,
                column=column,
                sticky="ew",
                padx=(8 if column == 1 else 4, 0),
            )
        right = ttk.Frame(body, style="Panel.TFrame", padding=18)
        right.grid(row=0, column=1, sticky="nsew", padx=(8, 0))
        right.columnconfigure(0, weight=1)
        ttk.Label(right, text="运行状态与通信日志", style="Panel.TLabel", font=("Microsoft YaHei UI", 13, "bold")).grid(
            row=0, column=0, sticky="w"
        )
        cpu_source_row = ttk.Frame(right, style="Panel.TFrame")
        cpu_source_row.grid(row=1, column=0, sticky="ew", pady=(10, 2))
        cpu_source_row.columnconfigure(0, weight=1)
        ttk.Label(
            cpu_source_row, textvariable=self.cpu_temp_source_var, style="PanelMuted.TLabel"
        ).grid(row=0, column=0, sticky="w")
        self.install_temp_button = ttk.Button(
            cpu_source_row, text="安装 CPU 温度驱动", command=self._install_temperature_driver
        )
        self.install_temp_button.grid(row=0, column=1, sticky="e", padx=(8, 0))
        ttk.Label(right, textvariable=self.gpu_name_var, style="PanelMuted.TLabel").grid(
            row=2, column=0, sticky="w", pady=(0, 2)
        )
        ttk.Label(right, textvariable=self.last_sample_var, style="PanelMuted.TLabel").grid(
            row=3, column=0, sticky="w", pady=(0, 10)
        )
        log_frame = ttk.Frame(right, style="Panel.TFrame")
        log_frame.grid(row=4, column=0, sticky="nsew")
        right.rowconfigure(4, weight=1)
        log_frame.columnconfigure(0, weight=1)
        log_frame.rowconfigure(0, weight=1)
        self.log_text = tk.Text(
            log_frame,
            bg="#0d1728",
            fg="#b9c7da",
            relief="flat",
            wrap="word",
            state="disabled",
            font=("Cascadia Mono", 9),
            padx=10,
            pady=8,
        )
        scroll = ttk.Scrollbar(log_frame, orient="vertical", command=self.log_text.yview)
        self.log_text.configure(yscrollcommand=scroll.set)
        self.log_text.grid(row=0, column=0, sticky="nsew")
        scroll.grid(row=0, column=1, sticky="ns")
        ttk.Label(right, textvariable=self.ack_var, style="PanelMuted.TLabel").grid(
            row=5, column=0, sticky="w", pady=(10, 0)
        )

    def _metrics_loop(self) -> None:
        try:
            collector = MetricsCollector()
            self._events.put(("gpu_source", collector.gpu_source))
            while not self._stop.is_set():
                started = time.monotonic()
                try:
                    metrics = collector.collect()
                    with self._metrics_lock:
                        self._latest_metrics = metrics
                    self._events.put(("metrics", metrics))
                except Exception as exc:
                    self._events.put(("error", f"采集系统指标失败：{exc}"))
                self._stop.wait(max(0.05, 0.5 - (time.monotonic() - started)))
        finally:
            if "collector" in locals():
                collector.close()

    def _communication_loop(self) -> None:
        manager = SerialManager(lambda kind, value: self._events.put((kind, value)))
        if not manager.dependency_available:
            self._events.put(("error", "未安装 pyserial，USB 功能暂不可用"))
        auto_connect = self.auto_connect_var.get()
        send_enabled = self.send_enabled_var.get()
        interval = self.interval_var.get() / 1000.0
        next_send = time.monotonic()
        next_scan = 0.0
        while not self._stop.is_set():
            while True:
                try:
                    command, value = self._commands.get_nowait()
                except queue.Empty:
                    break
                if command == "settings":
                    settings = value if isinstance(value, dict) else {}
                    auto_connect = bool(settings.get("auto_connect", auto_connect))
                    send_enabled = bool(settings.get("send_enabled", send_enabled))
                    interval = max(
                        SEND_INTERVAL_MIN_MS / 1000.0,
                        min(
                            SEND_INTERVAL_MAX_MS / 1000.0,
                            float(settings.get("interval", interval)),
                        ),
                    )
                elif command == "scan":
                    next_scan = 0.0
                elif command == "connect" and isinstance(value, str):
                    manager.connect(value, verify=True)
                elif command == "disconnect":
                    manager.disconnect(manual=True)
                elif command == "page" and isinstance(value, str):
                    manager.write(page_frame(value))

            now = time.monotonic()
            if now >= next_scan:
                ports = manager.scan_ports()
                self._events.put(("ports", ports))
                next_scan = now + (2.0 if not manager.connected else 5.0)
                if auto_connect and not manager.connected and ports:
                    manager.auto_connect(ports)

            for line in manager.read_lines():
                self._events.put(("rx", line))

            if manager.connected and send_enabled and now >= next_send:
                with self._metrics_lock:
                    metrics = self._latest_metrics
                manager.write(build_pc_frame(metrics))
                next_send = now + interval
            elif now >= next_send:
                next_send = now + interval
            self._stop.wait(0.04)
        manager.disconnect(manual=False)

    def _process_events(self) -> None:
        try:
            while True:
                kind, payload = self._events.get_nowait()
                if kind == "metrics" and isinstance(payload, PCMetrics):
                    self._show_metrics(payload)
                elif kind == "gpu_source":
                    self.gpu_name_var.set(f"GPU 数据源：{payload}")
                elif kind == "ports" and isinstance(payload, list):
                    self._show_ports(payload)
                elif kind == "connected":
                    self._set_connected(str(payload))
                    self._append_log("INFO", f"已通过 $PING 握手连接 {payload}")
                elif kind == "disconnected":
                    self._set_disconnected()
                    self._append_log("INFO", f"设备 {payload or ''} 已断开")
                elif kind == "tx":
                    self._append_log("TX", str(payload))
                elif kind == "rx":
                    line = str(payload)
                    self._append_log("RX", line)
                    self.ack_var.set(f"最近应答：{line}")
                elif kind == "error":
                    self._append_log("ERR", str(payload))
                    self.connection_detail_var.set(str(payload))
                elif kind == "debug":
                    # Failed probes are intentionally quiet in the visible log.
                    pass
        except queue.Empty:
            pass
        if not self._stop.is_set():
            self.root.after(80, self._process_events)

    def _show_metrics(self, metrics: PCMetrics) -> None:
        values = (
            (metrics.cpu_usage, "%"),
            (metrics.cpu_temp, " °C"),
            (metrics.ram_usage, "%"),
            (metrics.gpu_usage, "%"),
            (metrics.gpu_temp, " °C"),
            (metrics.vram_usage, "%"),
        )
        for card, (value, suffix) in zip(self.cards, values):
            card.update_value(value, suffix)
        gpu = metrics.gpu_name or "未检测到可用 GPU 采集接口"
        if metrics.cpu_temp is None:
            status = metrics.cpu_temp_status or "未发现 CPU 温度传感器"
            self.cpu_temp_source_var.set(f"CPU 温度：不可用（{status}）")
            self.install_temp_button.grid()
        else:
            source = metrics.cpu_temp_source or "系统传感器"
            self.cpu_temp_source_var.set(f"CPU 温度来源：{source}")
            self.install_temp_button.grid_remove()
        self.gpu_name_var.set(f"GPU：{gpu}")
        self.last_sample_var.set(
            f"最近采样：{datetime.fromtimestamp(metrics.sampled_at).strftime('%H:%M:%S')}"
        )
    def _show_ports(self, ports: list[PortInfo]) -> None:
        self._known_ports = ports
        labels = [port.label for port in ports]
        current_device = self.port_var.get().split(" — ", 1)[0]
        self.port_combo["values"] = labels
        devices = {port.device for port in ports}
        if labels and current_device in devices:
            matching = next(port.label for port in ports if port.device == current_device)
            self.port_var.set(matching)
        elif labels:
            self.port_var.set(labels[0])
        elif not labels:
            self.port_var.set("")
        if self.connection_var.get() == "未连接":
            self.connection_detail_var.set(
                f"发现 {len(ports)} 个串口设备，正在等待 MCU 握手"
                if ports
                else "未发现串口设备；将每 2 秒自动扫描"
            )

    def _set_connected(self, port: str) -> None:
        self.connection_var.set("已连接")
        self.connection_detail_var.set(f"MCU 已连接：{port}")
        self.status_badge.configure(bg="#143d35", fg=GREEN)

    def _set_disconnected(self) -> None:
        self.connection_var.set("未连接")
        self.connection_detail_var.set("设备未连接；自动扫描将继续运行")
        self.status_badge.configure(bg="#3f2434", fg=RED)

    def _append_log(self, level: str, message: str) -> None:
        stamp = datetime.now().strftime("%H:%M:%S")
        line = f"[{stamp}] {level:<4} {message}"
        self._logs.append(line)
        self.log_text.configure(state="normal")
        self.log_text.delete("1.0", "end")
        self.log_text.insert("1.0", "\n".join(self._logs))
        self.log_text.see("end")
        self.log_text.configure(state="disabled")

    def _connect_selected(self) -> None:
        device = self.port_var.get().split(" — ", 1)[0]
        if not device:
            messagebox.showinfo("没有可用端口", "请连接设备并点击“刷新”。")
            return
        self.connection_detail_var.set(f"正在与 {device} 握手…")
        self._settings.last_port = device
        self._save_settings()
        self._commands.put(("connect", device))

    def _disconnect(self) -> None:
        # A manual disconnect must not be immediately undone by the scan loop.
        self.auto_connect_var.set(False)
        self._sync_settings()
        self._commands.put(("disconnect", None))

    def _interval_changed(self, raw: str) -> None:
        value = int(round(float(raw) / 100.0) * 100)
        value = max(SEND_INTERVAL_MIN_MS, min(SEND_INTERVAL_MAX_MS, value))
        self.interval_var.set(value)
        self.interval_text_var.set(f"{value} ms")
        self._sync_settings()

    def _sync_settings(self, *, save: bool = True) -> None:
        self._settings.auto_connect = self.auto_connect_var.get()
        self._settings.send_enabled = self.send_enabled_var.get()
        self._settings.send_interval_ms = self.interval_var.get()
        if save:
            self._save_settings()
        self._commands.put(
            (
                "settings",
                {
                    "auto_connect": self.auto_connect_var.get(),
                    "send_enabled": self.send_enabled_var.get(),
                    "interval": self.interval_var.get() / 1000.0,
                },
            )
        )

    def _send_page(self, page: str) -> None:
        self._commands.put(("page", page))

    def _install_temperature_driver(self) -> None:
        installer = resource_path("tools", "PawnIO_setup.exe")
        if not installer.is_file():
            messagebox.showerror("安装器缺失", f"未找到：{installer}")
            return
        confirmed = messagebox.askyesno(
            "安装 CPU 温度驱动",
            "CPU 温度需要 PawnIO 提供安全的底层硬件访问。\n\n"
            "安装器来自 PawnIO 官方 GitHub，并具有有效代码签名。"
            "Windows 将弹出管理员权限确认。安装后可能需要重启电脑。\n\n"
            "是否现在安装？",
        )
        if not confirmed:
            return
        try:
            subprocess.Popen([str(installer)], cwd=str(installer.parent))
            self._append_log("INFO", "已启动 PawnIO 温度驱动安装器")
            messagebox.showinfo(
                "安装已启动",
                "请完成 PawnIO 安装。安装结束后重新启动本上位机；如果安装器提示重启，请先重启电脑。",
            )
        except OSError as exc:
            messagebox.showerror("无法启动安装器", str(exc))

    def _on_close(self) -> None:
        self._capture_window_settings()
        self._save_settings()
        if self.background_var.get():
            self._hide_to_tray()
            return
        self._quit_application()

    def _capture_window_settings(self) -> None:
        if self.root.state() != "withdrawn":
            self._settings.window_geometry = self.root.geometry()
        selected_port = self.port_var.get().split(" — ", 1)[0]
        if selected_port:
            self._settings.last_port = selected_port

    def _quit_application(self) -> None:
        if self._exiting:
            return
        self._exiting = True
        self._capture_window_settings()
        self._save_settings()
        self._stop.set()
        if self._tray_icon is not None:
            try:
                self._tray_icon.stop()
            except Exception:
                pass
        self.root.after(150, self.root.destroy)

    def _hide_to_tray(self) -> None:
        self._capture_window_settings()
        self._save_settings()
        self.root.withdraw()

    def _show_from_tray(self) -> None:
        if self._exiting:
            return
        self.root.deiconify()
        self.root.lift()
        self.root.focus_force()

    def _create_tray_icon(self) -> None:
        try:
            import pystray
            from PIL import Image, ImageDraw
        except ImportError:
            self._append_log("ERR", "缺少 pystray/Pillow，后台托盘功能不可用")
            self.background_var.set(False)
            self._settings.run_in_background = False
            return

        image = Image.new("RGBA", (64, 64), BG)
        draw = ImageDraw.Draw(image)
        draw.rounded_rectangle((5, 5, 59, 59), radius=11, fill="#172033", outline=ACCENT, width=3)
        draw.line((17, 43, 26, 20, 34, 42, 45, 17), fill=GREEN, width=6, joint="curve")

        def show_window(_icon: object = None, _item: object = None) -> None:
            self.root.after(0, self._show_from_tray)

        def exit_application(_icon: object = None, _item: object = None) -> None:
            self.root.after(0, self._quit_application)

        menu = pystray.Menu(
            pystray.MenuItem("显示串口界面", show_window, default=True),
            pystray.Menu.SEPARATOR,
            pystray.MenuItem("退出程序", exit_application),
        )
        self._tray_icon = pystray.Icon(
            "STM32PCMonitor", image, "STM32 PC Monitor", menu
        )
        self._tray_thread = threading.Thread(
            target=self._tray_icon.run, name="system-tray", daemon=True
        )
        self._tray_thread.start()

    def _background_changed(self) -> None:
        self._settings.run_in_background = self.background_var.get()
        self._save_settings()

    def _save_settings(self) -> None:
        try:
            self._settings_store.save(self._settings)
        except OSError as exc:
            if hasattr(self, "log_text"):
                self._append_log("ERR", f"保存设置失败：{exc}")

    def _sync_startup_state_on_launch(self) -> None:
        """Keep saved state and the Windows Run entry aligned after EXE moves."""
        try:
            registry_enabled = startup_is_enabled()
            if self._settings.start_with_windows:
                set_startup_enabled(True)
                registry_enabled = True
            self.startup_var.set(registry_enabled)
            self._settings.start_with_windows = registry_enabled
            self._save_settings()
        except OSError:
            self.startup_var.set(False)
            self._settings.start_with_windows = False

    def _startup_changed(self) -> None:
        enabled = self.startup_var.get()
        try:
            set_startup_enabled(enabled)
        except OSError as exc:
            self.startup_var.set(not enabled)
            messagebox.showerror("开机启动设置失败", str(exc))
            return
        self._settings.start_with_windows = enabled
        self._save_settings()


def run(*, started_by_windows: bool = False) -> None:
    root = tk.Tk()
    PCMonitorApp(root, started_by_windows=started_by_windows)
    root.mainloop()
