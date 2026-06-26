#ifndef _my_Task_H_
#define _my_Task_H_

#include <Arduino.h>
#include <STM32FreeRTOS.h>
#include <IWatchdog.h>
#include "IO_Setting.h"
#include "myModbus.h"
#include "myADS1115.h"

// ============================================================
// 宏定义
// ============================================================

// 位操作：根据bool值设置/清除寄存器的指定位
#define SET_BIT_BY_BOOL(reg, bitIndex, value) \
    ((value) ? ((reg) |= (1 << (bitIndex))) : ((reg) &= ~(1 << (bitIndex))))

// 看门狗超时时间 (ms)
#define WATCHDOG_TIMEOUT_MS 400

// 【调试用】取消注释以开启 FreeRTOS 任务堆栈剩余空间监测
// #define TaskStackTestEnable 1

/************************************************************************************
任务列表：
************************************************************************************/
/**
 * @brief Watchdog定时任务
 */
static void WatchdogTask(void *pvParameters)
{
    vTaskDelay(pdMS_TO_TICKS(500));
    ShowMsg("Watchdog Task started", true);
    IWatchdog.begin(1000 * WATCHDOG_TIMEOUT_MS);
    uint32_t lastFeedTime = millis();
    bool witchDogTimeout = false;

    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
        // 每半超时周期喂狗一次
        if (millis() - lastFeedTime > WATCHDOG_TIMEOUT_MS / 2)
        {
            IWatchdog.reload();
            lastFeedTime = millis();
        }
        // 检测看门狗是否触发过复位
        if (IWatchdog.isReset())
        {
            witchDogTimeout = true;
            IWatchdog.clearReset();
        }
        // 看门狗复位后闪烁错误LED指示
        if (witchDogTimeout)
        {
            digitalWrite(ERROR_LED, LOW);
        }
    }
}

#ifdef TaskStackTestEnable
// 定义任务句柄,用来测试任务堆栈剩余空间,将&taskTest放在任务中获取句柄
TaskHandle_t taskTest;

/**
 * @brief 任务测试函数,用来测试任务堆栈剩余空间
 */
