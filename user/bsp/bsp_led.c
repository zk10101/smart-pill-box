#include "app_port.h"
#include "app_config.h"
#include "pinmap.h"
#include "stm32f10x.h"

/**
 * ============================================================
 * 数据访问层：WS2812B 状态灯 ×3（PA11 单线，GRB）
 *   直接写 BSRR/BRR 寄存器把 GPIO 操作压缩到 2~3 个周期，
 *   再用 DWT 周期计数器拼出 0 码/1 码时序。
 *   若实测颜色错乱（3V3 供电偏暗时可能出现），
 *   可把 WS_T0H/WS_T1H 微调（单位：72MHz 周期）。
 * ============================================================
 */

#define DEMCR_REG       (*(volatile uint32_t *)0xE000EDFCUL)
#define DWT_CTRL_REG    (*(volatile uint32_t *)0xE0001000UL)
#define DWT_CYCCNT_REG  (*(volatile uint32_t *)0xE0001004UL)

#define WS_T0H          26U     /* ~0.36us 高电平 */
#define WS_T0L          64U     /* ~0.89us 低电平 */
#define WS_T1H          58U     /* ~0.81us 高电平 */
#define WS_T1L          32U     /* ~0.44us 低电平 */
#define WS_RESET        6000U   /* >50us 复位 */

#define WS_NUM          3U

static uint8_t s_buf[WS_NUM * 3];   /* GRB */

static void ws_dwt_init(void)
{
    DEMCR_REG     |= (1UL << 24);
    DWT_CYCCNT_REG = 0;
    DWT_CTRL_REG  |= 1UL;
}

static void ws_delay(uint32_t cycles)
{
    uint32_t start = DWT_CYCCNT_REG;
    while ((DWT_CYCCNT_REG - start) < cycles) { }
}

static void ws_send_byte(uint8_t b)
{
    uint8_t i;

    for (i = 0; i < 8U; i++) {
        if (b & 0x80U) {
            GPIOA->BSRR = WS2812_PIN;
            ws_delay(WS_T1H);
            GPIOA->BRR  = WS2812_PIN;
            ws_delay(WS_T1L);
        } else {
            GPIOA->BSRR = WS2812_PIN;
            ws_delay(WS_T0H);
            GPIOA->BRR  = WS2812_PIN;
            ws_delay(WS_T0L);
        }
        b <<= 1;
    }
}

void Led_Init(void)
{
    GPIO_InitTypeDef g;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    g.GPIO_Mode  = GPIO_Mode_Out_PP;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    g.GPIO_Pin   = WS2812_PIN;
    GPIO_Init(WS2812_PORT, &g);

    GPIO_ResetBits(WS2812_PORT, WS2812_PIN);
    ws_dwt_init();
    Led_Clear();
}

void Led_SetOne(uint8_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (index >= WS_NUM) return;
    s_buf[index * 3 + 0] = g;   /* WS2812 数据顺序为 G-R-B */
    s_buf[index * 3 + 1] = r;
    s_buf[index * 3 + 2] = b;
}

void Led_SetAll(uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t i;

    for (i = 0; i < WS_NUM; i++) Led_SetOne(i, r, g, b);
}

void Led_Show(void)
{
    uint16_t i;

    __disable_irq();
    for (i = 0; i < WS_NUM * 3U; i++) ws_send_byte(s_buf[i]);
    GPIOA->BRR = WS2812_PIN;
    ws_delay(WS_RESET);
    __enable_irq();
}

void Led_Clear(void)
{
    Led_SetAll(0, 0, 0);
    Led_Show();
}
