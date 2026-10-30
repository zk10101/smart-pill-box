#include "state_machine.h"
#include "app_config.h"
#include "bsp_motor.h"
#include "bsp_buzzer.h"
#include "bsp_ws2812.h"
#include "bsp_esp32.h"
#include "bsp_storage.h"
#include <string.h>

static SM_State sm_state = SM_IDLE;

/* 服药计划表（持久化在内部 Flash 最后一页） */
static ScheduleItem sched[SCHEDULE_MAX];
static uint8_t  sched_num = 0;

/* ---- 状态机内部计时（全部基于 1ms 节拍的 ms 计数，不阻塞主循环） ---- */
static volatile uint32_t sm_ms = 0;         /* 由 SM_Tick() 递增 */
static uint32_t phase_ms = 0;               /* 当前阶段起点 */
static uint8_t  remind_round = 0;           /* 已重复提醒次数 */
static uint8_t  recheck_round = 0;          /* 已复检/催促次数 */
static uint16_t last_minute_key = 0xFFFF;   /* 上一次判断过的“时*60+分”，防止同一分钟重复触发 */
static uint32_t dispense_steps = MOTOR_STEPS_PER_PILL;

static void SM_Enter(SM_State s)
{
    sm_state = s;
    phase_ms = sm_ms;
}

void SM_Tick(void)
{
    sm_ms++;
}

void SM_Init(void)
{
    sm_state = SM_IDLE;
    phase_ms = 0;
    remind_round = 0;
    recheck_round = 0;
    last_minute_key = 0xFFFF;
    dispense_steps = MOTOR_STEPS_PER_PILL;

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
    uint8_t i;

    if (num > SCHEDULE_MAX) return;
    for (i = 0; i < num; i++) {
        if (items[i].hour > 23 || items[i].minute > 59) return;   /* 非法时间拒收 */
    }
    memcpy(sched, items, num * sizeof(ScheduleItem));
    sched_num = num;
    Storage_SaveSchedule(sched, sched_num);
}

/* 查找当前时间命中的计划；返回索引，未命中返回 -1。
 * 同时把该计划的出药步数换算好。 */
static int8_t SM_FindSchedule(uint8_t hour, uint8_t minute)
{
    uint8_t i, n;

    for (i = 0; i < sched_num; i++) {
        if (sched[i].enabled && sched[i].hour == hour && sched[i].minute == minute) {
            n = sched[i].pill_count ? sched[i].pill_count : 1;
            dispense_steps = (uint32_t)n * MOTOR_STEPS_PER_PILL;
            return (int8_t)i;
        }
    }
    return -1;
}

void SM_Run(uint8_t hour, uint8_t minute, uint8_t sec)
{
    uint16_t mkey = (uint16_t)((uint16_t)hour * 60u + (uint16_t)minute);
    (void)sec;

    switch (sm_state) {
    case SM_IDLE:
        /* 每分钟只判断一次：确认取药回到 IDLE 后，同一分钟内不会再触发同一计划 */
        if (mkey != last_minute_key) {
            last_minute_key = mkey;
            if (SM_FindSchedule(hour, minute) >= 0) {
                remind_round = 0;
                SM_Enter(SM_REMIND);
                Buzzer_Beep(1000);                  /* 首轮提醒 */
                WS2812_SetPixel(0, 255, 0, 0);      /* 红灯提醒 */
                WS2812_Show();
            }
        }
        break;

    case SM_REMIND:
        /* 等待用户确认；超过重复间隔仍未确认，则重复提醒，轮数用完判漏服 */
        if (sm_ms - phase_ms >= (uint32_t)REMIND_RETRY_S * 1000u) {
            if ((uint8_t)(remind_round + 1u) >= REMIND_BEEP_TIMES) {
                Buzzer_Alarm();
                SM_Enter(SM_ALARM);
            } else {
                remind_round++;
                phase_ms = sm_ms;
                Buzzer_Beep(1000);
            }
        }
        break;

    case SM_DISPENSE:
        /* 自动轮询电机状态，出药结束即进入复检（不再依赖按键触发） */
        switch (Motor_GetStatus()) {
        case MOTOR_DONE:
            recheck_round = 0;
            SM_Enter(SM_CHECK);
            WS2812_SetPixel(0, 0, 0, 255);          /* 蓝灯：请取药 */
            WS2812_Show();
            break;
        case MOTOR_FAULT:
        case MOTOR_TIMEOUT:
            Buzzer_Alarm();                         /* 出药异常 -> 告警 */
            SM_Enter(SM_ALARM);
            break;
        default:
            break;
        }
        break;

    case SM_CHECK:
        /* 无视觉模块时，用“催促 + 超时”判定漏服；收到 CMD_DETECT_RESULT 可提前结束 */
        if (sm_ms - phase_ms >= (uint32_t)RECHECK_INTERVAL_S * 1000u) {
            phase_ms = sm_ms;
            if ((uint8_t)(recheck_round + 1u) >= RECHECK_MAX_TIMES) {
                Buzzer_Alarm();
                SM_Enter(SM_ALARM);
            } else {
                recheck_round++;
                Buzzer_Beep(300);                   /* 催促取药 */
                WS2812_SetPixel(0, 255, 0, 0);
                WS2812_Show();
            }
        }
        break;

    case SM_ALARM:
        Buzzer_Alarm();
        /* TODO: 通过 ESP32 上报告警 + 日志 */
        break;

    default:
        SM_Enter(SM_IDLE);
        break;
    }
}

void SM_OnKeyConfirm(void)
{
    switch (sm_state) {
    case SM_REMIND:
        /* 开始出药 */
        Buzzer_Stop();
        SM_Enter(SM_DISPENSE);
        if (Motor_Dispense(dispense_steps) == MOTOR_BUSY) {
            SM_Enter(SM_ALARM);         /* 电机忙（异常），转入告警 */
        }
        break;

    case SM_CHECK:
        /* 用户确认已取药 */
        SM_Enter(SM_IDLE);
        WS2812_Clear();
        Buzzer_Stop();
        break;

    case SM_ALARM:
        /* 告警确认，复位 */
        SM_Enter(SM_IDLE);
        WS2812_Clear();
        Buzzer_Stop();
        break;

    default:
        break;
    }
}

/* ESP32 视觉/传感器回传“已取药” */
void SM_OnPillTaken(void)
{
    if (sm_state == SM_CHECK) {
        SM_Enter(SM_IDLE);
        WS2812_Clear();
        Buzzer_Stop();
    }
}

void SM_OnKeyMute(void)
{
    Buzzer_Stop();      /* 静音，但不改变状态 */
}

SM_State SM_GetState(void) { return sm_state; }
