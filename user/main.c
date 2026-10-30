/**
 * ============================================================
 *  智能药盒 - 主程序
 *  主控: STM32F103RCT6  库: 标准外设库  工具链: Keil MDK
 *
 *  模块一览:
 *   bsp_key     按键×4
 *   bsp_buzzer  有源蜂鸣器
 *   bsp_motor   步进电机(TIM4 脉冲)
 *   bsp_limit   限位×2
 *   bsp_lcd     ST7789 SPI 屏
 *   bsp_ws2812  彩灯×3
 *   bsp_esp32   ESP32-C3 无线(USART1)
 *   bsp_w25q    SPI Flash(存储/字库)
 *   bsp_audio   I2S 功放+麦克风
 *   state_machine 五态状态机
 * ============================================================
 */
#include "stm32f10x.h"
#include "pinmap.h"
#include "app_config.h"
#include "bsp_key.h"
#include "bsp_buzzer.h"
#include "bsp_motor.h"
#include "bsp_limit.h"
#include "bsp_lcd.h"
#include "bsp_ws2812.h"
#include "bsp_esp32.h"
#include "bsp_w25q.h"
#include "bsp_audio.h"
#include "state_machine.h"

volatile uint32_t g_ms = 0;     /* 系统 1ms 节拍 */

/* 软件时钟（TODO: 接入内部 RTC + LSE，或由 ESP32/APP 对时） */
static volatile uint8_t g_hour = 8, g_min = 0, g_sec = 0;

/* ---- ESP32 收到帧的回调 ---- */
static void OnEsp32Frame(const ProtoFrame *f)
{
    switch (f->cmd) {
    case CMD_DETECT_RESULT:     /* 视觉检测结果: data[0]=状态码 */
        /* 0x05 已取药 -> 复检通过；0x04 仍在 -> 继续复检 */
        if (f->len >= 1 && f->data[0] == STATUS_TAKEN) {
            SM_OnKeyConfirm();  /* 复用确认路径 */
        }
        break;
    case CMD_SERVER_PUSH:       /* 云端下发：服药计划/设置，透传处理 */
        /* TODO: 解析并更新服药计划 */
        break;
    case CMD_GET_STATUS:
        {   uint8_t st = (uint8_t)SM_GetState();
            ESP32_SendFrame(CMD_ACK, &st, 1);
        }
        break;
    default:
        break;
    }
}

/* ---- 系统 1ms 节拍（在 SysTick 中断里调用） ---- */
void App_Tick1ms(void)
{
    g_ms++;
    Buzzer_Tick();
    Motor_Tick();
}

int main(void)
{
    uint32_t last10ms = 0;      /* 10ms 任务节拍基准 */

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    /* 系统节拍 1ms */
    SysTick_Config(SystemCoreClock / 1000);

    /* 初始化各外设 */
    Buzzer_Init();
    Key_Init();
    Limit_Init();
    Motor_Init();
    LCD_Init();         /* 内含 SPI1 初始化 */
#if FEATURE_EXT_FLASH
    W25Q_Init();        /* 复用 SPI1，仅 CS 独立（待硬件补 FLASH_CS 线后启用） */
#endif
    WS2812_Init();
    ESP32_Init();
    Audio_Init();
    SM_Init();

    ESP32_SetRxHandler(OnEsp32Frame);

    /* 开机自检闪灯 */
    WS2812_SetPixel(0, 0, 255, 0);
    WS2812_Show();
    Buzzer_Beep(200);

    while (1) {
        if (g_ms - last10ms >= 10) {
            last10ms = g_ms;

            /* 软件时钟累加（TODO: 用 RTC 替代） */
            if (++g_sec >= 60) { g_sec = 0; if (++g_min >= 60) { g_min = 0; if (++g_hour >= 24) g_hour = 0; } }

            Key_Scan();

            /* 按键事件 -> 状态机 */
            if (Key_GetEvent(KEY_ID_OK)   == KEY_EVT_SHORT) SM_OnKeyConfirm();
            if (Key_GetEvent(KEY_ID_DN)   == KEY_EVT_SHORT) SM_OnKeyMute();   /* 静音 */
            if (Key_GetEvent(KEY_ID_BACK) == KEY_EVT_LONG)  { /* 复位/回待机 */ SM_OnKeyMute(); }
            (void)Key_GetEvent(KEY_ID_UP); /* 上键暂未用 */

            SM_Run(g_hour, g_min, g_sec);
        }

        /* 串口协议轮询（每次循环都跑，及时处理帧） */
        ESP32_Poll();
    }
}

/* SysTick 中断（stm32f10x_it.c 中的空定义已注释） */
void SysTick_Handler(void)
{
    App_Tick1ms();
}
