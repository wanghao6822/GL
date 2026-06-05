# Remote-IO — 直流屏硅链调压器 Modbus 远程 IO 板

基于 STM32F103C8T6 + FreeRTOS + Modbus RTU/TCP 的智能远程 IO 设备，集成**直流屏硅链自动调压控制**功能。

---

## 硬件规格

| 类别 | 参数 |
|------|------|
| **MCU** | STM32F103C8T6 (Cortex-M3, 64KB Flash, 20KB RAM) |
| **系统时钟** | 64MHz (内部 HSI) / 可改 72MHz (外部 HSE 8MHz×9) |
| **供电** | DC 9~24V |
| **数字输入** | 8 路 (X0~X7)，DC 24V，兼容 NPN/PNP，内部上拉，带可配置消抖滤波 |
| **数字输出** | 6 路 (Y0~Y5)，开漏模式，LOW=导通 |
| **模拟输入** | 4 路 (AI0~AI3)，16 位 ADC (ADS1115)，±4.096V 量程 |
| **RS485** | 1 路，Modbus RTU 协议 |
| **以太网** | 1 路，Modbus TCP 协议 |
| **调试串口** | 1 路 TTL (PA9/PA10)，可打印系统运行信息 |

---

## 核心功能

### 基础远程 IO
- 8 路数字输入采集，带软件消抖滤波（默认 5ms，1~100ms 可配置）
- 6 路数字输出控制，Modbus 远程读写
- 4 路模拟量采集（ADS1115，16 位精度）
- Modbus RTU (RS485) + Modbus TCP (以太网) 双协议并行，**共享寄存器区**
- 独立硬件看门狗（IWatchdog），超时 400ms
- 拨码开关配置站号 (1~7) 和波特率 (9600/19200/38400/115200)
- MAC / IP 地址可配置，掉电保存（EEPROM）

### 直流屏硅链调压器 🆕
- **2 路电压采集**：合母电压 (HM) + 控母电压 (KM)，经降压模块 (默认 99:1) 后由 ADS1115 采集
- **3 继电器 8 档硅链控制**：Y2/Y3/Y4 编码控制，实现 0~35V 降压调节
- **自动调压模式**：实时检测 KM 电压，自动升/降档维持等于目标值，带死区滞回
- **手动控制模式**：上位机通过 Modbus 直接控制继电器
- **三重报警**：KM 越上限 / KM 越下限 / HM-KM 压差异常，各带 5 秒消抖
- **报警输出**：Y0 硬件导通报警（LOW=报警），寄存器 23 同步上报
- **档位掉电记忆**：当前档位和控制模式保存到 EEPROM
- **可校准降压系数**：HM/KM 各自独立校准 (×100，如 99.00→9900)
- **兼容 110V/220V 系统**：目标电压、每档压降、最大压降等参数均可配置

---

## IO 引脚分配

### 数字输入 (8 路，低电平有效)

| X0 | X1 | X2 | X3 | X4 | X5 | X6 | X7 |
|----|----|----|----|----|----|----|----|
| PB12 | PB13 | PB14 | PB15 | PA8 | PB0 | PA11 | PA12 |

### 数字输出 (6 路，开漏，LOW=导通)

| 名称 | 引脚 | 用途 |
|------|------|------|
| Y0 | PA15 | 报警输出 (AlarmOutPin) |
| Y1 | PB3 | 通用输出 |
| Y2 | PB4 | 硅链控制继电器 1 (bit0) |
| Y3 | PB5 | 硅链控制继电器 2 (bit1) |
| Y4 | PB6 | 硅链控制继电器 3 (bit2) |
| Y5 | PB7 | 通用输出 |

### 硅链继电器 8 档编码表

| 档位 | Y2 | Y3 | Y4 | 降压值 (默认 5V/档) |
|------|----|----|----|---------------------|
| G0 | 0 | 0 | 0 | 35V (最大降压) |
| G1 | 1 | 0 | 0 | 30V |
| G2 | 0 | 1 | 0 | 25V |
| G3 | 1 | 1 | 0 | 20V |
| G4 | 0 | 0 | 1 | 15V |
| G5 | 1 | 0 | 1 | 10V |
| G6 | 0 | 1 | 1 | 5V |
| G7 | 1 | 1 | 1 | 0V (直通) |

### 拨码开关

| 开关 | 引脚 | 功能 |
|------|------|------|
| SW_B1 | PC15 | 站号 bit0 (全 OFF=1) |
| SW_B2 | PA0 | 站号 bit1 |
| SW_B3 | PA1 | 站号 bit2 |
| SW_B4 | PA2 | 波特率 bit0 |
| SW_B5 | PA3 | 波特率 bit1 |

