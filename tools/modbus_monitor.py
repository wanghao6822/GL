#!/usr/bin/env python3
"""Modbus RTU 串口监听工具 — COM3"""
import serial
import time
import datetime

PORT = "COM3"
BAUD = 115200  # 可改: 9600, 19200, 38400, 115200

def main():
    ser = serial.Serial(PORT, BAUD, bytesize=8, parity='N', stopbits=1, timeout=0.05)
    print(f"[监听] {PORT} @ {BAUD} baud, 按 Ctrl+C 退出\n")

    buf = bytearray()
    last_rx = time.time()

    while True:
        try:
            data = ser.read(ser.in_waiting or 1)
            if data:
                buf.extend(data)
                last_rx = time.time()
            elif buf and (time.time() - last_rx) > 0.01:  # 10ms 空闲 = 帧结束
                ts = datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]
                hex_str = " ".join(f"{b:02X}" for b in buf)
                # 解析基本信息
                addr = buf[0] if len(buf) >= 1 else 0
                func = buf[1] if len(buf) >= 2 else 0
                crc_ok = ""
                if len(buf) >= 4:
                    crc_calc = _crc16(buf[:-2])
                    crc_recv = (buf[-1] << 8) | buf[-2]
                    crc_ok = " CRC✓" if crc_calc == crc_recv else f" CRC✗(calc={crc_calc:04X})"

                func_names = {1:"读线圈",2:"读离散输入",3:"读保持寄存器",4:"读输入寄存器",
                              5:"写单线圈",6:"写单寄存器",15:"写多线圈",16:"写多寄存器"}
                fname = func_names.get(func, "?")

                arrow = "← [响应]" if func < 5 or func in (5,6,15,16) and len(buf) < 8 else "→ [请求]"
                print(f"[{ts}] {arrow} 站号={addr} 功能={fname}({func:02X}) 长度={len(buf)}")
                print(f"        数据: {hex_str}{crc_ok}\n")
                buf.clear()
        except KeyboardInterrupt:
            print("\n[监听] 已停止")
            break
        except Exception as e:
            print(f"[错误] {e}")
            break
    ser.close()

def _crc16(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc

if __name__ == "__main__":
    main()
