#ifndef _my_Task_H_
#define _my_Task_H_

#include <Arduino.h>
#include <STM32FreeRTOS.h>
#include <IWatchdog.h>
#include "IO_Setting.h"
#include "myModbus.h"
#include "myADS1115.h"
// #include "myExternaIO.h"

// 设定字中的位状态
#define SET_BIT_BY_BOOL(uint16_t, bitIndex, value) \
    ((value) ? ((uint16_t) |= (1 << (bitIndex))) : ((uint16_t) &= ~(1 << (bitIndex))))

// 定义是否开启任务堆栈剩余空间测试功能
// #define TaskStackTestEnable 1

// Watchdog超时时间，单位为毫秒
#define WATCHDOG_TIMEOUT_MS 400

/************************************************************************************
任务列表：
************************************************************************************/
/**
 * @brief Watchdog定时任务
 */
void WatchdogTask(void *pvParameters)
{
    vTaskDelay(pdMS_TO_TICKS(500)); // 延时500毫秒
    ShowMsg("Watchdog Task started", true);
    IWatchdog.begin(1000 * WATCHDOG_TIMEOUT_MS); // 启动看门狗，单位是微秒
    uint32_t lastFeedTime = millis();            // 记录上次喂狗时间
    bool witchDogTimeout = false;                // 看门狗超时标志

    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(100)); // 延时100ms
        // 喂狗，更新喂狗时间
        if (millis() - lastFeedTime > WATCHDOG_TIMEOUT_MS / 2)
        {
            IWatchdog.reload(); // 喂狗
            lastFeedTime = millis();
            // ShowMsg("Watchdog Feed", true);
            // if (myModbusRTU.hreg(19) != 0)//用来触发看门狗超时，测试用
            // {
            //     delay(1200); // 延时1秒，等待主程序处理完Modbus数据
            // }
        }
        if (IWatchdog.isReset()) // 看门狗超时被复位过
        {
            witchDogTimeout = true;
            IWatchdog.clearReset(); // 清除复位标志
        }
        if (witchDogTimeout) // 当看门狗超时后，开始闪烁错误LED
        {
            digitalWrite(ERROR_LED, LOW); // 翻转错误LED状态
        }
    }
}

#ifdef TaskStackTestEnable
// 定义任务句柄,用来测试任务堆栈剩余空间,将&taskTest放在任务中获取句柄
TaskHandle_t taskTest;

/**
 * @brief 任务测试函数,用来测试任务堆栈剩余空间
 */
void TaskStackTest(void *pvParameters)
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

// IIC任务,本打算用来读取ADS1115的数值，但总是无法正确读取，这里就先取消
//  /**
//   * @brief IIC任务
//   */
void IICTask(void *pvParameters)
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
            ShowMsg("Read ADS1115...");
            ReadADS1115All(myAI.AI0, myAI.AI1, myAI.AI2, myAI.AI3); // 读取ADS1115的4个通道的模拟量,电压等于当前值当前值(Value*4.096/32767)*1.4545或者Value*0.0001818
            myModbusRTU.setHreg(15, myAI.AI0);
            myModbusRTU.setHreg(16, myAI.AI1);
            myModbusRTU.setHreg(17, myAI.AI2);
            myModbusRTU.setHreg(18, myAI.AI3);

            // 硅链：ADC原始值换算为实际电压（×100），写入寄存器20/21
            myModbusRTU.setHreg(20, ADCToVoltage(myAI.AI0, myPar.HM_Calibration)); // HM实际电压
            myModbusRTU.setHreg(21, ADCToVoltage(myAI.AI1, myPar.KM_Calibration)); // KM实际电压
            delayTime = millis();
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * 将参数加载到MB寄存器中
 */
void Load_ParameterTORegister(void)
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
}
/**
 * @brief 将MB寄存器参数保存到参数变量中,同时保存到EEPROM
 */
void Save_ParameterFromRegister()
{
    // 保存寄存器参数到参数变量
    myPar.Input_Filter_Time = myModbusRTU.hreg(4);
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

    // 硅链调压参数保存
    myPar.TargetVoltage = myModbusRTU.hreg(24);
    myPar.StepVoltage = myModbusRTU.hreg(25);
    myPar.DeadBandUpper = myModbusRTU.hreg(26);
    myPar.DeadBandLower = myModbusRTU.hreg(27);
    myPar.ControlMode = myModbusRTU.hreg(28);
    myPar.MaxDropVoltage = myModbusRTU.hreg(29);
    Save_Parameter();
}