### 指示灯

| 名称 | 引脚 | 说明 |
|------|------|------|
| RUN_LED | PC14 | 运行指示灯，每秒翻转 |
| ERROR_LED | PC13 | 看门狗复位后闪烁 |

### 通信接口

| 接口 | 引脚 | 协议 |
|------|------|------|
| RS485 TX | PB10 | Modbus RTU |
| RS485 RX | PB11 | Modbus RTU |
| RS485 EN | PB1 | 发送使能 |
| I2C SDA | PB9 | ADS1115 |
| I2C SCL | PB8 | ADS1115 |
| 调试 TX | PA9 | 串口打印 (115200bps) |
| 调试 RX | PA10 | 串口打印 |

### 模拟量输入 (ADS1115 @ 0x48，增益 ±4.096V)

| 通道 | 名称 | 用途 |
|------|------|------|
| AI0 | 合母电压 (HM) | 经降压模块 (默认 99:1) 输入 |
| AI1 | 控母电压 (KM) | 经降压模块 (默认 99:1) 输入 |
| AI2 | 预留 | — |
| AI3 | 预留 | — |

---

## 软件架构

### 技术栈

| 层 | 技术 |
|----|------|
| 构建工具 | PlatformIO |
| 开发框架 | Arduino (STM32duino) |
| RTOS | FreeRTOS 10.3.2 |
| 编译器 | arm-none-eabi-gcc 12.3.1 |
| 语言 | C++ (gnu++17) |

### 依赖库

| 库 | 版本 | 用途 |
|----|------|------|
| stm32duino/STM32duino FreeRTOS | ^10.3.2 | 实时操作系统 |
| epsilonrt/Modbus-Serial | ^2.0.5 | Modbus RTU 通信 |
| epsilonrt/Modbus-Ethernet | ^1.0.3 | Modbus TCP 通信 |
| robtillaart/ADS1X15 | ^0.4.2 | ADS1115 模数转换 |
| robtillaart/PCF8575 | ^0.2.2 | I2C 扩展 IO (预留) |
| IWatchdog | 内置 | 独立硬件看门狗 |
| EEPROM | 内置 | 参数掉电保存 |

### FreeRTOS 任务列表

| 任务名 | 优先级 | 堆栈 | 功能 |
|--------|--------|------|------|
| WatchdogTask | 6 (最高) | 96 | 每 200ms 喂狗，监测看门狗复位 |
| X_filter | 5 | 96 | 每 1ms 消抖滤波 8 路数字输入 |
| ModbusRTUTask | 4 | 128 | 处理 Modbus RTU 请求 |
| ModbusTCPTask | 4 | 256 | 处理 Modbus TCP 请求 |
| IICTask | 3 | 384 | 每秒读取 ADS1115 4 通道，换算 HM/KM 电压 |
| MainTask | 3 | 256 | 主逻辑：参数操作、IO 刷新、硅链调压控制、报警检测 |

### 程序启动流程

```
setup()
  ├─ Serial 重定向 (PA9/PA10, 115200bps)
  ├─ 显示系统信息 (时钟/时钟源/MCU ID)
  ├─ Load_Parameter()    → 从 EEPROM 加载参数
  ├─ GPIO_Init()         → 初始化所有 IO + 读取拨码开关
  ├─ ModbusRTU_Initialize() → 初始化 Modbus RTU (寄存器 0~29)
  ├─ ModbusTCP_Initialize() → 初始化 Modbus TCP (共享 RTU 寄存器)
  └─ xTaskCreate(CreateTaskMethods)
       ├─ WatchdogTask
       ├─ X_filter
       ├─ ModbusRTUTask
       ├─ ModbusTCPTask
       ├─ IICTask
       └─ MainTask
       └─ vTaskStartScheduler()
```

### 源文件结构

```
remote-io/
├── src/
│   └── main.cpp              # 程序入口 setup()/loop()
├── include/
│   ├── myTask.h              # 所有 FreeRTOS 任务 + 硅链控制逻辑
│   ├── myModbus.h            # Modbus RTU/TCP 初始化 + 通信任务
│   ├── IO_Setting.h          # GPIO 定义、硅链继电器表、ADC 换算
│   ├── Parameter_Config.h    # 参数结构体、EEPROM 读写、出厂重置
│   ├── myADS1115.h           # ADS1115 驱动封装
│   └── myShowMsg.h           # 串口打印调试信息
├── platformio.ini            # PlatformIO 项目配置
├── Resource/
│   └── extra_script.py       # 编译后生成 HEX 脚本
└── README.md
```

---

## Modbus 寄存器完整映射 (地址 0~29)

