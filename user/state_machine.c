#include "state_machine.h"
#include "app_config.h"
#include "bsp_motor.h"
#include "bsp_buzzer.h"
#include "bsp_ws2812.h"
#include "bsp_esp32.h"
#include "bsp_storage.h"
#include <string.h>

static SM_State sm_state = SM_IDLE;

/* 服药计划表（后续写入 W25Q 持久化，此处先用静态示例） */
static ScheduleItem sched[SCHEDULE_MAX];
static uint8_t  sched_num = 0;

static uint8_t  dispense_done;      /* 出药完成标志 */
static uint8_t  recheck_cnt;        /* 复检计数 */
static uint32_t remind_ts;          /* 提醒时间戳(秒) */

void SM_Init(void)
{
    sm_state = SM_IDLE;
    dispense_done = 0;
    recheck_cnt = 0;
    memset(sched, 0, sizeof(sched));
    sched_num = 0;

    /* 优先从内部 Flash 加载已保存的服药计划 */
    if (!Storage_LoadSchedule(sched, &sched_num)) {
        /* 首次上电：写入默认示例计划（08:00 / 19:30 各 1 粒） */
        sched[0].enabled = 1; sched[0].hour = 8;  sched[0].minute = 0;  sched[0].pill_count = 1;
        sched[1].enabled = 1; sched[1].hour = 19; sched[1].minute = 30; sched[1].pill_count = 1;
        sched_num = 2;
        Storage_SaveSchedule(sched, sched_num);
    }
}

/* APP/设置修改服药计划后调用，同步保存到内部 Flash */
void SM_SaveSchedule(const ScheduleItem *items, uint8_t num)
{
    if (num > SCHEDULE_MAX) return;
    memcpy(sched, items, num * sizeof(ScheduleItem));
    sched_num = num;
    Storage_SaveSchedule(sched, sched_num);
}

static uint8_t SM_CheckSchedule(uint8_t hour, uint8_t minute, uint8_t sec)
{
    uint8_t i;
    (void)sec;
    for (i = 0; i < sched_num; i++) {
        if (sched[i].enabled && sched[i].hour == hour && sched[i].minute == minute) {
            return 1;
        }
    }
    return 0;
}

void SM_Run(uint8_t hour, uint8_t minute, uint8_t sec)
{
    switch (sm_state) {
    case SM_IDLE:
        if (SM_CheckSchedule(hour, minute, sec)) {
            sm_state = SM_REMIND;
            Buzzer_Beep(1000);
            WS2812_SetPixel(0, 255, 0, 0);   /* 红灯提醒 */
            WS2812_Show();
        }
        break;

    case SM_REMIND:
        /* 等待用户确认 -> 出药 */
        /* 触发由 SM_OnKeyConfirm() 注入 */
        break;

    case SM_DISPENSE:
        if (dispense_done) {
            sm_state = SM_CHECK;
            recheck_cnt = 0;
            WS2812_SetPixel(0, 0, 0, 255);   /* 蓝灯：请取药 */
            WS2812_Show();
        }
        break;

    case SM_CHECK:
        /* 复检：实际用视觉/传感器判定，此处骨架用按键确认模拟 */
        if (recheck_cnt >= RECHECK_MAX_TIMES) {
            sm_state = SM_ALARM;             /* 判定漏服 */
        }
        break;

    case SM_ALARM:
        Buzzer_Alarm();
        /* TODO: 通过 ESP32 上报告警 + 日志 */
        break;
    }
}

void SM_OnKeyConfirm(void)
{
    if (sm_state == SM_REMIND) {
        /* 开始出药 */
        dispense_done = 0;
        sm_state = SM_DISPENSE;
        Buzzer_Stop();
        Motor_Dispense(800);        /* 示例步数，实际按药仓结构标定 */
    } else if (sm_state == SM_DISPENSE) {
        MotorStatus st = Motor_GetStatus();
        if (st == MOTOR_DONE || st == MOTOR_TIMEOUT || st == MOTOR_FAULT) {
            dispense_done = 1;
            if (st == MOTOR_TIMEOUT || st == MOTOR_FAULT) {
                sm_state = SM_ALARM;    /* 出药异常 -> 告警 */
            }
        }
    } else if (sm_state == SM_CHECK) {
        sm_state = SM_IDLE;             /* 用户确认已取药 */
        WS2812_Clear();
        Buzzer_Stop();
    } else if (sm_state == SM_ALARM) {
        sm_state = SM_IDLE;             /* 告警确认，复位 */
        WS2812_Clear();
        Buzzer_Stop();
    }
}

void SM_OnKeyMute(void)
{
    Buzzer_Stop();      /* 静音，但不改变状态 */
}

SM_State SM_GetState(void) { return sm_state; }
