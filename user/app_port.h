#ifndef __APP_PORT_H
#define __APP_PORT_H
/**
 * ============================================================
 * 数据访问层（DAL）接口 —— 三层架构中的"数据访问"契约
 *
 *   表现层  app_command / app_ui
 *        |-> 业务层  app_actuator / app_power
 *                 |-> 本文件（接口）
 *                          |-> bsp 目录下的 .c（实现，唯一允许接触寄存器与引脚的地方）
 *
 * 约定：
 *   1) 业务层与表现层只 include 本文件 + app_types.h + app_config.h，
 *      不得直接 include pinmap.h / stm32f10x.h，也不得直接读写 GPIO。
 *   2) 所有函数都必须是非阻塞或短阻塞（<1ms），长延时只允许出现在初始化。
 *   3) 凡是"物理量"都在本层换算完毕（例如 r/min -> 脉冲 Hz 由业务层给）。
 * ============================================================
 */
#include <stdint.h>
#include "app_types.h"

/* ---------------- 系统 / 时间 ---------------- */
void     Sys_Init(void);                /* 时钟确认 + AFIO + JTAG 释放 + WT 复位 */
uint32_t Time_Ms(void);                 /* 上电以来毫秒数（1ms 节拍） */
uint8_t  Time_Elapsed(uint32_t since, uint32_t ms);   /* since 起是否已过 ms */
uint8_t  Sys_TakeBootFlag(void);        /* 1=本次是复位启动（用于日志/自检） */

/* ---------------- 电源（IP5306 硬件层） ---------------- */
void     PwrHw_Init(void);
void     PwrHw_Key(uint8_t pressed);    /* 1=模拟按下 KEY（PC11 高），0=松开 */
uint8_t  PwrHw_Is5V(void);              /* PC9：5V 轨是否存在 */
uint8_t  PwrHw_IsUsb(void);             /* PC8：USB 是否插入 */
void     PwrHw_NetEnable(uint8_t on);   /* ESP32 EN */

/* ---------------- 步进电机（TIM4 脉冲 + 方向/使能/故障） ---------------- */
void     Motor_Init(void);
void     Motor_SetSpeed(uint16_t hz);
void     Motor_SetDir(uint8_t forward); /* 1=正向（出药方向）  0=反向（回零方向） */
void     Motor_Enable(uint8_t on);
uint8_t  Motor_IsEnabled(void);
void     Motor_Start(uint32_t steps);   /* 发 steps 个脉冲后自动停；0=非法 */
void     Motor_Stop(void);              /* 立即停止发脉冲（保持使能状态） */
uint8_t  Motor_IsBusy(void);
uint32_t Motor_StepsDone(void);
uint8_t  Motor_TakeDone(void);          /* 完成标志，读后清零 */
uint8_t  Motor_IsFault(void);           /* FAULT_N 低有效 */
void     Motor_Tick(void);              /* 1ms 后台（保留给硬件级保护） */

/* ---------------- 限位 ---------------- */
void     Limit_Init(void);
uint8_t  Limit_Org(void);               /* 1=原点开关已触发 */
uint8_t  Limit_Pos(void);               /* 1=终点开关已触发 */

/* ---------------- 按键（4 键，10ms 扫描） ---------------- */
#define KEY_ID_UP       0
#define KEY_ID_DN       1
#define KEY_ID_OK       2
#define KEY_ID_BACK     3
#define KEY_ID_NUM      4

#define KEY_EVT_NONE    0
#define KEY_EVT_SHORT   1
#define KEY_EVT_LONG    2

void     Key_Init(void);
void     Key_Scan(void);
uint8_t  Key_GetEvent(uint8_t id);      /* 读后清零 */

/* ---------------- 蜂鸣器（有源，高电平响） ---------------- */
void     Buzzer_Init(void);
void     Buzzer_Beep(uint16_t ms);      /* 单次提示音（非阻塞） */
void     Buzzer_Alarm(uint8_t on);      /* 报警音（循环，直到关闭） */
void     Buzzer_Tick(void);             /* 1ms */

/* ---------------- WS2812 状态灯 ×3 ---------------- */
void     Led_Init(void);
void     Led_SetAll(uint8_t r, uint8_t g, uint8_t b);
void     Led_SetOne(uint8_t index, uint8_t r, uint8_t g, uint8_t b);
void     Led_Show(void);
void     Led_Clear(void);

/* ---------------- SPI 彩屏（ST7789 240x240） ---------------- */
#define LCD_W           240
#define LCD_H           240
#define LCD_BLACK       0x0000
#define LCD_WHITE       0xFFFF
#define LCD_RED         0xF800
#define LCD_GREEN       0x07E0
#define LCD_BLUE        0x001F
#define LCD_YELLOW      0xFFE0
#define LCD_GRAY        0x8410
#define LCD_DARK        0x18E3

void     Lcd_Init(void);
void     Lcd_Backlight(uint8_t on);
void     Lcd_Clear(uint16_t color);
void     Lcd_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void     Lcd_HLine(uint16_t x, uint16_t y, uint16_t w, uint16_t color);
void     Lcd_Text(uint16_t x, uint16_t y, const char *s, uint16_t fg, uint16_t bg, uint8_t scale);
void     Lcd_TextNum(uint16_t x, uint16_t y, int32_t v, uint16_t fg, uint16_t bg, uint8_t scale);

/* ---------------- 串口（USART1=ESP32 上行，USART2=摄像头） ---------------- */
#define UART_CH_ESP32   0
#define UART_CH_CAM     1
#define UART_CH_NUM     2

void     Uart_Init(void);
void     Uart_Write(uint8_t ch, const uint8_t *buf, uint16_t len);
void     Uart_WriteByte(uint8_t ch, uint8_t b);
uint8_t  Uart_Read(uint8_t ch, uint8_t *out);   /* 1=取到 1 字节 */

/* ---------------- 内部 Flash 参数存储 ---------------- */
uint16_t Flash_ParamsCrc(const ActParams *p);   /* CRC16-CCITT 变体（不含 crc 字段） */
uint8_t  Flash_LoadParams(ActParams *p);        /* 1=读到有效参数 */
uint8_t  Flash_SaveParams(const ActParams *p);  /* 1=成功 */

#endif /* __APP_PORT_H */
