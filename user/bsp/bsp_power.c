#include "app_port.h"
#include "app_config.h"
#include "pinmap.h"
#include "stm32f10x.h"

/**
 * ============================================================
 * 数据访问层：IP5306 电源按键 / 5V 与 USB 检测 / ESP32 使能
 *
 *   PWR_KEY(PC11) --R1 10k--> Q3(MMBT3904) 基极
 *   Q3: 集电极=GND，发射极=IP5306 KEY(pin5)
 *   => PC11 输出高电平 = 把 KEY 拉低 = "按一下电源键"
 *   5V 状态: PC9(STBY_STAT) 高 = 5V 轨已建立
 *   USB 状态: PC8(CHG_STAT) 高 = USB 插入（VBUS_IN 存在）
 * ============================================================
 */

void PwrHw_Init(void)
{
    GPIO_InitTypeDef g;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC, ENABLE);

    /* PC11: 推挽输出，默认低（不按） */
    g.GPIO_Mode  = GPIO_Mode_Out_PP;
    g.GPIO_Speed = GPIO_Speed_2MHz;
    g.GPIO_Pin   = PWR_KEY_PIN;
    GPIO_Init(PWR_KEY_PORT, &g);
    GPIO_ResetBits(PWR_KEY_PORT, PWR_KEY_PIN);

    /* PA8: ESP32 EN，推挽输出，默认拉高使能模块 */
    g.GPIO_Pin = NET_EN_PIN;
    GPIO_Init(NET_EN_PORT, &g);
    GPIO_SetBits(NET_EN_PORT, NET_EN_PIN);

    /* PC8/PC9: 分压检测输入（外部 100k/100k 已确定电平，无需上下拉） */
    g.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
    g.GPIO_Pin   = STBY_STAT_PIN | CHG_STAT_PIN;
    GPIO_Init(GPIOC, &g);
}

void PwrHw_Key(uint8_t pressed)
{
    if (pressed) GPIO_SetBits(PWR_KEY_PORT, PWR_KEY_PIN);
    else         GPIO_ResetBits(PWR_KEY_PORT, PWR_KEY_PIN);
}

uint8_t PwrHw_Is5V(void)
{
    return (uint8_t)(GPIO_ReadInputDataBit(STBY_STAT_PORT, STBY_STAT_PIN) != 0);
}

uint8_t PwrHw_IsUsb(void)
{
    return (uint8_t)(GPIO_ReadInputDataBit(CHG_STAT_PORT, CHG_STAT_PIN) != 0);
}

void PwrHw_NetEnable(uint8_t on)
{
    if (on) GPIO_SetBits(NET_EN_PORT, NET_EN_PIN);
    else    GPIO_ResetBits(NET_EN_PORT, NET_EN_PIN);
}
