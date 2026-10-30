#ifndef __APP_CONFIG_H
#define __APP_CONFIG_H

/* ================= 系统常量（可按验收调整） ================= */

#define FW_VERSION            "V0.1"

/* ================= 功能开关（硬件未就绪的功能先禁用） =================
 * 1) 电池电量检测: 原理图 BAT_ADC 分压网络没有接到任何 MCU 引脚
 *    (PA0 空置)，暂禁用，待硬件补线到 PA0 后开启
 * 2) 外部 W25Q64 存储: FLASH_CS 只接了上拉电阻，没有接到 MCU，
 *    暂禁用；服药计划改用内部 Flash 兜底
 *    后续硬件补好 FLASH_CS 线后，把下面宏改回 1 即可 */
#define FEATURE_BAT_ADC       0
#define FEATURE_EXT_FLASH     0

/* 出药机构 */
#define MOTOR_TIMEOUT_MS      5000    /* 出药超时: 超过此时间未到限位则报错 */
#define MOTOR_STEP_FREQ_HZ    800     /* STEP 脉冲频率 */
#define MOTOR_STEPS_PER_PILL  800     /* 每粒药对应的步进脉冲数(待按机构标定) */
/* 驱动器 EN 有效电平: 原理图 U4.3 EN 有 R24 10k 下拉，常见驱动(DRV8825/A4988/TMC)
 * 均为低有效，故默认 1。若实测“使能时应为高电平”，把此宏改成 0。 */
#define MOTOR_EN_ACTIVE_LOW   1

/* 复检机制 (出药后等待用户取药；可用 ESP32 的 CMD_DETECT_RESULT 提前结束) */
#define RECHECK_MAX_TIMES     3       /* 最多复检/催促次数, 超过判漏服 */
#define RECHECK_INTERVAL_S    60      /* 复检/催促间隔(秒) */

/* 提醒 */
#define REMIND_BEEP_TIMES     3       /* 提醒轮数: 首次 + 重复提醒的次数, 用完仍未确认判漏服 */
#define REMIND_RETRY_S        120     /* 未确认时重复提醒间隔(秒) */

/* 服药计划容量 */
#define SCHEDULE_MAX          8       /* 最多 8 条服药计划 */

/* 串口 */
#define ESP32_BAUDRATE        115200
#define RX_RING_SIZE          256     /* ESP32 接收环形缓冲区大小(2的幂) */

/* WS2812 */
#define WS2812_NUM            3       /* 彩灯数量 */

#endif /* __APP_CONFIG_H */
