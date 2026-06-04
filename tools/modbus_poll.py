#!/usr/bin/env python3
"""Modbus RTU 波特率探测 + 轮询 — COM3"""
import serial
import time

PORT = "COM3"
SLAVE = 1

def crc16(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc

def try_connect(baud):
    print(f"\n尝试 {baud} baud...", end=" ", flush=True)
    try:
        ser = serial.Serial(PORT, baud, bytesize=8, parity='N', stopbits=1, timeout=0.8)
        # 请求读版本号(寄存器0)
        cmd = bytes([SLAVE, 3, 0, 0, 0, 1])
        c = crc16(cmd)
        cmd += bytes([c & 0xFF, c >> 8])
        ser.write(cmd)
        time.sleep(0.15)
        resp = ser.read(100)
        if resp and len(resp) >= 5:
            print(f"✓ 收到响应! {resp.hex(' ').upper()}")
            ser.close()
            return True, baud, resp
        ser.close()
        print("无响应")
        return False, baud, None
    except Exception as e:
        print(f"错误: {e}")
        return False, baud, None

def poll_all(baud):
    """找到正确波特率后，读取全部关键寄存器"""
    print(f"\n{'='*70}")
    print(f"使用 {baud} baud 读取全部数据")
    print("="*70)
    ser = serial.Serial(PORT, baud, bytesize=8, parity='N', stopbits=1, timeout=1.0)

    registers = [
        ("固件版本", 0, 1),
        ("从站ID+波特率", 1, 2),
        ("滤波时间", 4, 1),
        ("输入状态(11)", 11, 1),
        ("输出状态(12)", 12, 1),
        ("AI0~AI3", 15, 4),
        ("硅链参数(20~29)", 20, 10),
    ]

    for name, start, count in registers:
        cmd = bytes([SLAVE, 3, (start>>8)&0xFF, start&0xFF, (count>>8)&0xFF, count&0xFF])
        c = crc16(cmd)
        cmd += bytes([c&0xFF, c>>8])

        ser.write(cmd)
        print(f"\nTX → {cmd.hex(' ').upper()}")
        time.sleep(0.15)
        resp = ser.read(ser.in_waiting or 100)
        if resp:
            print(f"RX ← {resp.hex(' ').upper()}")

            if len(resp) >= 5 and resp[1] == 3:
                vals = []
                for i in range(3, resp[2]+3, 2):
                    if i+1 < len(resp)-2:
                        vals.append((resp[i]<<8)|resp[i+1])
                print(f"     [{name}] 值={vals}")
            elif len(resp) >= 3 and resp[1] == 0x83:
                print(f"     [{name}] 异常码={resp[2]}")
        else:
            print(f"RX ← (无响应)")

    ser.close()

if __name__ == "__main__":
    # 探测波特率
    print("=== Modbus 波特率探测 ===")
    found = False
    for baud in [115200, 9600, 19200, 38400]:
        ok, b, _ = try_connect(baud)
        if ok:
            found = True
            poll_all(b)
            break

    if not found:
        print("\n所有波特率均无响应！")
        print("请检查: 1) 板子是否上电  2) RS485接线  3) COM3是否正确  4) 从站ID拨码开关")