static void TaskStackTest(void *pvParameters)
{
    vTaskDelay(pdMS_TO_TICKS(1000)); // 延时1000ms
    ShowMsg("Task Stack Test Task started", true);
    // 获取任务堆栈的使用情况
    UBaseType_t uxHighWaterMark;
    while (true)
    {
        uxHighWaterMark = uxTaskGetStackHighWaterMark(taskTest);
        ShowMsg("Task Stack High Water Mark " + String(uxHighWaterMark, DEC) + "words by " + pcTaskGetName(taskTest), true);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
#endif

/**
 * @brief IIC任务 — ADS1115 模拟量采集（与PCF8575共用I2C总线，PCF8575预留）
 *
 * 采集周期：1秒
 * ADS1115初始化失败重试5次，全部失败后任务退出，寄存器15~18写入32767(错误标记)
 */
static void IICTask(void *pvParameters)
{
    uint8_t ADS1115InitCounter = 0; // ADS1115初始化计数器

    Wire.setSDA(PB9); // 设置I2C的SDA和SCL引脚
    Wire.setSCL(PB8); // 设置I2C的SDA和SCL引脚
    Wire.begin();     // 初始化Wire库
    ShowMsg("ADS1115 Init start:", true);
    while (!InitializeADS1115())
    {
        ShowMsg("ADS1115 Init Failed,Try again!", true);
        vTaskDelay(pdMS_TO_TICKS(1000));
        ADS1115InitCounter++;
        if (ADS1115InitCounter > 5) // 尝试5次初始化失败后退出
        {
            ShowMsg("ADS1115 Init Failed,Exit!", true);
            myModbusRTU.setHreg(15, 32767);
            myModbusRTU.setHreg(16, 32767);
            myModbusRTU.setHreg(17, 32767);
            myModbusRTU.setHreg(18, 32767);
            vTaskDelete(NULL); // 退出任务
        }
    }
    ShowMsg("ADS1115 Init OK", true);

    while (true)
    {
        static uint32_t delayTime; // 延时时间

        if (millis() - delayTime > 1000) // 每隔1秒读取一次模拟量
        {
            ReadADS1115All(myAI.AI0, myAI.AI1, myAI.AI2, myAI.AI3);
            myModbusRTU.setHreg(15, myAI.AI0);
            myModbusRTU.setHreg(16, myAI.AI1);
            myModbusRTU.setHreg(17, myAI.AI2);
            myModbusRTU.setHreg(18, myAI.AI3);

            // 硅链：ADC原始值换算为实际电压（×100），写入寄存器20/21
            uint32_t hmVoltage = ADCToVoltage(myAI.AI0, myPar.HM_Calibration);
            uint32_t kmVoltage = ADCToVoltage(myAI.AI1, myPar.KM_Calibration);
            myModbusRTU.setHreg(20, (uint16_t)(hmVoltage > 65535 ? 65535 : hmVoltage));
            myModbusRTU.setHreg(21, (uint16_t)(kmVoltage > 65535 ? 65535 : kmVoltage));

            // 实时打印采样值
            ShowMsg("[AI] raw0=" + String(myAI.AI0) + " raw1=" + String(myAI.AI1) +
                    " | HM=" + String(hmVoltage / 100) + "." + String(hmVoltage % 100 / 10) + String(hmVoltage % 10) +
                    "V KM=" + String(kmVoltage / 100) + "." + String(kmVoltage % 100 / 10) + String(kmVoltage % 10) +
                    "V | gear=" + String(myPar.CurrentGear), true);
            delayTime = millis();
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * 将参数加载到MB寄存器中
 */
static void Load_ParameterTORegister(void)
{
    // 读取参数到寄存器
    myModbusRTU.setHreg(0, Version);             // 固件版本
    myModbusRTU.setHreg(1, myPar.SlaveId);       // 设备ID
    myModbusRTU.setHreg(2, baudRateSwitchState); // 波特率

    myModbusRTU.setHreg(4, myPar.Input_Filter_Time);            // 输入滤波时间
    myModbusRTU.setHreg(5, (myPar.mac[1] << 8) + myPar.mac[0]); // MAC地址字节1和2
    myModbusRTU.setHreg(6, (myPar.mac[3] << 8) + myPar.mac[2]); // MAC地址字节3和4
    myModbusRTU.setHreg(7, (myPar.mac[5] << 8) + myPar.mac[4]); // MAC地址字节5和6
    myModbusRTU.setHreg(8, (myPar.ip[2] << 8) + myPar.ip[3]);   // IP地址低位1/2
    myModbusRTU.setHreg(9, (myPar.ip[0] << 8) + myPar.ip[1]);   // IP地址高位3/4

    // 硅链调压参数→寄存器
    myModbusRTU.setHreg(24, myPar.TargetVoltage);
    myModbusRTU.setHreg(25, myPar.StepVoltage);
    myModbusRTU.setHreg(26, myPar.DeadBandUpper);
    myModbusRTU.setHreg(27, myPar.DeadBandLower);
    myModbusRTU.setHreg(28, myPar.ControlMode);
    myModbusRTU.setHreg(29, myPar.MaxDropVoltage);
    myModbusRTU.setHreg(30, myPar.ParityMode);   // 校验模式
    myModbusRTU.setHreg(31, myPar.HM_Calibration); // HM校准系数
    myModbusRTU.setHreg(32, myPar.KM_Calibration); // KM校准系数
}
/**
 * @brief 钳位辅助：若val超出[min,max]则回退到fallback
 * @note  与PLC梯形图7参数合法性校验逻辑等效
 */
static inline uint16_t ClampParam(uint16_t val, uint16_t minVal, uint16_t maxVal, uint16_t fallback)
{
    return (val >= minVal && val <= maxVal) ? val : fallback;
}

/**
 * @brief 将MB寄存器参数保存到参数变量中,同时保存到EEPROM
 * @note  参数范围校验（参照PLC梯形图7 / SCTY-D1规格书）：
 *        - 目标电压: 0~300.00V（0~30000），兼容110V/220V系统
 *        - 每档压降: 1.00~20.00V（100~2000）
 *        - 死区:     0~20.00V（0~2000），PLC: D594/D596∈[0,1000]
 *        - 最大压降: 5.00~50.00V（500~5000），220V系统≤35V，110V系统≤21V
 *        - 校准系数: 无范围限制，默认91.00
 *        - 控制模式: 0=强制自动, 1=强制手动, 2=跟随X0硬件开关（默认）
 *        - 校验模式: 0=无校验, 1=偶校验, 2=奇校验
 */
static void Save_ParameterFromRegister()
{
    // 保存寄存器参数到参数变量
    uint16_t val;
    val = myModbusRTU.hreg(4);
    myPar.Input_Filter_Time = ClampParam(val, 1, 100, myPar.Input_Filter_Time);

    myPar.mac[0] = myModbusRTU.hreg(5) & 0xFF;
    myPar.mac[1] = (myModbusRTU.hreg(5) >> 8) & 0xFF;
    myPar.mac[2] = myModbusRTU.hreg(6) & 0xFF;
    myPar.mac[3] = (myModbusRTU.hreg(6) >> 8) & 0xFF;
    myPar.mac[4] = myModbusRTU.hreg(7) & 0xFF;
    myPar.mac[5] = (myModbusRTU.hreg(7) >> 8) & 0xFF;

    myPar.ip[0] = (myModbusRTU.hreg(9) >> 8) & 0xFF;
    myPar.ip[1] = myModbusRTU.hreg(9) & 0xFF;
    myPar.ip[2] = (myModbusRTU.hreg(8) >> 8) & 0xFF;
    myPar.ip[3] = myModbusRTU.hreg(8) & 0xFF;

    // 硅链调压参数：从寄存器回读（含范围校验，超限回退旧值）
    val = myModbusRTU.hreg(24);
    myPar.TargetVoltage  = ClampParam(val, 0, 30000, myPar.TargetVoltage);

    val = myModbusRTU.hreg(25);
    myPar.StepVoltage    = ClampParam(val, 100, 2000, myPar.StepVoltage);

    val = myModbusRTU.hreg(26);
    myPar.DeadBandUpper  = ClampParam(val, 0, 2000, myPar.DeadBandUpper);

    val = myModbusRTU.hreg(27);
    myPar.DeadBandLower  = ClampParam(val, 0, 2000, myPar.DeadBandLower);

    val = myModbusRTU.hreg(28);
    myPar.ControlMode    = ClampParam(val, 0, 2, myPar.ControlMode); // 0=强制自动, 1=强制手动, 2=跟随X0

    val = myModbusRTU.hreg(29);
    myPar.MaxDropVoltage = ClampParam(val, 500, 5000, myPar.MaxDropVoltage);

    val = myModbusRTU.hreg(30);
    myPar.ParityMode     = ClampParam(val, 0, 2, myPar.ParityMode);

    myPar.HM_Calibration = myModbusRTU.hreg(31);
    myPar.KM_Calibration = myModbusRTU.hreg(32);

    Save_Parameter();
}

/// @brief 主任务 — 参数管理 + IO刷新 + 硅链调压控制 + 报警检测
/// @param pvParameters FreeRTOS任务参数（未使用）
///
/// 执行周期：
///   - 10ms:  参数操作响应 + 输入/输出状态刷新
///   - 1000ms: 运行时间更新 + 硅链调压 + 报警检测
///
/// 硅链调压算法（等效PLC梯形图10公式）：
///   deviation  = TargetVoltage - KM
///   gear_steps = deviation / dynStepVoltage        (PLC: D532 = (D524-D520)/D570)
///   targetGear = clamp(CurrentGear ± gear_steps, 0, 7)
///
/// 档位-继电器编码（与SCTY-D1用户手册表3一致）：
///   G0(全断=35V)→G1→G2→G3→G4→G5→G6→G7(全通=0V)
static void MainTask(void *pvParameters)
{
    vTaskDelay(pdMS_TO_TICKS(500)); // 延时500毫秒
    ShowMsg("Main task started", true);
    uint32_t timeRecord = 0;  // 记录时间
    uint16_t Input_Temp = 0;  // 输入状态暂存
    uint16_t Output_Temp = 0; // 输出状态暂存
    uint16_t Param_Temp = 0;  // 参数操作暂存
    bool runLedTemp = false;  // 运行LED状态暂存

    // 硅链报警消抖计时
    uint32_t alarmOverTime = 0;  // 越上限计时
    uint32_t alarmUnderTime = 0; // 越下限计时
    uint32_t alarmDiffTime = 0;  // 压差异常计时
    bool alarmState = false;     // 当前报警状态

    Load_ParameterTORegister(); // 读取参数到寄存器
    /*打印信息*/
    ShowMsg("", true);
    ShowMsg("Parameter SaveFlag:" + String(myPar.InitFlag), true);                                                                  // 打印参数保存标志
    ShowMsg("ID:" + String(myPar.SlaveId), true);                                                                                   // 打印设备ID
    ShowMsg("BaudRate:" + String(myPar.Baudrate), true);                                                                            // 打印波特率
    ShowMsg("IP:" + String(myPar.ip[0]) + "." + String(myPar.ip[1]) + "." + String(myPar.ip[2]) + "." + String(myPar.ip[3]), true); // 打印IP地址
    ShowMsg("Port:" + String(MODBUSIP_PORT), true);                                                                                 // 打印MODBUS-TCP端口
    ShowMsg("Mac:" + String(myPar.mac[0]) + " " + String(myPar.mac[1]) + " " + String(myPar.mac[2]) + " ");                         // 打印MAC地址
    ShowMsg(String(myPar.mac[3]) + " " + String(myPar.mac[4]) + " " + String(myPar.mac[5]), true);                                  // 打印MAC地址
    ShowMsg("", true);
    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
        /********************************参数设置********************************/
        Param_Temp = myModbusRTU.hreg(3); // 读取参数操作寄存器
        if (Param_Temp != 0)              // 非0时进行参数设置
        {
            myModbusRTU.setHreg(3, 0);
            if (Param_Temp == ParameterOption::Save) // 保存参数
            {
                IWatchdog.reload();           // 保存参数需要耗费一定时间，所以需要在保存之前喂一次狗，避免看门狗超时
                Save_ParameterFromRegister(); // 保存参数到EEPROM
            }
            else if (Param_Temp == ParameterOption::Reload) // 重新加载参数
            {
                Load_ParameterTORegister(); // 读取参数到寄存器
            }
            else if (Param_Temp == ParameterOption::Reboot) // 重启系统
            {
                NVIC_SystemReset(); // 重启系统
            }
            else if (Param_Temp == ParameterOption::Factory_Reset) // 工厂设置
            {
                IWatchdog.reload(); // 保存参数需要耗费一定时间，所以需要在保存之前喂一次狗，避免看门狗超时
                Parameter_Init();   // 重置参数
                NVIC_SystemReset(); // 重启系统
            }
            Param_Temp = 0;
        }

        /********************************输入状态刷新********************************/
        // 将 X0~X7 的 bool 状态打包为寄存器11的8个bit位
        SET_BIT_BY_BOOL(Input_Temp, 0, Input.X0);
        SET_BIT_BY_BOOL(Input_Temp, 1, Input.X1);
        SET_BIT_BY_BOOL(Input_Temp, 2, Input.X2);
        SET_BIT_BY_BOOL(Input_Temp, 3, Input.X3);
        SET_BIT_BY_BOOL(Input_Temp, 4, Input.X4);
        SET_BIT_BY_BOOL(Input_Temp, 5, Input.X5);
        SET_BIT_BY_BOOL(Input_Temp, 6, Input.X6);
        SET_BIT_BY_BOOL(Input_Temp, 7, Input.X7);
        myModbusRTU.setHreg(11, Input_Temp); // 将输入状态写入寄存器11

        /********************************输出状态刷新********************************/
        // 寄存器12变更时刷新物理输出
        if (myModbusRTU.hreg(12) != Output_Temp)
        {
            Output_Temp = myModbusRTU.hreg(12);
            // 手/自动模式判定（优先级: 寄存器28 > X0硬件开关）
            // 寄存器28=2(默认) → 跟随X0; =0→强制自动; =1→强制手动
            uint8_t reg28 = myModbusRTU.hreg(28);
            uint8_t effMode = (reg28 == 2) ? (Input.X0 ? 0 : 1) : reg28;

            if (effMode == 0) // 自动模式：Y0=报警, Y2/Y3/Y4=硅链控制, Y1/Y5=通用
            {
                digitalWrite(Output_Y1, (Output_Temp & 0x02) > 0 ? LOW : HIGH);
                digitalWrite(Output_Y5, (Output_Temp & 0x20) > 0 ? LOW : HIGH);
            }
            else // 手动模式：Y2-Y5由上位机通过寄存器12控制，Y0=报警
            {
                digitalWrite(Output_Y1, (Output_Temp & 0x02) > 0 ? LOW : HIGH);
                digitalWrite(Output_Y2, (Output_Temp & 0x04) > 0 ? LOW : HIGH);
                digitalWrite(Output_Y3, (Output_Temp & 0x08) > 0 ? LOW : HIGH);
                digitalWrite(Output_Y4, (Output_Temp & 0x10) > 0 ? LOW : HIGH);
                digitalWrite(Output_Y5, (Output_Temp & 0x20) > 0 ? LOW : HIGH);
            }
        }

        /********************************时间刷新 + 硅链调压控制（每秒执行一次）**********/
        if (millis() - timeRecord > 1000)
        {
            timeRecord = millis();
            digitalWrite(RUN_LED, runLedTemp = !runLedTemp);
            myModbusRTU.setHreg(10, timeRecord / 1000);

            // ============================================================
            // 硅链调压控制 — 动态每挡压差 + 动态死区
            // 参照 PLC 梯形图10: (D524-D520)/D570 = D532
            // ============================================================
            uint16_t hmVoltage = myModbusRTU.hreg(20); // HM实际电压(×100)
            uint16_t kmVoltage = myModbusRTU.hreg(21); // KM实际电压(×100)

            // 动态参数（掉电不保存，每周期重新计算）
            static uint16_t dynStepVoltage = 0;    // 动态每挡压差×100
            static uint16_t dynDeadBand = 0;       // 动态死区×100 (=动态压差)
            static int32_t  prevKM = 0;            // 调挡前KM电压（用于计算实际压降）
            static int8_t   prevGear = -1;         // 调挡前档位
            static bool     waitForSettle = false; // true=下周期计算实际压差

            // 首次运行从EEPROM默认值初始化
            if (dynStepVoltage == 0) {
                dynStepVoltage = (myPar.StepVoltage > 0) ? myPar.StepVoltage : 350;
                dynDeadBand    = dynStepVoltage;
            }

            // 手/自动模式判定（优先级: 寄存器28 > X0硬件开关）
            uint8_t reg28 = myModbusRTU.hreg(28);
            uint8_t effMode = (reg28 == 2) ? (Input.X0 ? 0 : 1) : reg28;

            if (effMode == 0) // 自动模式
            {
                // --- 步骤1：动态学习（上周期调挡→本周计算实际每挡压降） ---
                if (waitForSettle)
                {
                    waitForSettle = false;
                    int8_t gearDelta = abs((int8_t)myPar.CurrentGear - prevGear);
                    if (gearDelta > 0 && prevKM > 0)
                    {
                        int32_t voltDrop = prevKM - (int32_t)kmVoltage; // 降压量=调前KM-当前KM
                        if (voltDrop > 0)
                        {
                            dynStepVoltage = (uint16_t)(voltDrop / gearDelta);
                            if (dynStepVoltage < 100 || dynStepVoltage > 500) dynStepVoltage = 350;
                            dynDeadBand = dynStepVoltage;
                        }
                    }
                }

                if (dynStepVoltage == 0) dynStepVoltage = 350;
                if (dynDeadBand == 0)    dynDeadBand    = dynStepVoltage;

                // --- 步骤2：调压判断（PLC公式）---
                int32_t deviation = (int32_t)myPar.TargetVoltage - (int32_t)kmVoltage;

                if (deviation > (int32_t)dynDeadBand) // KM偏低→升档(减小硅链降压)
                {
                    uint8_t steps = deviation / dynStepVoltage;
                    if (steps == 0) steps = 1;
                    int16_t targetGear = (int16_t)myPar.CurrentGear + steps;
                    if (targetGear > 7) targetGear = 7;   // 钳位: 最大G7(直通)

                    if (targetGear != myPar.CurrentGear)
                    {
                        prevKM   = kmVoltage;             // 记录调前电压
                        prevGear = myPar.CurrentGear;      // 记录调前档位
                        myPar.CurrentGear = (uint8_t)targetGear;
                        waitForSettle = true;              // 下周期计算压差
                    }
                }
                else if (-deviation > (int32_t)dynDeadBand) // KM偏高→降档(增大硅链降压)
                {
                    uint8_t steps = (-deviation) / dynStepVoltage;
                    if (steps == 0) steps = 1;
                    int16_t targetGear = (int16_t)myPar.CurrentGear - steps;
                    if (targetGear < 0) targetGear = 0;    // 钳位: 最小G0(最大降压)

                    if (targetGear != myPar.CurrentGear)
                    {
                        prevKM   = kmVoltage;
                        prevGear = myPar.CurrentGear;
                        myPar.CurrentGear = (uint8_t)targetGear;
                        waitForSettle = true;
                    }
                }

                // --- 步骤3：输出继电器 + 更新寄存器 ---
                SetGearOutput(myPar.CurrentGear);
                myModbusRTU.setHreg(22, myPar.CurrentGear);
                // 同步动态值到寄存器（上位机可实时查看）
                myModbusRTU.setHreg(25, dynStepVoltage);
                myModbusRTU.setHreg(26, dynDeadBand);
                myModbusRTU.setHreg(27, dynDeadBand);
            }
            else // 手动模式：Y2/Y3/Y4由寄存器12控制，此处反算档位仅用于显示
            {
                myModbusRTU.setHreg(22, RelayToGear(
                    (Output_Temp >> 2) & 1,
                    (Output_Temp >> 3) & 1,
                    (Output_Temp >> 4) & 1));
            }

            // ============================================================
            // 报警检测（5秒消抖，参照PLC梯形图3的T11~T13定时器）
            // ============================================================
            int32_t diffVoltage = (int32_t)hmVoltage - (int32_t)kmVoltage;
            uint32_t now = millis();
            bool alarmOver = false, alarmUnder = false, alarmDiff = false;

            // 越上限: KM > 目标 + 动态死区，持续5秒
            if ((int32_t)kmVoltage > (int32_t)myPar.TargetVoltage + (int32_t)dynDeadBand)
            {
                if (alarmOverTime == 0) alarmOverTime = now;
                else if (now - alarmOverTime >= 5000) alarmOver = true;
            }
            else { alarmOverTime = 0; }

            // 越下限: KM < 目标 - 动态死区，持续5秒
            if ((int32_t)kmVoltage < (int32_t)myPar.TargetVoltage - (int32_t)dynDeadBand)
            {
                if (alarmUnderTime == 0) alarmUnderTime = now;
                else if (now - alarmUnderTime >= 5000) alarmUnder = true;
            }
            else { alarmUnderTime = 0; }

            // 压差异常: HM-KM > 硅链最大降压值，持续5秒
            if (diffVoltage > (int32_t)myPar.MaxDropVoltage)
            {
                if (alarmDiffTime == 0) alarmDiffTime = now;
                else if (now - alarmDiffTime >= 5000) alarmDiff = true;
            }
            else { alarmDiffTime = 0; }

            // 综合报警输出（任一条件满足即报警）
            alarmState = alarmOver || alarmUnder || alarmDiff;
            digitalWrite(AlarmOutPin, alarmState ? LOW : HIGH); // LOW=导通(报警)
            myModbusRTU.setHreg(23, alarmState ? 1 : 0);
        }
    }
}

/**
 * @brief 创建所有应用任务
 * @note  xTaskCreate参数: (任务函数, 任务名, 堆栈大小(字), 参数, 优先级(0最低), 句柄)
 *        优先级分配: 6=看门狗 5=输入滤波 4=Modbus 3=主逻辑
 */
static void CreateTaskMethods(void *pvParameters)
{
    xTaskCreate(WatchdogTask, "WatchdogTask", 96, NULL, 6, NULL);
    ShowMsg("Watchdog task created.", true);

    xTaskCreate(X_filter, "X_filter", 96, NULL, 5, NULL);  // 堆栈剩余66字节
    ShowMsg("Input filter task created.", true);

    xTaskCreate(ModbusRTUTask, "ModbusRTUSevice", 128, NULL, 4, NULL);  // 堆栈剩余70字节
    ShowMsg("ModbusRTU task created.", true);

    xTaskCreate(ModbusTCPTask, "ModbusTCPTask", 128 * 2, NULL, 4, NULL);  // 堆栈剩余148字节
    ShowMsg("ModbusTCP task created.", true);

    xTaskCreate(IICTask, "IICTask", 128 * 3, NULL, 3, NULL);  // 堆栈剩余186字节
    ShowMsg("IIC task created.", true);

    xTaskCreate(MainTask, "MainTask", 128 * 2, NULL, 3, NULL);  // 堆栈剩余233字节
    ShowMsg("MainTask created.", true);

#ifdef TaskStackTestEnable
    xTaskCreate(TaskStackTest, "TaskStackTest", 128 * 2, NULL, 2, NULL);
    ShowMsg("TaskStackTest created.", true);
#endif

    ShowMsg("All Task Create Success", true);
    ShowMsg("", true);
    vTaskDelete(NULL);
}
#endif