#include "bsp_esp32.h"
#include "pinmap.h"
#include "app_config.h"
#include "stm32f10x.h"

static uint8_t       rx_ring[RX_RING_SIZE];
static volatile uint16_t rx_head = 0;   /* 写指针(中断) */
static volatile uint16_t rx_tail = 0;   /* 读指针(主循环) */

static ProtoParser  parser;
static ProtoFrame   rx_frame;
static ESP32_RxHandler rx_handler = 0;

void ESP32_Init(void)
{
    GPIO_InitTypeDef g;
    USART_InitTypeDef u;
    NVIC_InitTypeDef n;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);

    /* EN 控制 */
    g.GPIO_Mode  = GPIO_Mode_Out_PP;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    g.GPIO_Pin   = NET_EN_PIN;
    GPIO_Init(NET_EN_PORT, &g);
    ESP32_Power(0);

    /* TX/RX */
    g.GPIO_Mode  = GPIO_Mode_AF_PP;
    g.GPIO_Pin   = NET_TX_PIN;
    GPIO_Init(NET_TX_PORT, &g);
    g.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
    g.GPIO_Pin   = NET_RX_PIN;
    GPIO_Init(NET_RX_PORT, &g);

    u.USART_BaudRate            = ESP32_BAUDRATE;
    u.USART_WordLength          = USART_WordLength_8b;
    u.USART_StopBits            = USART_StopBits_1;
    u.USART_Parity              = USART_Parity_No;
    u.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    u.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART1, &u);

    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
    n.NVIC_IRQChannel = USART1_IRQn;
    n.NVIC_IRQChannelPreemptionPriority = 1;
    n.NVIC_IRQChannelSubPriority = 0;
    n.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&n);

    USART_Cmd(USART1, ENABLE);
    ESP32_Power(1);
}

void ESP32_Power(uint8_t on)
{
    if (on) GPIO_SetBits(NET_EN_PORT, NET_EN_PIN);
    else    GPIO_ResetBits(NET_EN_PORT, NET_EN_PIN);
}

void ESP32_Send(const uint8_t *buf, uint16_t len)
{
    while (len--) {
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
        USART_SendData(USART1, *buf++);
    }
}

void ESP32_SendFrame(uint8_t cmd, const uint8_t *data, uint8_t len)
{
    uint8_t buf[PROTO_MAX_LEN + 5];
    uint16_t n = Protocol_Pack(buf, cmd, data, len);
    ESP32_Send(buf, n);
}

void ESP32_SetRxHandler(ESP32_RxHandler handler) { rx_handler = handler; }

void ESP32_OnRxByte(uint8_t byte)
{
    uint16_t next = (uint16_t)((rx_head + 1) & (RX_RING_SIZE - 1));
    if (next != rx_tail) {          /* 未满 */
        rx_ring[rx_head] = byte;
        rx_head = next;
    }
}

void ESP32_Poll(void)
{
    while (rx_tail != rx_head) {
        uint8_t b = rx_ring[rx_tail];
        rx_tail = (uint16_t)((rx_tail + 1) & (RX_RING_SIZE - 1));
        if (Protocol_ParseByte(&parser, b, &rx_frame)) {
            if (rx_handler) rx_handler(&rx_frame);
        }
    }
}

void USART1_IRQHandler(void)
{
    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
        uint8_t b = (uint8_t)USART_ReceiveData(USART1);
        ESP32_OnRxByte(b);
    }
}
