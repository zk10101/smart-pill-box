#ifndef __APP_TYPES_H
#define __APP_TYPES_H
/**
 * ============================================================
 * 跨层共享的数据类型（无任何硬件/寄存器依赖）
 * 表现层、业务层都可包含本文件。
 * ============================================================
 */
#include <stdint.h>

/* ---------------- 执行机构状态机 ---------------- */
typedef enum {
    ACT_ST_IDLE = 0,        /* 空闲 */
    ACT_ST_HOMING,          /* 旧协议保留值，本期不使用 */
    ACT_ST_MOVING,          /* 转盘移动中 */
    ACT_ST_ERROR            /* 故障保持（需 CLEAR_ERROR 或断电） */
} ActState;

/* ---------------- 错误码（对外上报） ---------------- */
typedef enum {
    ACT_ERR_NONE = 0,
    ACT_ERR_FAULT,          /* 驱动器 FAULT_N 有效 */
    ACT_ERR_TIMEOUT,        /* 运动超时未到位 */
    ACT_ERR_NOT_HOMED,      /* 旧协议保留值 */
    ACT_ERR_NO_LIMIT,       /* 旧协议保留值 */
    ACT_ERR_5V_OFF,         /* 5V 未建立（无法驱动电机/驱动器） */
    ACT_ERR_BUSY,           /* 正在执行其他动作 */
    ACT_ERR_PARAM,          /* 参数非法 */
    ACT_ERR_LIMIT_STOP      /* 行程终点限位触发导致的停止 */
} ActError;

/* ---------------- 事件（日志 / 主动上报） ---------------- */
typedef enum {
    EVT_NONE = 0,
    EVT_POWER_ON,           /* 上电初始化完成 */
    EVT_PWR5V_ON,
    EVT_PWR5V_OFF,
    EVT_HOME_START,
    EVT_HOME_DONE,
    EVT_HOME_FAIL,
    EVT_MOVE_START,
    EVT_MOVE_DONE,
    EVT_MOVE_ABORT,
    EVT_FAULT,
    EVT_TIMEOUT,
    EVT_CMD_DISPENSE,
    EVT_CMD_STOP,
    EVT_CMD_HOME,
    EVT_KEY,                /* a=按键号 b=事件类型 */
    EVT_CAM_RESULT,         /* a=found b=count */
    EVT_PARAM_SAVED
} AppEvent;

/* ---------------- 运动参数（可在运行期设置并掉电保存） ---------------- */
#pragma pack(1)
typedef struct {
    uint16_t steps_per_slot;
    uint16_t speed_hz;
    uint8_t  en_active_low;     /* 1: EN 低有效，0: 高有效 */
    uint8_t  dir_invert;        /* 1: 方向取反 */
    uint16_t home_backoff;      /* 保留字段，本期固定为 0 */
    uint16_t move_timeout_10ms; /* 运动超时，单位 10ms */
    uint16_t crc;               /* 简单校验（Flash 存储用） */
} ActParams;
#pragma pack()

/* ---------------- 日志条目 ---------------- */
typedef struct {
    uint8_t  ev;
    uint8_t  a;
    uint8_t  b;
    uint16_t t_ms;              /* 事件时刻低 16 位（毫秒） */
} LogEntry;

/* ---------------- 对外状态快照（GET_STATUS 返回值） ---------------- */
typedef struct {
    uint8_t  state;             /* ActState */
    uint8_t  error;             /* ActError */
    uint8_t  flags;             /* bit0:5V bit1:USB bit2/3:限位 bit4:运行 bit5:格号有效 */
    uint8_t  current_slot;      /* 0=未知；1..12 */
    uint8_t  target_slot;       /* 0=无目标；1..12 */
    uint8_t  slot_valid;        /* 上次动作完整结束后的预测位置 */
    uint32_t position;          /* 本次动作完成的累计脉冲 */
    uint16_t steps_per_slot;
    uint16_t speed_hz;
} ActStatus;

/* ---------------- 日志接口（业务层实现） ---------------- */
void    AppLog_Push(uint8_t ev, uint8_t a, uint8_t b);
uint8_t AppLog_Count(void);
uint8_t AppLog_GetNewest(uint8_t index, LogEntry *out);   /* index=0 最新 */

#endif /* __APP_TYPES_H */
