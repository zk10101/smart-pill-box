#ifndef __STATE_MACHINE_H
#define __STATE_MACHINE_H
#include <stdint.h>

/* 主控五态状态机（验收项 M2/M3/M4 核心） */
typedef enum {
    SM_IDLE = 0,        /* 待机，等待服药时间 */
    SM_REMIND,          /* 提醒服药（蜂鸣+彩灯+屏幕） */
    SM_DISPENSE,        /* 出药（步进电机） */
    SM_CHECK,           /* 复检（确认是否取药） */
    SM_ALARM             /* 告警（漏服/异常，上报云端） */
} SM_State;

/* 服药计划条目 */
typedef struct {
    uint8_t enabled;
    uint8_t hour;
    uint8_t minute;
    uint8_t pill_count;     /* 出药颗粒数(换算为步进步数) */
} ScheduleItem;

void SM_Init(void);
/* 主循环周期调用：需外部提供当前 RTC 时间（时/分/秒） */
void SM_Run(uint8_t hour, uint8_t minute, uint8_t sec);
/* 更新服药计划并持久化到内部 Flash */
void SM_SaveSchedule(const ScheduleItem *items, uint8_t num);
/* 用户按键事件注入 */
void SM_OnKeyConfirm(void);     /* 用户确认(已取药) */
void SM_OnKeyMute(void);        /* 静音 */
/* 获取当前状态 */
SM_State SM_GetState(void);

#endif
