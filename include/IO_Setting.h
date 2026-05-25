#ifndef _IO_SETTING_H_
#define _IO_SETTING_H_
#include <Arduino.h>
#include "Parameter_Config.h"
#include "myShowMsg.h"

uint8_t idSwitchState = 0;       // 站号拨码开关状态
uint8_t baudRateSwitchState = 0; // 波特率拨码开关状态

/*********通信引脚*********/

#define mbRxPin PB11    // 接收引脚
#define mbTxPin PB10    // 发送引脚
#define mbSendEnPin PB1 // 发送使能引脚

/*********拨码开关引脚定义*********/
/**
 * SW_B1 SW_B2 SW_B3组合表示站号
 * SW_B4 SW_B5 组合表示波特率
 */
#define SW_B1 PC15
#define SW_B2 PA0
#define SW_B3 PA1
#define SW_B4 PA2
#define SW_B5 PA3

/*********输入引脚定义*********/
#define Temp_X0 PB12
#define Temp_X1 PB13
#define Temp_X2 PB14
#define Temp_X3 PB15
#define Temp_X4 PA8
#define Temp_X5 PB0
#define Temp_X6 PA11
#define Temp_X7 PA12

/// @brief GPIO端口定义
typedef struct
{
    uint8_t X0 : 1;
    uint8_t X1 : 1;
    uint8_t X2 : 1;
    uint8_t X3 : 1;
    uint8_t X4 : 1;
    uint8_t X5 : 1;
    uint8_t X6 : 1;
    uint8_t X7 : 1;
} GPIO_Port;

/// @brief 输入端口
GPIO_Port Input;

/*********输出引脚定义*********/
#define Output_Y0 PA15
#define Output_Y1 PB3
#define Output_Y2 PB4
#define Output_Y3 PB5
#define Output_Y4 PB6
#define Output_Y5 PB7

/*********指示灯定义*********/
#define ERROR_LED PC13
#define RUN_LED PC14

/*********模拟量定义*********/
typedef struct
{
    int16_t AI0;
    int16_t AI1;
    int16_t AI2;
    int16_t AI3;
} AnalogStruct;

AnalogStruct myAI;

/*********硅链调压控制*********/
#define AlarmOutPin Output_Y0 // 报警输出引脚 Y0(PA15)

// 3继电器→7档位编码表：{Y2, Y3, Y4}，1=导通(LOW)
// 档位0降压最大(35V)，档位6降压最小(5V)
const uint8_t GearRelayTable[7][3] = {
    {0, 0, 0}, // 档位0: 降压35V(220V系统) / 21V(110V系统)
    {1, 0, 0}, // 档位1: 降压30V / 18V
    {0, 1, 0}, // 档位2: 降压25V / 15V
    {1, 1, 0}, // 档位3: 降压20V / 12V
    {0, 0, 1}, // 档位4: 降压15V / 9V
    {1, 0, 1}, // 档位5: 降压10V / 6V
    {0, 1, 1}, // 档位6: 降压5V  / 3V
};

// 根据3个继电器的状态反推档位（用于手动模式读取）
uint8_t RelayToGear(uint8_t y2, uint8_t y3, uint8_t y4)
{
    for (uint8_t i = 0; i < 7; i++)
        if (GearRelayTable[i][0] == y2 && GearRelayTable[i][1] == y3 && GearRelayTable[i][2] == y4)
            return i;
    return 0;
}

// 根据档位写继电器输出
void SetGearOutput(uint8_t gear)
{
    if (gear > 6) gear = 6;
    digitalWrite(Output_Y2, GearRelayTable[gear][0] ? LOW : HIGH);
    digitalWrite(Output_Y3, GearRelayTable[gear][1] ? LOW : HIGH);
    digitalWrite(Output_Y4, GearRelayTable[gear][2] ? LOW : HIGH);
}

// ADC原始值→实际电压（放大100倍存储）
// calibration: 降压系数×100，如99.00→9900
uint16_t ADCToVoltage(int16_t adcValue, uint16_t calibration)
{
    // V_actual = adcValue × 4.096 / 32767 × calibration / 100
    // stored  = V_actual × 100
    float voltage = (float)adcValue * 4.096f / 32767.0f * (float)calibration / 100.0f;
    return (uint16_t)(voltage * 100.0f);
}


/*设置输出模式并设置为低电平*/
void pinMode_OutSetting(uint32_t ulPin)
{
    pinMode(ulPin, OUTPUT_OPEN_DRAIN);
    digitalWrite(ulPin, HIGH);
}

/**
 * GPIO初始化
 */
