#include "app_port.h"
#include "app_config.h"
#include "pinmap.h"
#include "stm32f10x.h"

/**
 * ============================================================
 * 数据访问层：限位开关
 *   CN2 LIM_ORG -> R25 100R -> PC6，R27 10k 上拉 3V3，C27 100nF
 *   CN3 LIM_POS -> R26 100R -> PC7，R28 10k 上拉 3V3，C28 100nF
 *   开关闭合到 GND => 低电平 = 触发
 * ============================================================
 */

void Limit_Init(void)
{
    GPIO_InitTypeDef g;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    g.GPIO_Mode  = GPIO_Mode_IPU;           /* 内部再上拉一次，外部已有 10k */
    g.GPIO_Speed = GPIO_Speed_2MHz;
    g.GPIO_Pin   = LIM_ORG_PIN | LIM_POS_PIN;
    GPIO_Init(GPIOC, &g);
}

uint8_t Limit_Org(void)
{
    return (uint8_t)(GPIO_ReadInputDataBit(LIM_ORG_PORT, LIM_ORG_PIN) == 0);
}

uint8_t Limit_Pos(void)
{
    return (uint8_t)(GPIO_ReadInputDataBit(LIM_POS_PORT, LIM_POS_PIN) == 0);
}