> **注意**：Modbus RTU 和 TCP **共享同一寄存器区**，地址 0~29 均可通过两种协议读写。

### 基础 IO 寄存器 (0~19)

| 地址 | 名称 | 读写 | 默认值 | 说明 |
|------|------|------|--------|------|
| 0 | 固件版本 | R | 260604 | 格式：2位年+2位月+2位日 |
| 1 | 从站 ID | R | 拨码决定 | SW_B1~B3，全 OFF=1 |
| 2 | 波特率状态 | R | 拨码决定 | 0=115200, 1=9600, 2=19200, 3=38400 |
| 3 | 参数操作 | R/W | 0 | 写 10=保存, 20=重载, 30=重启, 66=恢复出厂 |
| 4 | 滤波时间 | R/W | 5 | 输入消抖滤波，ms，范围 1~100 |
| 5 | MAC 低 2 字节 | R/W | MCU ID 生成 | MAC[0] + MAC[1] |
| 6 | MAC 中 2 字节 | R/W | MCU ID 生成 | MAC[2] + MAC[3] |
| 7 | MAC 高 2 字节 | R/W | MCU ID 生成 | MAC[4] + MAC[5] |
| 8 | IP 低 2 字节 | R/W | 0x01A8 | IP[2].IP[3]，如 192.168.**1.168** |
| 9 | IP 高 2 字节 | R/W | 0xC0A8 | IP[0].IP[1]，如 **192.168**.1.168 |
| 10 | 运行时间 | R | 0 | 设备运行秒数，0~65535 循环，可作心跳 |
| 11 | 输入状态 | R | — | bit0=X0 … bit7=X7 |
| 12 | 输出状态 | R/W | 0 | bit0=Y0 … bit5=Y5 |
| 13 | 扩展输入 | R | 0 | 预留 (PCF8575) |
| 14 | 扩展输出 | R/W | 0 | 预留 (PCF8575) |
| 15 | AI0 原始值 | R | — | 合母电压 ADC 原始值 (0~32767) |
| 16 | AI1 原始值 | R | — | 控母电压 ADC 原始值 (0~32767) |
| 17 | AI2 原始值 | R | — | 预留 |
| 18 | AI3 原始值 | R | — | 预留 |
| 19 | 保留/测试 | R/W | 0 | 看门狗触发测试 |

### 硅链调压控制寄存器 (20~29) 🆕

| 地址 | 名称 | 读写 | 默认值 | 说明 |
|------|------|------|--------|------|
| 20 | HM 实际电压 | R | — | 合母电压 ×100，如 24300 = 243.00V |
| 21 | KM 实际电压 | R | — | 控母电压 ×100，如 22000 = 220.00V |
| 22 | 当前档位 | R | — | 0~7，当前硅链降压档位 |
| 23 | 报警状态 | R | 0 | 0=正常, 1=故障 (越限/压差异常任一) |
| 24 | 目标电压 | R/W | 22000 | 控母目标值 ×100 (110V系统设为 11000) |
| 25 | 每档压降 | R/W | 500 | 每级降压值 ×100，500 = 5.00V |
| 26 | 死区上限 | R/W | 200 | 偏差死区上限 ×100，200 = 2.00V |
| 27 | 死区下限 | R/W | 200 | 偏差死区下限 ×100，200 = 2.00V |
| 28 | 控制模式 | R/W | 0 | 0=自动调压, 1=手动 (寄存器12 bit2-4 控制 Y2/Y3/Y4) |
| 29 | 最大压降 | R/W | 3500 | 硅链最大降压 ×100，3500=35V (110V系统改为 2100) |

### 参数操作说明 (寄存器 3)

| 写入值 | 操作 | 说明 |
|--------|------|------|
| 10 | 保存参数 | 将当前寄存器值写入 EEPROM |
| 20 | 重载参数 | 从 EEPROM 重新加载到寄存器 |
| 30 | 重启设备 | 软件复位 MCU |
| 66 | 恢复出厂 | 重置所有参数为默认值并重启 |

---

## 硅链调压器工作原理

### 自动调压逻辑 (每 1 秒执行)

```
1. 读取 KM 实际电压 (寄存器 21) 和 HM 实际电压 (寄存器 20)
2. 计算偏差: deviation = TargetVoltage - KM
3. 若 deviation > DeadBandUpper → KM 偏低，升档 (减小降压)
     升档数 = deviation / StepVoltage
4. 若 -deviation > DeadBandLower → KM 偏高，降档 (增大降压)
     降档数 = -deviation / StepVoltage
5. 档位限制: 0~7
6. 写继电器: SetGearOutput(gear) → Y2/Y3/Y4
7. 更新寄存器 22 (当前档位)
```

