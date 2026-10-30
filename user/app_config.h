#ifndef __APP_CONFIG_H
#define __APP_CONFIG_H

/* ================= 系统常量（按需求文档，可按验收调整） ================= */

#define FW_VERSION            "V0.1"

/* ================= 功能开关（硬件未就绪的功能先禁用） =================
 * 1) 电池电量检测: 原理图 BAT_ADC 未接 MCU(PA0 空置)，暂禁用，待硬件补线后开启
 * 2) 外部 W25Q64 存储: FLASH_CS 未接 MCU，暂禁用；服药计划改用内部 Flash 兜底
 *    后续硬件补好 FLASH_CS 线后，把下面宏改回 1 即可 */
#define FEATURE_BAT_ADC       0
#define FEATURE_EXT_FLASH     0

/* 出药机构 */
#define MOTOR_TIMEOUT_MS      5000    /* 出药超时: 超过此时间未到限位则报错 (M3 要求) */
#define MOTOR_STEP_FREQ_HZ    800     /* STEP 脉冲频率 */

/* 复检机制 (拍照/检测确认药片是否取走) */
#define RECHECK_MAX_TIMES     3       /* 最多复检次数, 超过判漏服 */
#define RECHECK_INTERVAL_S    60      /* 复检间隔(秒) */

/* 提醒 */
#define REMIND_BEEP_TIMES     3       /* 每轮提醒蜂鸣次数 */
#define REMIND_RETRY_S        120     /* 未确认时重复提醒间隔(秒) */

/* 服药计划容量 */
#define SCHEDULE_MAX          8       /* 最多 8 条服药计划 */

/* 串口 */
#define ESP32_BAUDRATE        115200
#define RX_RING_SIZE          256     /* ESP32 接收环形缓冲区大小(2的幂) */

/* WS2812 */
#define WS2812_NUM            3       /* 彩灯数量 */

#endif /* __APP_CONFIG_H */