void GPIO_Init()
{
    ShowMsg("GPIO_Initizing", true);
    /*拨码开关初始化*/
    pinMode(SW_B1, INPUT_PULLUP);
    pinMode(SW_B2, INPUT_PULLUP);
    pinMode(SW_B3, INPUT_PULLUP);
    pinMode(SW_B4, INPUT_PULLUP);
    pinMode(SW_B5, INPUT_PULLUP);
    /*输入引脚初始化*/
    pinMode(Temp_X0, INPUT);
    pinMode(Temp_X1, INPUT);
    pinMode(Temp_X2, INPUT);
    pinMode(Temp_X3, INPUT);
    pinMode(Temp_X4, INPUT);
    pinMode(Temp_X5, INPUT);
    pinMode(Temp_X6, INPUT);
    pinMode(Temp_X7, INPUT);
    /*输出引脚初始化*/
    pinMode_OutSetting(Output_Y0);
    pinMode_OutSetting(Output_Y1);
    pinMode_OutSetting(Output_Y2);
    pinMode_OutSetting(Output_Y3);
    pinMode_OutSetting(Output_Y4);
    pinMode_OutSetting(Output_Y5);
    /*指示灯引脚初始化*/
    pinMode_OutSetting(ERROR_LED);
    pinMode_OutSetting(RUN_LED);
    /*获取拨码开关状态*/
    idSwitchState = (!digitalRead(SW_B3) << 2) | (!digitalRead(SW_B2) << 1) | !digitalRead(SW_B1);
    baudRateSwitchState = (!digitalRead(SW_B5) << 1) | !digitalRead(SW_B4);
    myPar.SlaveId = (idSwitchState == 0 ? 1 : idSwitchState);
    switch (baudRateSwitchState)
    {
    case 0:
        myPar.Baudrate = 115200;
        
        break;
    case 1:
        myPar.Baudrate = 9600;
        break;
    case 2:
        myPar.Baudrate = 19200;
        break;
    case 3:
        myPar.Baudrate = 38400;
        break;
    default:
        myPar.Baudrate = 115200;
        break;
    }    
    ShowMsg("GPIO_Initized", true);
}

/*输入滤波函数*/
void X_filter(void *pvParameters) // 每1MS调用一次，用来给输入滤波,滤波时间由Input_Filter_Time指定，默认5ms
{
    vTaskDelay(pdMS_TO_TICKS(100)); // 延时100ms再启动任务
    ShowMsg("X_filter task started", true);
    static uint8_t x_buffer[8];     // 刷新端口数
    static uint32_t timeRecord = 0; // 记录上一次刷新时间
    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(1)); // 延时1个滴答
        // 输入滤波
        if (millis() - timeRecord >= 1) // 1ms刷新一次
        {
            timeRecord = millis(); // 更新刷新时间
            (digitalRead(Temp_X0)) ? (x_buffer[0] = 0, Input.X0 = 0) : ((x_buffer[0] < myPar.Input_Filter_Time) ? (x_buffer[0]++) : (Input.X0 = 1));
            (digitalRead(Temp_X1)) ? (x_buffer[1] = 0, Input.X1 = 0) : ((x_buffer[1] < myPar.Input_Filter_Time) ? (x_buffer[1]++) : (Input.X1 = 1));
            (digitalRead(Temp_X2)) ? (x_buffer[2] = 0, Input.X2 = 0) : ((x_buffer[2] < myPar.Input_Filter_Time) ? (x_buffer[2]++) : (Input.X2 = 1));
            (digitalRead(Temp_X3)) ? (x_buffer[3] = 0, Input.X3 = 0) : ((x_buffer[3] < myPar.Input_Filter_Time) ? (x_buffer[3]++) : (Input.X3 = 1));
            (digitalRead(Temp_X4)) ? (x_buffer[4] = 0, Input.X4 = 0) : ((x_buffer[4] < myPar.Input_Filter_Time) ? (x_buffer[4]++) : (Input.X4 = 1));
            (digitalRead(Temp_X5)) ? (x_buffer[5] = 0, Input.X5 = 0) : ((x_buffer[5] < myPar.Input_Filter_Time) ? (x_buffer[5]++) : (Input.X5 = 1));
            (digitalRead(Temp_X6)) ? (x_buffer[6] = 0, Input.X6 = 0) : ((x_buffer[6] < myPar.Input_Filter_Time) ? (x_buffer[6]++) : (Input.X6 = 1));
            (digitalRead(Temp_X7)) ? (x_buffer[7] = 0, Input.X7 = 0) : ((x_buffer[7] < myPar.Input_Filter_Time) ? (x_buffer[7]++) : (Input.X7 = 1));
        }
        // SetOut(OutputEnum::Test,static_cast<OutState>(!digitalRead(OutputEnum::Test)));
    }
}

#endif