### 报警检测 (5 秒消抖)

| 报警类型 | 触发条件 | 表达式 |
|----------|----------|--------|
| 越上限 | KM > 目标电压 + 死区上限 | `KM > Target + DeadBandUpper` |
| 越下限 | KM < 目标电压 − 死区下限 | `KM < Target − DeadBandLower` |
| 压差异常 | HM − KM > 最大压降 | `HM − KM > MaxDropVoltage` |

任一报警触发 → Y0 导通 (LOW) + 寄存器 23 = 1

---

## 快速开始

### 1. 环境准备

- [VSCode](https://code.visualstudio.com/) + [PlatformIO 扩展](https://platformio.org/install/ide?install=vscode)
- ST-Link 烧录器 (推荐) 或 USB-TTL 串口模块 (CH340)

### 2. 编译

```bash
cd remote-io
platformio run
```

编译产物位于 `.pio/build/genericSTM32F103C8/`：
- `firmware.elf` — ELF 调试文件
- `firmware.hex` — HEX 烧录文件
- `firmware.bin` — 二进制固件

### 3. 烧录

**方式 A — ST-Link (推荐，无需切换 Boot 模式)**：

`platformio.ini` 默认已配置，直接运行：
```bash
platformio run --target upload
```

**方式 B — 串口烧录 (USB-TTL)**：

修改 `platformio.ini`：
```ini
upload_port = COM3          # 改为实际串口号
upload_protocol = serial
upload_speed = 115200
```
烧录前需将 **BOOT0 置 1**，按复位或重新上电。烧录完成后 **BOOT0 置 0** 才会从 Flash 启动。

### 4. 串口调试

在 `include/myShowMsg.h` 中启用：
```cpp
#define UseSerialPrint  // 取消注释以启用串口打印
```

串口参数：TTL (PA9/PA10)，115200bps。开机后会打印系统时钟、MCU ID 等信息：
```
System Version:260604
Remote IO System Start...
GPIO_Initizing
ModbusRTU initializing
ModbusTCP initializing
All Task Create Success
Watchdog task created.
ModbusRTU task started.
...
[AI] raw0=16384 raw1=16000 | HM=243.00V KM=220.00V | gear=7
```

### 5. Modbus 通信测试

- **RTU**：RS485 连接，波特率和站号由拨码开关决定
- **TCP**：以太网连接，默认 IP `192.168.1.168`，端口 `502`

推荐上位机软件（任选）：
- Modbus Poll (Windows)
- QModMaster (跨平台/免费)
- 自定义 Modbus 客户端

---

## 110V 直流屏系统配置

若用于 110V 直流屏系统，建议修改以下寄存器：

| 寄存器 | 参数 | 220V 默认 | 110V 建议 |
|--------|------|-----------|-----------|
| 24 | 目标电压 | 22000 | 11000 |
| 25 | 每档压降 | 500 | 250 或 500 |
| 29 | 最大压降 | 3500 | 2100 |
| 26 | 死区上限 | 200 | 100 |
| 27 | 死区下限 | 200 | 100 |

修改后写 `10` 到寄存器 3 保存。

---

## 注意事项

1. **ADS1115 初始化失败**：寄存器 15~18 将显示 `32767`。检查 I2C 连接 (PB8/PB9) 和模块供电。
2. **看门狗超时**：ERROR_LED 闪烁表示发生过看门狗复位，检查是否有任务阻塞超过 400ms。
3. **EEPROM 写入寿命**：约 10 万次。参数保存操作 (寄存器 3 写 10) 会触发写入，避免高频循环改写。
4. **降压模块校准**：寄存器 24~29 存储的是 **降压后 ADC 采样的换算值**。若降压模块实际系数不是 99:1，需调整寄存器对应的 `HM_Calibration` / `KM_Calibration`（代码中修改，默认 9900）。
5. **Modbus 库修改**：为实现 RTU/TCP 共享寄存器，已将 Modbus-Serial 和 Modbus-Ethernet 库中的 `_regs_head` / `_regs_last` 从 `private` 改为 `public`。若更新库版本需重新修改。
6. 本程序供学习参考。若用于商业/工业场景，请自行评估并承担风险。

---

## 引用与致谢

- 参考项目：[remote-io (Gitee)](https://gitee.com/manrong_2008/remote-io) — 硬件架构和 Modbus 共享寄存器机制
- 开发框架：[STM32duino](https://github.com/stm32duino/Arduino_Core_STM32)
- FreeRTOS 移植：[STM32duino FreeRTOS](https://github.com/stm32duino/STM32FreeRTOS)

---

*固件版本: 260604 | 更新日期: 2026-06-05*