/// @brief 主任务
/// @param pvParameters
void MainTask(void *pvParameters)
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
            else
            {
            }
            Param_Temp = 0;
        }

        /********************************输入状态刷新********************************/
        // 将多个位字段组合成两个字节
        // 现在 combinedBytes 包含了 Input 结构体的位字段组合成的两个字节的数值
        SET_BIT_BY_BOOL(Input_Temp, 0, Input.X0);
        SET_BIT_BY_BOOL(Input_Temp, 1, Input.X1);
        SET_BIT_BY_BOOL(Input_Temp, 2, Input.X2);
        SET_BIT_BY_BOOL(Input_Temp, 3, Input.X3);
        SET_BIT_BY_BOOL(Input_Temp, 4, Input.X4);
        SET_BIT_BY_BOOL(Input_Temp, 5, Input.X5);
        SET_BIT_BY_BOOL(Input_Temp, 6, Input.X6);
        SET_BIT_BY_BOOL(Input_Temp, 7, Input.X7);
        myModbusRTU.setHreg(11, Input_Temp); // 将输入状态写入寄存器10

        /********************************输出状态刷新********************************/
        if (myModbusRTU.hreg(12) != Output_Temp)
        {
            Output_Temp = myModbusRTU.hreg(12);
            if (myPar.ControlMode == 0) // 自动模式：Y0=报警, Y2/Y3/Y4=硅链控制, Y1/Y5=通用
            {
                digitalWrite(Output_Y1, (Output_Temp & 0x02) > 0 ? LOW : HIGH);
                digitalWrite(Output_Y5, (Output_Temp & 0x20) > 0 ? LOW : HIGH);
            }
            else // 手动模式：全部由寄存器12控制
            {
                digitalWrite(Output_Y0, (Output_Temp & 0x01) > 0 ? LOW : HIGH);
                digitalWrite(Output_Y1, (Output_Temp & 0x02) > 0 ? LOW : HIGH);
                digitalWrite(Output_Y2, (Output_Temp & 0x04) > 0 ? LOW : HIGH);
                digitalWrite(Output_Y3, (Output_Temp & 0x08) > 0 ? LOW : HIGH);
                digitalWrite(Output_Y4, (Output_Temp & 0x10) > 0 ? LOW : HIGH);
                digitalWrite(Output_Y5, (Output_Temp & 0x20) > 0 ? LOW : HIGH);
            }
        }

        /********************************时间刷新 + 硅链调压控制（每秒执行一次）**********/
        if (millis() - timeRecord > 1000) // 每隔1秒刷新一次时间
        {
            timeRecord = millis();
            digitalWrite(RUN_LED, runLedTemp = !runLedTemp); // 翻转运行LED状态
            myModbusRTU.setHreg(10, timeRecord / 1000);      // 写入时间到寄存器10

            // ===== 硅链调压控制 =====
            uint16_t hmVoltage = myModbusRTU.hreg(20); // HM实际电压(×100)
            uint16_t kmVoltage = myModbusRTU.hreg(21); // KM实际电压(×100)

            if (myPar.ControlMode == 0) // 自动模式
            {
                int32_t deviation = (int32_t)myPar.TargetVoltage - (int32_t)kmVoltage;

                if (deviation > (int32_t)myPar.DeadBandUpper) // KM偏低→升档(减小降压)
                {
                    uint8_t steps = deviation / myPar.StepVoltage;
                    if (steps == 0) steps = 1; // 至少调1档，防止偏差超过死区但不足1档压降时卡住
                    uint16_t newGear = (uint16_t)myPar.CurrentGear + steps;
                    myPar.CurrentGear = (newGear > 6) ? 6 : (uint8_t)newGear;
                }
                else if (-deviation > (int32_t)myPar.DeadBandLower) // KM偏高→降档(增大降压)
                {
                    uint8_t steps = (-deviation) / myPar.StepVoltage;
                    if (steps == 0) steps = 1; // 至少调1档
                    myPar.CurrentGear = (myPar.CurrentGear < steps) ? 0 : (myPar.CurrentGear - steps);
                }

                SetGearOutput(myPar.CurrentGear);
                myModbusRTU.setHreg(22, myPar.CurrentGear);
            }
            else // 手动模式
            {
                uint8_t manualGear = RelayToGear(
                    (myModbusRTU.hreg(12) >> 2) & 0x01,
                    (myModbusRTU.hreg(12) >> 3) & 0x01,
                    (myModbusRTU.hreg(12) >> 4) & 0x01);
                myPar.CurrentGear = manualGear;
                SetGearOutput(manualGear);
                myModbusRTU.setHreg(22, manualGear);
            }

            // ===== 报警检测（5s消抖） =====
            int32_t diffVoltage = (int32_t)hmVoltage - (int32_t)kmVoltage;
            uint32_t now = millis();
            bool alarmOver = false, alarmUnder = false, alarmDiff = false;

            // 越上限：KM > 目标 + 死区上限
            if (kmVoltage > myPar.TargetVoltage + myPar.DeadBandUpper)
            {
                if (alarmOverTime == 0) alarmOverTime = now;
                else if (now - alarmOverTime >= 5000) alarmOver = true;
            }
            else { alarmOverTime = 0; }

            // 越下限：KM < 目标 - 死区下限
            if (kmVoltage < (int32_t)myPar.TargetVoltage - (int32_t)myPar.DeadBandLower)
            {
                if (alarmUnderTime == 0) alarmUnderTime = now;
                else if (now - alarmUnderTime >= 5000) alarmUnder = true;
            }
            else { alarmUnderTime = 0; }

            // 压差异常：HM - KM > 硅链最大降压
            if (diffVoltage > (int32_t)myPar.MaxDropVoltage)
            {
                if (alarmDiffTime == 0) alarmDiffTime = now;
                else if (now - alarmDiffTime >= 5000) alarmDiff = true;
            }
            else { alarmDiffTime = 0; }

            alarmState = alarmOver || alarmUnder || alarmDiff;
            digitalWrite(AlarmOutPin, alarmState ? LOW : HIGH); // LOW=导通报警
            myModbusRTU.setHreg(23, alarmState ? 1 : 0);
        }
    }
}

/**
 * @brief 创建所有应用任务
 * @note  xTaskCreate参数: (任务函数, 任务名, 堆栈大小(字), 参数, 优先级(0最低), 句柄)
 *        优先级分配: 6=看门狗 5=输入滤波 4=Modbus 3=主逻辑
 */
void CreateTaskMethods(void *pvParameters)
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