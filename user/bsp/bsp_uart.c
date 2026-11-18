#include "app_port.h"
#include "app_config.h"
#include "pinmap.h"
#include "stm32f10x.h"

/**
 * ============================================================
 * 数据访问层：两路串口
 *
 *   USART1 (115200-8N1)：ESP32-C3 上行链路
 *       TX=PA9 -> 1k -> ESP32 RXD0 ; RX=PA10 <- 1k <- ESP32 TXD0 ; EN=PA8
 *   USART2 (115200-8N1)：摄像头 MaixBit(K210) 链路（H1 接口）
 *       TX=PA2 -> 1k -> H1.2 ; RX=PA3 <- 1k <- H1.3
 *
 *   接收走中断 + 环形缓冲，读写都不阻塞业务层；
 *   发送为短阻塞（帧最长 40 字节，115200 下约 3.5ms，可接受）。
 * ============================================================
 */

#define RING_MASK   (UART_RX_RING_SIZE - 1U)

static volatile uint8_t  s_ring[UART_CH_NUM][UART_RX_RING_SIZE];
static volatile uint16_t s_head[UART_CH_NUM];   /* 写入位置 */
static volatile uint16_t s_tail[UART_CH_NUM];   /* 读取位置 */
static volatile uint16_t s_overflow[UART_CH_NUM];

static void uart_gpio_init(void)
{
    GPIO_InitTypeDef g;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    g.GPIO_Speed = GPIO_Speed_50MHz;

    /* USART1 TX / USART2 TX */
    g.GPIO_Mode = GPIO_Mode_AF_PP;
    g.GPIO_Pin  = UART1_TX_PIN | UART2_TX_PIN;
    GPIO_Init(GPIOA, &g);

    /* USART1 RX / USART2 RX */
    g.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    g.GPIO_Pin  = UART1_RX_PIN | UART2_RX_PIN;
    GPIO_Init(GPIOA, &g);
}

static void uart_hw_init(USART_TypeDef *usart, uint32_t baud)
{
    USART_InitTypeDef u;

    USART_StructInit(&u);
    u.USART_BaudRate            = baud;
    u.USART_WordLength          = USART_WordLength_8b;
    u.USART_StopBits            = USART_StopBits_1;
    u.USART_Parity              = USART_Parity_No;
    u.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    u.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(usart, &u);

    USART_ITConfig(usart, USART_IT_RXNE, ENABLE);
    USART_Cmd(usart, ENABLE);
}

void Uart_Init(void)
{
    NVIC_InitTypeDef nvic;
    uint8_t ch;

    for (ch = 0; ch < UART_CH_NUM; ch++) {
        s_head[ch] = 0;
        s_tail[ch] = 0;
        s_overflow[ch] = 0;
    }

    uart_gpio_init();

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
    uart_hw_init(USART1, UART_BAUDRATE);
    uart_hw_init(USART2, UART_BAUDRATE);

    nvic.NVIC_IRQChannelCmd                = ENABLE;
    nvic.NVIC_IRQChannelPreemptionPriority = 0;   /* 串口最高，保证不丢字节 */
    nvic.NVIC_IRQChannelSubPriority        = 0;

    nvic.NVIC_IRQChannel = USART1_IRQn;
    NVIC_Init(&nvic);
    nvic.NVIC_IRQChannel = USART2_IRQn;
    NVIC_Init(&nvic);
}

static void uart_isr(uint8_t ch, USART_TypeDef *usart)
{
    if (USART_GetITStatus(usart, USART_IT_RXNE) != RESET) {
        uint8_t  data = (uint8_t)USART_ReceiveData(usart);
        uint16_t head = s_head[ch];

        if ((uint16_t)(head - s_tail[ch]) < UART_RX_RING_SIZE) {
            s_ring[ch][head & RING_MASK] = data;
            s_head[ch] = (uint16_t)(head + 1U);
        } else {
            s_overflow[ch]++;
        }
    }

    /* 溢出/噪声等错误：读 SR 再读 DR 清标志，防止反复进中断 */
    if (USART_GetFlagStatus(usart, USART_FLAG_ORE) != RESET) {
        (void)USART_ReceiveData(usart);
    }
}

void USART1_IRQHandler(void)
{
    uart_isr(UART_CH_ESP32, USART1);
}

void USART2_IRQHandler(void)
{
    uart_isr(UART_CH_CAM, USART2);
}

uint8_t Uart_Read(uint8_t ch, uint8_t *out)
{
    uint16_t tail;

    if (ch >= UART_CH_NUM) return 0;

    tail = s_tail[ch];
    if (tail == s_head[ch]) return 0;       /* 空 */

    *out = s_ring[ch][tail & RING_MASK];
    s_tail[ch] = (uint16_t)(tail + 1U);
    return 1;
}

uint16_t Uart_RxOverflowCount(uint8_t ch)
{
    return ch < UART_CH_NUM ? s_overflow[ch] : 0U;
}

void Uart_WriteByte(uint8_t ch, uint8_t b)
{
    USART_TypeDef *usart = (ch == UART_CH_CAM) ? USART2 : USART1;

    while (USART_GetFlagStatus(usart, USART_FLAG_TXE) == RESET) { }
    USART_SendData(usart, b);
}

void Uart_Write(uint8_t ch, const uint8_t *buf, uint16_t len)
{
    uint16_t i;

    for (i = 0; i < len; i++) Uart_WriteByte(ch, buf[i]);
}
