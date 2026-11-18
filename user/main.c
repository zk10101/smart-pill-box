#include "stm32f10x.h"
#include "app_config.h"
#include "app_port.h"
#include "app_types.h"
#include "app_actuator.h"
#include "app_power.h"
#include "app_ui.h"
#include "app_command.h"

/**
 * ============================================================
 * 平台启动层（main）
 *
 *   只做三件事：
 *     1) 按依赖顺序初始化各层（数据访问 -> 业务 -> 表现）
 *     2) 建立 1ms / 10ms / 50ms / 100ms 的时间片调度
 *     3) 喂看门狗，并把"业务状态变化"作为事件上报给上位机
 *
 *   本文件不直接操作外设寄存器（IWDG 除外，它属于平台守护）。
 * ============================================================
 */

static void iwdg_init(void)
{
    /* LSI ≈ 40kHz，64 分频 => 625Hz，重装值 2500 => 约 4s 超时 */
    RCC_LSICmd(ENABLE);
    while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET) {
    }

    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
    IWDG_SetPrescaler(IWDG_Prescaler_64);
    IWDG_SetReload(IWDG_RELOAD_VALUE);
    IWDG_ReloadCounter();
    IWDG_Enable();
}

/* 上报用的状态快照 */
static uint8_t  s_evt_state;
static uint8_t  s_evt_error;
static uint8_t  s_evt_slot;

static void event_poll(void)
{
    ActStatus st;

    Act_GetStatus(&st);
    if (st.state != s_evt_state ||
        st.error != s_evt_error ||
        st.current_slot != s_evt_slot) {
        if (s_evt_state == ACT_ST_MOVING && st.state == ACT_ST_IDLE && st.slot_valid) {
            Cmd_ReportEvent(EVT_MOVE_DONE, st.current_slot, 0);
        }
        s_evt_state = st.state;
        s_evt_error = st.error;
        s_evt_slot = st.current_slot;
        Cmd_ReportEvent((uint8_t)(0xE0 | st.state), st.error, st.current_slot);
    }
}

int main(void)
{
    uint32_t t_1ms;
    uint32_t t_10ms;
    uint32_t t_50ms;
    uint32_t t_100ms;

    /* ---------- 数据访问层 ---------- */
    Sys_Init();          /* 时钟确认、AFIO、释放 JTAG、1ms SysTick */
    iwdg_init();

    Motor_Init();
    Limit_Init();
    Key_Init();
    Buzzer_Init();
    Led_Init();
    Lcd_Init();
    PwrHw_Init();

    /* ---------- 业务逻辑层 ---------- */
    Act_Init();
    Pwr_Init();

    /* ---------- 表现层 ---------- */
    Cmd_Init();          /* 内部完成 Uart_Init */
    Ui_Init();

    Buzzer_Beep(80);

    s_evt_state = 0xFF;
    s_evt_error = 0xFF;
    s_evt_slot = 0xFF;

    t_1ms   = Time_Ms();
    t_10ms  = t_1ms;
    t_50ms  = t_1ms;
    t_100ms = t_1ms;

    while (1) {
        IWDG_ReloadCounter();

        /* 串口收包要尽量勤，避免 128 字节环形缓冲被灌满 */
        Cmd_Tick();

        if (Time_Elapsed(t_1ms, 1)) {
            t_1ms = Time_Ms();
            Buzzer_Tick();
            Act_Tick();
        }

        if (Time_Elapsed(t_10ms, 10)) {
            t_10ms += 10;
            Ui_Tick();
        }

        if (Time_Elapsed(t_50ms, 50)) {
            t_50ms += 50;
            event_poll();
        }

        if (Time_Elapsed(t_100ms, 100)) {
            t_100ms += 100;
            Ui_Refresh();
        }
    }
}
