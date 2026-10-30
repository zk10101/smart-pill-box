#include "stm32f10x.h"
#include "Delay.h"

/**
 * 说明：
 *   原实现直接改写 SysTick->LOAD/VAL/CTRL 做延时，会把 main.c 里
 *   SysTick_Config(1ms) 配置的节拍彻底破坏（延时结束后系统节拍停止，
 *   g_ms 不再增长，整个状态机/按键/电机超时都会失效）。
 *   这里改用 Cortex-M3 内核的 DWT 周期计数器，不碰 SysTick，安全可靠。
 *
 *   DWT->CYCCNT 需要先使能 CoreDebug->DEMCR 的 TRCENA 位。
 */
#define DEMCR_TRCENA        (1UL << 24)
#define DEMCR_REG           (*(volatile uint32_t *)0xE000EDFC)
#define DWT_CTRL_REG        (*(volatile uint32_t *)0xE0001000)
#define DWT_CYCCNT_REG      (*(volatile uint32_t *)0xE0001004)
#define DWT_CTRL_CYCCNTENA  (1UL << 0)

static uint8_t s_dwt_ready = 0;

void Delay_Init(void)
{
    if (s_dwt_ready) return;

    DEMCR_REG |= DEMCR_TRCENA;      /* 使能跟踪单元 */
    DWT_CYCCNT_REG = 0;             /* 计数器清零 */
    DWT_CTRL_REG |= DWT_CTRL_CYCCNTENA;   /* 启动周期计数 */
    s_dwt_ready = 1;
}

/**
  * @brief  微秒级延时（基于 DWT 周期计数，不占用/不破坏 SysTick）
  * @param  xus 延时时长（us）
  */
void Delay_us(uint32_t xus)
{
    uint32_t start, wait;

    Delay_Init();

    wait  = xus * (SystemCoreClock / 1000000UL);
    if (wait == 0) return;

    start = DWT_CYCCNT_REG;
    while ((DWT_CYCCNT_REG - start) < wait);   /* 32 位差值，自动处理回绕 */
}

/**
  * @brief  毫秒级延时（基于 DWT，不阻塞中断，仅忙等 CPU）
  * @param  xms 延时时长（ms）
  */
void Delay_ms(uint32_t xms)
{
    while (xms--) {
        Delay_us(1000);
    }
}

/**
  * @brief  秒级延时
  * @param  xs 延时时长（s）
  */
void Delay_s(uint32_t xs)
{
    while (xs--) {
        Delay_ms(1000);
    }
}
