#include "app_power.h"
#include "app_port.h"
#include "app_types.h"
#include "app_config.h"

/**
 * ============================================================
 * 业务逻辑层：IP5306 电源管理
 *
 *   IP5306 的关键行为：KEY 每被拉低一次，就在"开"和"关"之间切换；
 *   一旦 5V 建立，PC9(STBY_STAT) 变高，可以据此判断结果。
 *   因此这里统一采用"先看反馈、再决定是否按键"的策略，避免误关。
 * ============================================================
 */

void Pwr_Init(void)
{
    PwrHw_Init();

    /* 上电后如果 5V 还没建立（电池供电冷启动的典型情况），主动开一次 */
    if (!PwrHw_Is5V()) {
        (void)Pwr_Ensure5V();
    }
}

uint8_t Pwr_Is5V(void)
{
    return PwrHw_Is5V();
}

uint8_t Pwr_IsUsb(void)
{
    return PwrHw_IsUsb();
}

uint8_t Pwr_Ensure5V(void)
{
    uint32_t t0;

    if (PwrHw_Is5V()) return 1;

    /* 按一下 KEY，然后等 5V 建立 */
    PwrHw_Key(1);
    t0 = Time_Ms();
    while (!Time_Elapsed(t0, POWER_KEY_PULSE_MS)) { }
    PwrHw_Key(0);

    t0 = Time_Ms();
    while (!Time_Elapsed(t0, POWER_ON_WAIT_MS)) {
        if (PwrHw_Is5V()) return 1;
    }

    return PwrHw_Is5V();
}

uint8_t Pwr_Shutdown5V(void)
{
    uint32_t t0;

    if (!PwrHw_Is5V()) return 1;

    PwrHw_Key(1);
    t0 = Time_Ms();
    while (!Time_Elapsed(t0, POWER_KEY_PULSE_MS)) { }
    PwrHw_Key(0);

    t0 = Time_Ms();
    while (!Time_Elapsed(t0, POWER_OFF_WAIT_MS)) {
        if (!PwrHw_Is5V()) return 1;
    }

    return (uint8_t)(!PwrHw_Is5V());
}
