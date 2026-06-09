#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
硅链控制板 - 上位机监控软件
基于 Modbus RTU (RS485) 通信协议
依赖: Python 3.6+, pyserial, minimalmodbus
"""

import tkinter as tk
from tkinter import ttk, messagebox, scrolledtext
import threading
import time
import datetime
import os
import sys

# ==================== 依赖检查 ====================
try:
    import serial.tools.list_ports
except ImportError:
    print("错误: 请先安装 pyserial")
    print("  pip install pyserial")
    sys.exit(1)

try:
    import minimalmodbus
except ImportError:
    print("错误: 请先安装 minimalmodbus")
    print("  pip install minimalmodbus")
    sys.exit(1)


# ==================== 常量定义 ====================

# 寄存器地址
REG_VERSION       = 0
REG_SLAVE_ID      = 1
REG_BAUDRATE_IDX  = 2
REG_PARAM_OP      = 3
REG_FILTER_TIME   = 4
REG_INPUT_STATUS  = 11
REG_OUTPUT_STATUS = 12
REG_AI0           = 15
REG_AI1           = 16
REG_HM_VOLT       = 20
REG_KM_VOLT       = 21
REG_GEAR          = 22
REG_ALARM         = 23
REG_TARGET_V      = 24
REG_STEP_V        = 25
REG_DEADBAND_U    = 26
REG_DEADBAND_L    = 27
REG_CTRL_MODE     = 28
REG_MAX_DROP      = 29
REG_PARITY        = 30  # 校验模式: 0=无校验(8N2), 1=偶校验(8E1), 2=奇校验(8O1)

# 参数操作码
OP_SAVE    = 10
OP_RELOAD  = 20
OP_REBOOT  = 30
OP_FACTORY = 66

# 操作名称
OP_NAMES = {OP_SAVE: "保存参数", OP_RELOAD: "重载参数",
            OP_REBOOT: "重启设备", OP_FACTORY: "恢复出厂"}

# 波特率映射
BAUD_FROM_REG = {0: 115200, 1: 9600, 2: 19200, 3: 38400}
BAUD_NAMES    = {0: "115200", 1: "9600", 2: "19200", 3: "38400"}

# 可选波特率列表
BAUD_RATE_LIST = [9600, 19200, 38400, 57600, 115200]

# 校验位选项
PARITY_OPTIONS = ["奇校验 (8O1)", "偶校验 (8E1)", "无校验 (8N2)"]
PARITY_VALUES  = {"奇校验 (8O1)": "O", "偶校验 (8E1)": "E", "无校验 (8N2)": "N"}

# 输入/输出通道名称
INPUT_NAMES  = [f"X{i}" for i in range(8)]
OUTPUT_NAMES = [f"Y{i}" for i in range(6)]

# 档位对应降压值说明（220V系统）
GEAR_DROP = {0: 35, 1: 30, 2: 25, 3: 20, 4: 15, 5: 10, 6: 5, 7: 0}

# 颜色
C_ON         = "#00CC66"   # 高电平 / 导通
C_OFF        = "#BBBBBB"   # 低电平 / 断开
C_ALARM      = "#FF4444"   # 报警
C_OK         = "#44AA66"   # 正常
C_CONNECTED  = "#00CC66"
C_DISCONNECT = "#FF4444"
C_BG         = "#EEF2F7"
C_FRAME_BG   = "#FFFFFF"
C_TEXT       = "#2C3E50"
C_ACCENT     = "#3B7DD8"


# ==================== 主程序 ====================

class SiliconChainMonitor:
    """硅链控制板 — 上位机监控"""

    # ---------- 初始化 ----------

    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("硅链控制板 - 上位机监控")
        self.root.geometry("900x780")
        self.root.minsize(800, 700)
        self.root.configure(bg=C_BG)

        # 通信对象
        self.instrument = None
        self.connected = False
        self._lock = threading.Lock()
        self._poll_event = threading.Event()
        self._poll_thread = None
        self._error_count = 0

        # 数据缓存
        self.data = {}

        # 自动刷新
        self.auto_refresh = tk.BooleanVar(value=True)

        # 创建界面
        self._setup_styles()
        self._create_widgets()
        self._scan_ports()

        # 窗口关闭
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

    # ---------- 样式 ----------

    def _setup_styles(self):
        style = ttk.Style()
        style.theme_use("clam")

        style.configure("Title.TLabel", font=("Microsoft YaHei", 11, "bold"),
                        foreground=C_ACCENT, background=C_FRAME_BG)
        style.configure("Section.TLabelframe", background=C_FRAME_BG, relief="solid", borderwidth=1)
        style.configure("Section.TLabelframe.Label", font=("Microsoft YaHei", 10, "bold"),
                        foreground=C_TEXT, background=C_FRAME_BG)
        style.configure("Status.TLabel", font=("Microsoft YaHei", 9),
                        background=C_FRAME_BG, foreground=C_TEXT)
        style.configure("Connected.TLabel", font=("Microsoft YaHei", 9, "bold"),
                        foreground=C_CONNECTED, background=C_FRAME_BG)
        style.configure("Disconnected.TLabel", font=("Microsoft YaHei", 9, "bold"),
                        foreground=C_DISCONNECT, background=C_FRAME_BG)
        style.configure("Alarm.TLabel", font=("Microsoft YaHei", 11, "bold"),
                        foreground=C_ALARM, background=C_FRAME_BG)
        style.configure("Normal.TLabel", font=("Microsoft YaHei", 11, "bold"),
                        foreground=C_OK, background=C_FRAME_BG)
        style.configure("Value.TLabel", font=("Consolas", 14, "bold"),
                        foreground="#1a5276", background=C_FRAME_BG)
        style.configure("Small.TButton", font=("Microsoft YaHei", 8), padding=2)
        style.configure("Action.TButton", font=("Microsoft YaHei", 9), padding=4)
        style.configure("Connect.TButton", font=("Microsoft YaHei", 9, "bold"), padding=6)

    # ---------- 界面构建 ----------

    def _create_widgets(self):
        # 顶部：通信设置
        self._build_connection_frame()
        # 中部左侧：设备信息 + IO状态
        self._build_left_panel()
        # 中部右侧：监测数据 + 参数设置
        self._build_right_panel()
        # 底部：日志
        self._build_log_frame()
        # 状态栏
        self._build_status_bar()

    def _build_connection_frame(self):
        """一、通信设置"""
        frame = ttk.LabelFrame(self.root, text="  通信设置  ", style="Section.TLabelframe")
        frame.pack(fill=tk.X, padx=8, pady=(8, 4), ipady=4)

        # 第1行
        row1 = tk.Frame(frame, bg=C_FRAME_BG)
        row1.pack(fill=tk.X, padx=10, pady=4)

        tk.Label(row1, text="串口:", bg=C_FRAME_BG, font=("Microsoft YaHei", 9)).pack(side=tk.LEFT)
        self.combo_port = ttk.Combobox(row1, width=12, state="readonly")
        self.combo_port.pack(side=tk.LEFT, padx=(4, 4))

        self.btn_scan = ttk.Button(row1, text="刷新", style="Small.TButton",
                                   command=self._scan_ports, width=4)
        self.btn_scan.pack(side=tk.LEFT, padx=(0, 16))

        tk.Label(row1, text="波特率:", bg=C_FRAME_BG, font=("Microsoft YaHei", 9)).pack(side=tk.LEFT)
        self.combo_baud = ttk.Combobox(row1, width=10, state="readonly")
        self.combo_baud['values'] = [str(b) for b in BAUD_RATE_LIST]
        self.combo_baud.current(BAUD_RATE_LIST.index(115200))
        self.combo_baud.pack(side=tk.LEFT, padx=(4, 12))

        tk.Label(row1, text="校验位:", bg=C_FRAME_BG, font=("Microsoft YaHei", 9)).pack(side=tk.LEFT)
        self.combo_parity = ttk.Combobox(row1, width=13, state="readonly")
        self.combo_parity['values'] = PARITY_OPTIONS
        self.combo_parity.current(0)  # 默认奇校验
        self.combo_parity.pack(side=tk.LEFT, padx=(4, 16))

        tk.Label(row1, text="从站ID:", bg=C_FRAME_BG, font=("Microsoft YaHei", 9)).pack(side=tk.LEFT)
        self.spin_slave = ttk.Spinbox(row1, from_=1, to=247, width=5)
        self.spin_slave.set("1")
        self.spin_slave.pack(side=tk.LEFT, padx=(4, 20))

        self.btn_connect = ttk.Button(row1, text="连接设备", style="Connect.TButton",
                                      command=self._connect, width=10)
        self.btn_connect.pack(side=tk.LEFT, padx=4)

        self.btn_disconnect = ttk.Button(row1, text="断开连接", style="Action.TButton",
                                         command=self._disconnect, width=10)
        self.btn_disconnect.pack(side=tk.LEFT, padx=4)
        self.btn_disconnect.configure(state=tk.DISABLED)

        self.lbl_conn_status = tk.Label(row1, text="  未连接  ", bg=C_FRAME_BG,
                                        font=("Microsoft YaHei", 9, "bold"), fg=C_DISCONNECT)
        self.lbl_conn_status.pack(side=tk.LEFT, padx=16)

    def _build_left_panel(self):
        """中部左侧面板"""
        left = tk.Frame(self.root, bg=C_BG)
        left.pack(side=tk.LEFT, fill=tk.BOTH, expand=False, padx=(8, 4), pady=4)

        self._build_device_info(left)
        self._build_io_status(left)

    def _build_right_panel(self):
        """中部右侧面板"""
        right = tk.Frame(self.root, bg=C_BG)
        right.pack(side=tk.RIGHT, fill=tk.BOTH, expand=True, padx=(4, 8), pady=4)

        self._build_monitor(right)
        self._build_params(right)

    def _build_device_info(self, parent):
        """二、设备信息"""
        frame = ttk.LabelFrame(parent, text="  设备信息  ", style="Section.TLabelframe")
        frame.pack(fill=tk.X, pady=(0, 4))

        inner = tk.Frame(frame, bg=C_FRAME_BG)
        inner.pack(fill=tk.X, padx=10, pady=6)

        info_items = [
            ("从站ID:", "dev_slave_id", ""),
            ("波特率:", "dev_baudrate", ""),
            ("滤波时间:", "dev_filter", " ms"),
        ]

        for i, (label, key, suffix) in enumerate(info_items):
            row = tk.Frame(inner, bg=C_FRAME_BG)
            row.pack(fill=tk.X, pady=1)
            tk.Label(row, text=label, width=10, anchor=tk.W, bg=C_FRAME_BG,
                     font=("Microsoft YaHei", 9)).pack(side=tk.LEFT)
            lbl = tk.Label(row, text="--", width=14, anchor=tk.W, bg="white",
                           relief=tk.SUNKEN, font=("Consolas", 10), fg=C_TEXT)
            lbl.pack(side=tk.LEFT, padx=2)
            setattr(self, f"lbl_{key}", lbl)
            if suffix:
                tk.Label(row, text=suffix, bg=C_FRAME_BG,
                         font=("Microsoft YaHei", 9)).pack(side=tk.LEFT)

        # 控制模式 (可切换)
        mode_row = tk.Frame(inner, bg=C_FRAME_BG)
        mode_row.pack(fill=tk.X, pady=3)
        tk.Label(mode_row, text="控制模式:", width=10, anchor=tk.W, bg=C_FRAME_BG,
                 font=("Microsoft YaHei", 9)).pack(side=tk.LEFT)
        self.combo_mode = ttk.Combobox(mode_row, values=["自动调压", "手动控制"],
                                        state="readonly", width=12)
        self.combo_mode.pack(side=tk.LEFT, padx=2)
        self.combo_mode.bind("<<ComboboxSelected>>", self._on_mode_change)
        self.lbl_dev_mode = tk.Label(mode_row, text="(X0硬件决定)", bg=C_FRAME_BG,
                                      font=("Microsoft YaHei", 8), fg="#999")
        self.lbl_dev_mode.pack(side=tk.LEFT, padx=6)

        # 操作按钮
        btn_frame = tk.Frame(inner, bg=C_FRAME_BG)
        btn_frame.pack(fill=tk.X, pady=(8, 2))

        ops = [
            ("保存参数", OP_SAVE),
            ("重载参数", OP_RELOAD),
            ("重启设备", OP_REBOOT),
            ("恢复出厂", OP_FACTORY),
        ]
        for text, code in ops:
            btn = ttk.Button(btn_frame, text=text, style="Action.TButton",
                             command=lambda c=code: self._device_operation(c))
            btn.pack(side=tk.LEFT, padx=2, expand=True, fill=tk.X)

    def _build_io_status(self, parent):
        """三、IO 状态"""
        frame = ttk.LabelFrame(parent, text="  IO 状态  ", style="Section.TLabelframe")
        frame.pack(fill=tk.X, pady=4)

        inner = tk.Frame(frame, bg=C_FRAME_BG)
        inner.pack(fill=tk.X, padx=10, pady=6)

        # --- 数字输入 ---
        tk.Label(inner, text="数字输入", bg=C_FRAME_BG,
                 font=("Microsoft YaHei", 9, "bold"), fg=C_ACCENT).pack(anchor=tk.W, pady=(0, 2))

        input_frame = tk.Frame(inner, bg=C_FRAME_BG)
        input_frame.pack(fill=tk.X)
        self.io_inputs = {}
        for i, name in enumerate(INPUT_NAMES):
            frame_io = tk.Frame(input_frame, bg=C_OFF, relief=tk.RAISED, bd=1)
            frame_io.pack(side=tk.LEFT, padx=2, ipadx=8, ipady=2)
            lbl = tk.Label(frame_io, text=name, bg=C_OFF, fg="white",
                           font=("Consolas", 9, "bold"), width=3)
            lbl.pack()
            self.io_inputs[i] = (frame_io, lbl)

        # --- 模拟量输入 ---
        tk.Label(inner, text="模拟量输入 (ADC 原始值)", bg=C_FRAME_BG,
                 font=("Microsoft YaHei", 9, "bold"), fg=C_ACCENT).pack(anchor=tk.W, pady=(8, 2))

        ai_frame = tk.Frame(inner, bg=C_FRAME_BG)
        ai_frame.pack(fill=tk.X)
        self.io_ai_labels = {}
        for i in range(4):
            cell = tk.Frame(ai_frame, bg="white", relief=tk.SUNKEN, bd=1)
            cell.pack(side=tk.LEFT, padx=2, fill=tk.X, expand=True)
            tk.Label(cell, text=f"AI{i}", bg=C_FRAME_BG, font=("Consolas", 8), fg="#666").pack(pady=(2, 0))
            lbl = tk.Label(cell, text="--", bg="white", font=("Consolas", 10, "bold"), fg=C_TEXT)
            lbl.pack(pady=(0, 2))
            self.io_ai_labels[i] = lbl

        # --- 数字输出 (可操控) ---
        tk.Label(inner, text="数字输出 (点击切换)", bg=C_FRAME_BG,
                 font=("Microsoft YaHei", 9, "bold"), fg=C_ACCENT).pack(anchor=tk.W, pady=(8, 2))

        output_frame = tk.Frame(inner, bg=C_FRAME_BG)
        output_frame.pack(fill=tk.X)
        self.io_outputs = {}
        self.io_output_vars = {}
        for i, name in enumerate(OUTPUT_NAMES):
            frame_io = tk.Frame(output_frame, bg=C_OFF, relief=tk.RAISED, bd=1)
            frame_io.pack(side=tk.LEFT, padx=2, ipadx=6, ipady=2)
            var = tk.IntVar(value=0)
            self.io_output_vars[i] = var
            cb = tk.Checkbutton(frame_io, text=name, variable=var,
                               bg=C_OFF, fg="white", selectcolor=C_ON,
                               activebackground=C_OFF,
                               font=("Consolas", 9, "bold"), width=3,
                               indicatoron=False,
                               command=lambda idx=i: self._toggle_output(idx))
            cb.pack()
            self.io_outputs[i] = (frame_io, cb)

    def _build_monitor(self, parent):
        """四、监测数据"""
        frame = ttk.LabelFrame(parent, text="  监测数据  ", style="Section.TLabelframe")
        frame.pack(fill=tk.X, pady=(0, 4))

        inner = tk.Frame(frame, bg=C_FRAME_BG)
        inner.pack(fill=tk.X, padx=10, pady=6)

        # 自动刷新控制
        ctrl = tk.Frame(inner, bg=C_FRAME_BG)
        ctrl.pack(fill=tk.X, pady=(0, 4))

        self.chk_auto = ttk.Checkbutton(ctrl, text="自动刷新 (1秒)",
                                        variable=self.auto_refresh)
        self.chk_auto.pack(side=tk.LEFT)

        self.lbl_update_time = tk.Label(ctrl, text="", bg=C_FRAME_BG,
                                        font=("Microsoft YaHei", 8), fg="#999")
        self.lbl_update_time.pack(side=tk.RIGHT)

        # 2x3 全框网格
        grid = tk.Frame(inner, bg=C_FRAME_BG)
        grid.pack(fill=tk.X)
        grid.columnconfigure(0, weight=1)
        grid.columnconfigure(1, weight=1)
        grid.columnconfigure(2, weight=1)

        monitors = [
            ("合母电压 HM", "hm_volt", "V", 0, 0),
            ("控母电压 KM", "km_volt", "V", 0, 1),
            ("当前档位", "gear", "", 0, 2),
            ("报警状态", "alarm", "", 1, 0),
            ("控制模式", "ctrl_mode", "", 1, 1),
            ("HM-KM 压差", "diff_volt", "V", 1, 2),
        ]

        for label, key, unit, row, col in monitors:
            cell = tk.Frame(grid, bg=C_FRAME_BG, relief=tk.GROOVE, bd=2)
            cell.grid(row=row, column=col, padx=3, pady=3, sticky="nsew")
            tk.Label(cell, text=label, bg=C_FRAME_BG,
                     font=("Microsoft YaHei", 9), fg="#666").pack(pady=(4, 0))
            val_frame = tk.Frame(cell, bg=C_FRAME_BG)
            val_frame.pack(pady=4)
            lbl = tk.Label(val_frame, text="--", font=("Consolas", 22, "bold"),
                           fg=C_ACCENT, bg=C_FRAME_BG)
            lbl.pack(side=tk.LEFT)
            if unit:
                tk.Label(val_frame, text=f" {unit}", bg=C_FRAME_BG,
                         font=("Microsoft YaHei", 11), fg="#999").pack(side=tk.LEFT)
            setattr(self, f"lbl_{key}", lbl)

        # 档位高亮条
        self.canvas_drop = tk.Canvas(inner, height=36, bg="white",
                                     highlightthickness=1, highlightbackground="#ddd")
        self.canvas_drop.pack(fill=tk.X, pady=(4, 0))

    def _build_params(self, parent):
        """五、参数设置"""
        frame = ttk.LabelFrame(parent, text="  参数设置  ", style="Section.TLabelframe")
        frame.pack(fill=tk.X, pady=4)

        inner = tk.Frame(frame, bg=C_FRAME_BG)
        inner.pack(fill=tk.X, padx=10, pady=6)

        # 参数列表: (标签, 变量名, 单位, 除数)
        params = [
            ("目标电压", "target_v", "V", 100.0),
            ("每档压降", "step_v", "V", 100.0),
            ("死区上限", "deadband_u", "V", 100.0),
            ("死区下限", "deadband_l", "V", 100.0),
            ("最大压降", "max_drop", "V", 100.0),
        ]

        self.param_entries = {}
        self.param_labels = {}

        for i, (label, key, unit, div) in enumerate(params):
            row = tk.Frame(inner, bg=C_FRAME_BG)
            row.pack(fill=tk.X, pady=2)

            tk.Label(row, text=f"{label}:", width=10, anchor=tk.W,
                     bg=C_FRAME_BG, font=("Microsoft YaHei", 9)).pack(side=tk.LEFT)

            entry = ttk.Entry(row, width=10, font=("Consolas", 11))
            entry.pack(side=tk.LEFT, padx=4)
            self.param_entries[key] = entry

            tk.Label(row, text=unit, bg=C_FRAME_BG,
                     font=("Microsoft YaHei", 9)).pack(side=tk.LEFT, padx=(0, 8))

            lbl_current = tk.Label(row, text="(当前: --)", bg=C_FRAME_BG,
                                   font=("Consolas", 9), fg="#888", width=16, anchor=tk.W)
            lbl_current.pack(side=tk.LEFT)
            self.param_labels[key] = lbl_current

            ttk.Button(row, text="读取", style="Small.TButton",
                       command=lambda k=key: self._read_single_param(k)).pack(side=tk.RIGHT, padx=1)
            ttk.Button(row, text="写入", style="Small.TButton",
                       command=lambda k=key: self._write_single_param(k)).pack(side=tk.RIGHT, padx=1)

        # 一键操作按钮
        btn_row = tk.Frame(inner, bg=C_FRAME_BG)
        btn_row.pack(fill=tk.X, pady=(8, 2))
        ttk.Button(btn_row, text=" 读取全部参数 ", style="Action.TButton",
                   command=self._read_all_params).pack(side=tk.LEFT, padx=4)
        ttk.Button(btn_row, text=" 写入全部参数 ", style="Action.TButton",
                   command=self._write_all_params).pack(side=tk.LEFT, padx=4)

    def _build_log_frame(self):
        """日志区域"""
        frame = ttk.LabelFrame(self.root, text="  通信日志  ", style="Section.TLabelframe")
        frame.pack(fill=tk.BOTH, expand=True, padx=8, pady=4)

        self.log_text = scrolledtext.ScrolledText(
            frame, height=6, font=("Consolas", 9), wrap=tk.WORD,
            bg="#1E1E1E", fg="#D4D4D4", insertbackground="white",
            selectbackground="#264F78", relief=tk.FLAT, borderwidth=4)
        self.log_text.pack(fill=tk.BOTH, expand=True, padx=2, pady=2)

        # 日志标签颜色
        self.log_text.tag_config("info", foreground="#D4D4D4")
        self.log_text.tag_config("ok", foreground="#4EC9B0")
        self.log_text.tag_config("err", foreground="#F44747")
        self.log_text.tag_config("warn", foreground="#CCA700")
        self.log_text.tag_config("tx", foreground="#569CD6")
        self.log_text.tag_config("rx", foreground="#6A9955")

    def _build_status_bar(self):
        """底部状态栏"""
        bar = tk.Frame(self.root, bg="#D5DCE6", height=24)
        bar.pack(fill=tk.X, side=tk.BOTTOM)

        self.lbl_status = tk.Label(bar, text=" 就绪 ", bg="#D5DCE6",
                                   font=("Microsoft YaHei", 8), fg="#666")
        self.lbl_status.pack(side=tk.LEFT, padx=6)

        self.lbl_poll_count = tk.Label(bar, text="", bg="#D5DCE6",
                                       font=("Microsoft YaHei", 8), fg="#999")
        self.lbl_poll_count.pack(side=tk.RIGHT, padx=6)

    # ---------- 串口扫描 ----------

    def _scan_ports(self):
        """扫描可用串口"""
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self.combo_port['values'] = ports if ports else ["(无可用串口)"]
        if ports:
            self.combo_port.current(0)
        self._log(f"扫描到 {len(ports)} 个串口: {', '.join(ports) if ports else '无'}", "info")

    # ---------- 连接 / 断开 ----------

    def _connect(self):
        """连接设备"""
        if self.connected:
            return

        port = self.combo_port.get()
        if not port or "(无" in port:
            messagebox.showwarning("警告", "请先选择有效的串口")
            return

        try:
            baud = int(self.combo_baud.get())
            slave = int(self.spin_slave.get())
        except ValueError:
            messagebox.showwarning("警告", "波特率或从站ID格式错误")
            return

        try:
            self._log(f"正在连接 {port} 波特率 {baud} 从站 #{slave}...", "info")
            self._set_status("连接中...")

            parity_label = self.combo_parity.get()
            parity_code = PARITY_VALUES.get(parity_label, "O")

            instr = minimalmodbus.Instrument(port, slave)
            instr.serial.baudrate = baud
            instr.serial.bytesize = 8
            if parity_code == "E":
                instr.serial.parity = serial.PARITY_EVEN
                instr.serial.stopbits = 1
            elif parity_code == "O":
                instr.serial.parity = serial.PARITY_ODD
                instr.serial.stopbits = 1
            else:  # N — 无校验，须2停止位（Modbus标准）
                instr.serial.parity = serial.PARITY_NONE
                instr.serial.stopbits = 2
            instr.serial.timeout = 0.5
            instr.mode = minimalmodbus.MODE_RTU
            instr.clear_buffers_before_each_transaction = True

            # 测试通信：读版本号
            version = instr.read_register(REG_VERSION, 0)
            self._log(f"连接成功! 设备固件版本: {version}", "ok")

            self.instrument = instr
            self.connected = True
            self._error_count = 0

            # 更新UI状态
            self.btn_connect.configure(state=tk.DISABLED)
            self.btn_disconnect.configure(state=tk.NORMAL)
            self.combo_port.configure(state=tk.DISABLED)
            self.combo_baud.configure(state=tk.DISABLED)
            self.combo_parity.configure(state=tk.DISABLED)
            self.spin_slave.configure(state=tk.DISABLED)
            self.lbl_conn_status.config(text="  已连接  ", fg=C_CONNECTED)

            # 首次读取全部数据
            self._read_device_info()
            self._read_io_status()
            self._read_monitor_data()
            self._read_all_params()

            # 启动轮询线程
            self._poll_event.clear()
            self._poll_thread = threading.Thread(target=self._poll_loop, daemon=True)
            self._poll_thread.start()
            self._log("自动轮询已启动 (1秒间隔)", "info")

        except minimalmodbus.NoResponseError:
            self._log(f"连接失败: 设备无响应 (检查接线/从站ID/波特率)", "err")
            self._set_status("连接失败: 无响应")
            messagebox.showerror("连接失败", "设备无响应\n请检查：\n1. RS485接线 (A+/B-)\n2. 从站ID是否正确\n3. 波特率是否匹配")
        except Exception as e:
            self._log(f"连接失败: {e}", "err")
            self._set_status("连接失败")
            messagebox.showerror("连接失败", str(e))

    def _disconnect(self):
        """断开连接"""
        self._poll_event.set()  # 停止轮询
        if self._poll_thread and self._poll_thread.is_alive():
            self._poll_thread.join(timeout=1.5)

        if self.instrument and self.instrument.serial:
            try:
                self.instrument.serial.close()
            except:
                pass

        self.instrument = None
        self.connected = False
        self._error_count = 0

        self.btn_connect.configure(state=tk.NORMAL)
        self.btn_disconnect.configure(state=tk.DISABLED)
        self.combo_port.configure(state="readonly")
        self.combo_baud.configure(state="readonly")
        self.combo_parity.configure(state="readonly")
        self.spin_slave.configure(state=tk.NORMAL)
        self.lbl_conn_status.config(text="  未连接  ", fg=C_DISCONNECT)

        self._log("已断开连接", "info")
        self._set_status("已断开")

    # ---------- 后台轮询 ----------

    def _poll_loop(self):
        """后台轮询线程：每1秒读取监测数据"""
        while not self._poll_event.is_set():
            if self.auto_refresh.get():
                try:
                    self._read_io_status()
                    self._read_monitor_data()
                    self._error_count = 0
                except Exception as e:
                    self._error_count += 1
                    if self._error_count <= 1:
                        self._log(f"轮询异常: {e}", "warn")
                    if self._error_count >= 5:
                        self._log("连续通信失败5次，请检查连接!", "err")
            self._poll_event.wait(1.0)

    # ---------- 输出操控 ----------

    def _toggle_output(self, idx):
        """点击切换输出状态 — 读写寄存器12"""
        if not self.connected:
            self._log("请先连接设备", "warn")
            self.root.after(0, self._update_io_ui)
            return
        try:
            current = self._safe_read(REG_OUTPUT_STATUS)
            bit = 1 << idx
            new_val = current ^ bit
            self._safe_write(REG_OUTPUT_STATUS, new_val)
            self.data['output'] = new_val
            state = "导通" if (new_val & bit) else "断开"
            self._log(f"Y{idx} → {state}  (寄存器12={new_val})", "tx")
        except Exception as e:
            self._log(f"切换Y{idx}失败: {e}", "err")
        self.root.after(0, self._update_io_ui)

    # ---------- Modbus 读写 ----------

    def _safe_read(self, addr, decimals=0):
        """安全读取单个寄存器"""
        if not self.connected or not self.instrument:
            raise RuntimeError("设备未连接")
        with self._lock:
            return self.instrument.read_register(addr, decimals)

    def _safe_read_range(self, start, count):
        """安全读取连续寄存器"""
        if not self.connected or not self.instrument:
            raise RuntimeError("设备未连接")
        with self._lock:
            return self.instrument.read_registers(start, count)

    def _safe_write(self, addr, value, decimals=0):
        """安全写入单个寄存器"""
        if not self.connected or not self.instrument:
            raise RuntimeError("设备未连接")
        with self._lock:
            self.instrument.write_register(addr, value, number_of_decimals=decimals)

    # ---------- 读取操作 ----------

    def _read_device_info(self):
        """读取设备信息"""
        try:
            vals = self._safe_read_range(REG_SLAVE_ID, 2)
            self.data['slave_id'] = vals[0]
            self.data['baudrate_idx'] = vals[1]

            filt = self._safe_read(REG_FILTER_TIME)
            self.data['filter_time'] = filt

            mode = self._safe_read(REG_CTRL_MODE)
            self.data['ctrl_mode'] = mode

            # 更新UI (在主线程)
            self.root.after(0, self._update_device_info_ui)
            self._log(f"设备信息读取完成 - 从站ID:{vals[0]} "
                      f"波特率:{BAUD_NAMES.get(vals[1], '?')} "
                      f"滤波:{filt}ms 模式:{'自动' if mode == 0 else '手动'}", "rx")
        except Exception as e:
            self._log(f"读取设备信息失败: {e}", "err")

    def _read_io_status(self):
        """读取IO状态 + 模拟量"""
        try:
            vals = self._safe_read_range(REG_INPUT_STATUS, 2)
            self.data['input'] = vals[0]
            self.data['output'] = vals[1]

            # 读取AI0~AI3 (寄存器15~18)
            ai_vals = self._safe_read_range(REG_AI0, 4)
            for i in range(4):
                self.data[f'ai{i}'] = ai_vals[i]

            self.root.after(0, self._update_io_ui)
        except Exception:
            pass  # 轮询中的错误静默处理

    def _read_monitor_data(self):
        """读取监测数据"""
        try:
            vals = self._safe_read_range(REG_HM_VOLT, 4)
            self.data['hm_volt'] = vals[0]
            self.data['km_volt'] = vals[1]
            self.data['gear'] = vals[2]
            self.data['alarm'] = vals[3]
            # 控制模式 (寄存器28)
            self.data['ctrl_mode'] = self._safe_read(REG_CTRL_MODE)
            self.root.after(0, self._update_monitor_ui)
        except Exception:
            pass

    def _read_all_params(self):
        """读取全部参数"""
        try:
            vals = self._safe_read_range(REG_TARGET_V, 6)
            self.data['target_v'] = vals[0]
            self.data['step_v'] = vals[1]
            self.data['deadband_u'] = vals[2]
            self.data['deadband_l'] = vals[3]
            self.data['ctrl_mode'] = vals[4]
            self.data['max_drop'] = vals[5]
            self.root.after(0, self._update_params_ui)
            self._log("全部参数读取完成", "rx")
        except Exception as e:
            self._log(f"读取参数失败: {e}", "err")

    def _read_single_param(self, key):
        """读取单个参数"""
        mapping = {
        "target_v":    (REG_TARGET_V,   100.0, "目标电压"),
        "step_v":      (REG_STEP_V,     100.0, "每档压降"),
        "deadband_u":  (REG_DEADBAND_U, 100.0, "死区上限"),
        "deadband_l":  (REG_DEADBAND_L, 100.0, "死区下限"),
        "max_drop":    (REG_MAX_DROP,   100.0, "最大压降"),
        }
        if key not in mapping:
            return
        addr, div, name = mapping[key]
        try:
            raw = self._safe_read(addr)
            self.data[key] = raw
            val = raw / div
            self.root.after(0, lambda: [
                self.param_entries[key].delete(0, tk.END),
                self.param_entries[key].insert(0, f"{val:.2f}"),
                self.param_labels[key].config(text=f"(当前: {val:.2f})"),
            ])
            self._log(f"读取 {name}: {val:.2f}", "rx")
        except Exception as e:
            self._log(f"读取{name}失败: {e}", "err")

    # ---------- 写入操作 ----------

    PARAM_MAP = {
        "target_v":   (REG_TARGET_V,   "目标电压"),
        "step_v":     (REG_STEP_V,     "每档压降"),
        "deadband_u": (REG_DEADBAND_U, "死区上限"),
        "deadband_l": (REG_DEADBAND_L, "死区下限"),
        "max_drop":   (REG_MAX_DROP,   "最大压降"),
    }

    def _write_single_param(self, key):
        """写入单个参数"""
        if key not in self.PARAM_MAP:
            return
        addr, name = self.PARAM_MAP[key]
        entry = self.param_entries[key]
        try:
            val = float(entry.get().strip())
            raw = int(val * 100)
            self._safe_write(addr, raw)
            self.data[key] = raw
            self._log(f"写入 {name}: {val:.2f}V → 寄存器值 {raw}", "tx")
            self.root.after(0, lambda: self.param_labels[key].config(
                text=f"(当前: {val:.2f})"))
        except ValueError:
            messagebox.showwarning("输入错误", f"{name} 请输入有效数字")
        except Exception as e:
            self._log(f"写入{name}失败: {e}", "err")
            messagebox.showerror("写入失败", str(e))

    def _write_all_params(self):
        """写入全部参数"""
        try:
            for key, (addr, name) in self.PARAM_MAP.items():
                val = float(self.param_entries[key].get().strip())
                raw = int(val * 100)
                self._safe_write(addr, raw)
                self.data[key] = raw
                self._log(f"写入 {name}: {val:.2f}V", "tx")

            self._log("全部参数写入完成 ✓", "ok")
            # 自动保存到EEPROM
            self._safe_write(REG_PARAM_OP, OP_SAVE)
            self._log("已触发 EEPROM 保存", "ok")
            self._read_all_params()
        except ValueError as e:
            messagebox.showwarning("输入错误", f"请检查所有参数格式: {e}")
        except Exception as e:
            self._log(f"批量写入失败: {e}", "err")
            messagebox.showerror("写入失败", str(e))

    # ---------- 设备操作 ----------

    def _device_operation(self, code):
        """执行设备操作（保存/重载/重启/恢复出厂）"""
        if not self.connected:
            messagebox.showwarning("警告", "请先连接设备")
            return

        name = OP_NAMES.get(code, str(code))

        # 恢复出厂需要确认
        if code == OP_FACTORY:
            if not messagebox.askyesno("确认操作",
                                       "恢复出厂设置将清除所有配置参数\n设备将自动重启\n\n确定要继续吗?"):
                return

        # 重启需要确认
        if code == OP_REBOOT:
            if not messagebox.askyesno("确认操作", "设备将重启并断开连接\n确定要继续吗?"):
                return

        try:
            self._safe_write(REG_PARAM_OP, code)
            self._log(f"✓ 已发送: {name}", "ok")

            if code in (OP_REBOOT, OP_FACTORY):
                self._log("设备即将重启，3秒后自动断开...", "warn")
                self.root.after(3000, self._disconnect)

            if code == OP_SAVE:
                self._log("参数已保存到 EEPROM", "ok")
            if code == OP_RELOAD:
                self._log("参数已从 EEPROM 重载", "ok")
                self.root.after(500, self._read_all_params)

        except Exception as e:
            self._log(f"操作失败 ({name}): {e}", "err")
            messagebox.showerror("操作失败", str(e))

    # ---------- UI 更新 ----------

    def _update_device_info_ui(self):
        """更新设备信息面板"""
        sid = self.data.get('slave_id', '--')
        self.lbl_dev_slave_id.config(text=str(sid))

        bidx = self.data.get('baudrate_idx', '--')
        if isinstance(bidx, int) and bidx in BAUD_NAMES:
            self.lbl_dev_baudrate.config(text=BAUD_NAMES[bidx])
        else:
            self.lbl_dev_baudrate.config(text=str(bidx))

        ft = self.data.get('filter_time', '--')
        self.lbl_dev_filter.config(text=str(ft))

        mode = self.data.get('ctrl_mode')
        if mode is not None:
            self.combo_mode.set("自动调压" if mode == 0 else "手动控制")
            self.lbl_dev_mode.config(
                text="(当前: X0={})".format("高" if mode == 1 else "低"),
                fg=C_OK if mode == 0 else C_ALARM)
        else:
            self.combo_mode.set("")

    def _on_mode_change(self, event=None):
        """控制模式切换 — 写寄存器28"""
        if not self.connected:
            return
        sel = self.combo_mode.get()
        new_mode = 1 if sel == "手动控制" else 0
        try:
            self._safe_write(REG_CTRL_MODE, new_mode)
            self.data['ctrl_mode'] = new_mode
            self._log(f"切换控制模式 → {sel} (寄存器28={new_mode})", "tx")
            self.root.after(100, self._read_device_info)
        except Exception as e:
            self._log(f"切换模式失败: {e}", "err")
            self.root.after(0, self._update_device_info_ui)

    def _update_io_ui(self):
        """更新IO状态"""
        # 数字输入
        inp = self.data.get('input', 0)
        for i in range(8):
            frame, lbl = self.io_inputs[i]
            state = (inp >> i) & 1
            color = C_ON if state else C_OFF
            frame.configure(bg=color)
            lbl.configure(bg=color, text=f"{INPUT_NAMES[i]}")

        # 模拟量输入
        for i in range(4):
            val = self.data.get(f'ai{i}', None)
            text = str(val) if val is not None else "--"
            self.io_ai_labels[i].config(text=text)

        # 数字输出 (勾选框)
        out = self.data.get('output', 0)
        for i in range(6):
            frame, cb = self.io_outputs[i]
            state = (out >> i) & 1
            color = C_ON if state else C_OFF
            frame.configure(bg=color)
            cb.configure(bg=color, selectcolor=C_ON,
                        activebackground=color)
            self.io_output_vars[i].set(state)

    def _update_monitor_ui(self):
        """更新监测数据显示"""
        now = datetime.datetime.now().strftime("%H:%M:%S")
        self.lbl_update_time.config(text=f"更新: {now}")

        hm = self.data.get('hm_volt')
        km = self.data.get('km_volt')
        gear = self.data.get('gear')
        alarm = self.data.get('alarm')

        if hm is not None:
            self.lbl_hm_volt.config(text=f"{hm / 100:.2f}")
        if km is not None:
            self.lbl_km_volt.config(text=f"{km / 100:.2f}")

        # 压差
        if hm is not None and km is not None:
            diff = (hm - km) / 100.0
            self.lbl_diff_volt.config(text=f"{diff:.2f}",
                                      fg=C_ALARM if abs(diff) > 35 else C_OK)
        else:
            self.lbl_diff_volt.config(text="--")

        # 档位
        if gear is not None:
            self.lbl_gear.config(text=f"{gear}/7")
            self._update_drop_bar(gear)
        else:
            self.lbl_gear.config(text="--")

        # 报警
        if alarm is not None:
            if alarm == 0:
                self.lbl_alarm.config(text="正常", fg=C_OK)
            else:
                self.lbl_alarm.config(text="报警!", fg=C_ALARM)
        else:
            self.lbl_alarm.config(text="--")

        # 控制模式
        mode = self.data.get('ctrl_mode')
        if mode is not None:
            self.lbl_ctrl_mode.config(
                text="自动" if mode == 0 else "手动",
                fg=C_OK if mode == 0 else C_ALARM)
        else:
            self.lbl_ctrl_mode.config(text="--")

    def _update_drop_bar(self, gear):
        """档位可视化 — 仅G0~G7标签，当前档位高亮"""
        self.canvas_drop.delete("all")
        w = self.canvas_drop.winfo_width()
        if w < 50:
            w = 500

        bar_h = 36
        seg_w = (w - 4) / 8

        for g in range(8):
            x1 = 2 + g * seg_w
            x2 = 2 + (g + 1) * seg_w - 2
            if g == gear:
                fill = "#00CC33"   # 高亮绿
                text_color = "white"
                font_weight = "bold"
            else:
                fill = "#E8ECF0"
                text_color = "#AAA"
                font_weight = "normal"
            self.canvas_drop.create_rectangle(x1, 2, x2, bar_h - 1,
                                              fill=fill, outline="")
            self.canvas_drop.create_text((x1 + x2) / 2, bar_h / 2,
                                         text=f"G{g}",
                                         font=("Consolas", 11, font_weight),
                                         fill=text_color)

    def _update_params_ui(self):
        """更新参数显示"""
        param_map = {
            "target_v":   (REG_TARGET_V,   "target_v"),
            "step_v":     (REG_STEP_V,     "step_v"),
            "deadband_u": (REG_DEADBAND_U, "deadband_u"),
            "deadband_l": (REG_DEADBAND_L, "deadband_l"),
            "max_drop":   (REG_MAX_DROP,   "max_drop"),
        }

        for key, (addr, dkey) in param_map.items():
            raw = self.data.get(dkey)
            if raw is not None:
                val = raw / 100.0
                self.param_labels[key].config(text=f"(当前: {val:.2f})")
                # 仅在没有焦点时更新输入框
                entry = self.param_entries[key]
                if not self.root.focus_get() or self.root.focus_get() != entry:
                    entry.delete(0, tk.END)
                    entry.insert(0, f"{val:.2f}")

    # ---------- 日志 & 状态 ----------

    def _log(self, msg: str, tag: str = "info"):
        """写入日志"""
        timestamp = datetime.datetime.now().strftime("%H:%M:%S")
        # 在主线程中更新UI
        def _write():
            self.log_text.insert(tk.END, f"[{timestamp}] ", "info")
            self.log_text.insert(tk.END, f"{msg}\n", tag)
            self.log_text.see(tk.END)
        self.root.after(0, _write)

    def _set_status(self, text: str):
        """设置状态栏"""
        self.root.after(0, lambda: self.lbl_status.config(text=f" {text} "))

    # ---------- 关闭 ----------

    def _on_close(self):
        """窗口关闭处理"""
        if self.connected:
            if messagebox.askyesno("确认退出", "与设备仍保持连接\n确定要断开并退出吗?"):
                self._disconnect()
            else:
                return
        self.root.destroy()


# ==================== 入口 ====================

def main():
    root = tk.Tk()
    app = SiliconChainMonitor(root)
    root.mainloop()


if __name__ == "__main__":
    main()
