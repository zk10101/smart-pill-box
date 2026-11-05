#include "app_port.h"
#include "app_config.h"
#include "stm32f10x.h"

/**
 * ============================================================
 * 数据访问层：系统时钟 / 1ms 节拍 / 启动原因
 * ============================================================
 */

static volatile uint32_t s_ms;      /* 上电以来的毫秒计数（SysTick 递增） */
static uint8_t s_boot_flag;

/* 唯一一处 SysTick 处理函数（stm32f10x_it.c 中的同名函数已注释） */
void SysTick_Handler(void)
{
    s_ms++;
}

void Sys_Init(void)
{
    GPIO_InitTypeDef g;

    /* 1) 只使用 SWD 调试，释放 PA15/PB3(JTAG) 复用给普通 GPIO */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);

    /* 2) 记录/清除复位原因：POR 视为"正常上电"，其余视为复位 */
    s_boot_flag = (uint8_t)(RCC_GetFlagStatus(RCC_FLAG_PORRST) == SET);
    RCC_ClearFlag();

    /* 3) 中断优先级分组：2 位抢占 + 2 位子优先级 */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    /* 4) 1ms 系统节拍（SysTick 优先级设为最低，避免打断串口/电机中断） */
    (void)SysTick_Config(SystemCoreClock / 1000U);
    NVIC_SetPriority(SysTick_IRQn, 15);

    /* 5) 打开本板用到的 GPIO 端口时钟（A/B/C 三个端口覆盖全部外设脚） */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB |
                           RCC_APB2Periph_GPIOC, ENABLE);
    (void)g;

    s_ms = 0;
}

uint32_t Time_Ms(void)
{
    return s_ms;
}

uint8_t Time_Elapsed(uint32_t since, uint32_t ms)
{
    return (uint8_t)((s_ms - since) >= ms);
}

uint8_t Sys_TakeBootFlag(void)
{
    return s_boot_flag;
}
