#include "bsp_limit.h"
#include "pinmap.h"
#include "stm32f10x.h"

void Limit_Init(void)
{
    GPIO_InitTypeDef g;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    g.GPIO_Mode  = GPIO_Mode_IPU;           /* 上拉输入，触发为低 */
    g.GPIO_Speed = GPIO_Speed_50MHz;
    g.GPIO_Pin   = LIM_ORG_PIN | LIM_POS_PIN;
    GPIO_Init(LIM_ORG_PORT, &g);
}

uint8_t Limit_Org_Tripped(void)
{
    return (uint8_t)(GPIO_ReadInputDataBit(LIM_ORG_PORT, LIM_ORG_PIN) == 0);
}

uint8_t Limit_Pos_Tripped(void)
{
    return (uint8_t)(GPIO_ReadInputDataBit(LIM_POS_PORT, LIM_POS_PIN) == 0);
}
