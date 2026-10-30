#include "bsp_ws2812.h"
#include "pinmap.h"
#include "app_config.h"
#include "stm32f10x.h"

static uint8_t ws_buf[WS2812_NUM * 3];  /* GRB */

/* 使能 DWT 作微秒级延时（72MHz 主频下每周期约 1/72us）
 * 注：老版 core_cm3.h 未提供 DWT 结构体定义，这里直接按寄存器地址访问 */
#define DEMCR_ADDR      0xE000EDFCUL    /* Debug Exception and Monitor Control */
#define DWT_CTRL_ADDR   0xE0001000UL    /* DWT Control */
#define DWT_CYCCNT_ADDR 0xE0001004UL    /* DWT Cycle Counter */

static void DWT_Init(void)
{
    *(volatile uint32_t *)DEMCR_ADDR      |= (1UL << 24);   /* TRCENA */
    *(volatile uint32_t *)DWT_CYCCNT_ADDR  = 0;
    *(volatile uint32_t *)DWT_CTRL_ADDR   |= (1UL << 0);    /* CYCCNTENA */
}
static void DWT_Delay(uint32_t cycles)  /* 等待 cycles 个系统周期 */
{
    volatile uint32_t start = *(volatile uint32_t *)DWT_CYCCNT_ADDR;
    while ((*(volatile uint32_t *)DWT_CYCCNT_ADDR - start) < cycles) { }
}

void WS2812_Init(void)
{
    GPIO_InitTypeDef g;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    g.GPIO_Mode  = GPIO_Mode_Out_PP;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    g.GPIO_Pin   = WS2812_PIN;
    GPIO_Init(WS2812_PORT, &g);
    GPIO_ResetBits(WS2812_PORT, WS2812_PIN);
    DWT_Init();
    WS2812_Clear();
}

void WS2812_SetPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (index >= WS2812_NUM) return;
    ws_buf[index * 3 + 0] = g;  /* GRB 顺序 */
    ws_buf[index * 3 + 1] = r;
    ws_buf[index * 3 + 2] = b;
}

/* 发送一个字节 (MSB first)；1.25us/bit，高电平: 0码~0.4us, 1码~0.8us */
static void WS2812_SendByte(uint8_t byte)
{
    uint8_t i;
    for (i = 0; i < 8; i++) {
        if (byte & 0x80) {
            GPIO_SetBits(WS2812_PORT, WS2812_PIN);
            DWT_Delay(58);          /* ~0.8us 高 */
            GPIO_ResetBits(WS2812_PORT, WS2812_PIN);
            DWT_Delay(32);          /* ~0.45us 低 */
        } else {
            GPIO_SetBits(WS2812_PORT, WS2812_PIN);
            DWT_Delay(29);          /* ~0.4us 高 */
            GPIO_ResetBits(WS2812_PORT, WS2812_PIN);
            DWT_Delay(61);          /* ~0.85us 低 */
        }
        byte <<= 1;
    }
}

void WS2812_Show(void)
{
    uint16_t i;
    __disable_irq();                /* 关闭中断保证时序 */
    for (i = 0; i < WS2812_NUM * 3; i++) {
        WS2812_SendByte(ws_buf[i]);
    }
    GPIO_ResetBits(WS2812_PORT, WS2812_PIN);
    DWT_Delay(4000);                /* >50us 复位 */
    __enable_irq();
}

void WS2812_Clear(void)
{
    uint16_t i;
    for (i = 0; i < WS2812_NUM; i++) WS2812_SetPixel(i, 0, 0, 0);
    WS2812_Show();
}
