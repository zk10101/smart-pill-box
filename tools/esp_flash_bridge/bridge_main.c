#include "stm32f10x.h"

/* H1 USART2 <-> onboard ESP32-C3 USART1. No application framing is used. */
#define BRIDGE_BAUD       115200U
#define BRIDGE_QUEUE_SIZE 2048U
#define BRIDGE_QUEUE_MASK (BRIDGE_QUEUE_SIZE - 1U)
#define ESP_EN_PIN        GPIO_Pin_8

typedef struct {
    volatile uint16_t head;
    volatile uint16_t tail;
    uint8_t bytes[BRIDGE_QUEUE_SIZE];
} ByteQueue;

static ByteQueue to_esp;
static ByteQueue to_pc;
static volatile uint32_t ticks_ms;
volatile uint32_t bridge_pc_overflow;
volatile uint32_t bridge_esp_overflow;

void SysTick_Handler(void)
{
    ticks_ms++;
}

static void bridge_receive(USART_TypeDef *source, USART_TypeDef *dest,
                           ByteQueue *queue, volatile uint32_t *overflow)
{
    uint32_t status = source->SR;
    uint8_t byte;

    if ((status & (USART_SR_RXNE | USART_SR_ORE | USART_SR_FE | USART_SR_NE)) == 0U)
        return;

    byte = (uint8_t)source->DR; /* Reading SR then DR also clears UART errors. */
    if (status & USART_SR_ORE) (*overflow)++;
    if ((status & USART_SR_RXNE) == 0U) return;

    if ((uint16_t)(queue->head - queue->tail) == BRIDGE_QUEUE_SIZE) {
        (*overflow)++;
        return;
    }

    queue->bytes[queue->head & BRIDGE_QUEUE_MASK] = byte;
    queue->head++;
    dest->CR1 |= USART_CR1_TXEIE;
}

static void bridge_transmit(USART_TypeDef *dest, ByteQueue *queue)
{
    if ((dest->CR1 & USART_CR1_TXEIE) == 0U ||
        (dest->SR & USART_SR_TXE) == 0U) return;

    if (queue->tail == queue->head) {
        dest->CR1 &= (uint16_t)~USART_CR1_TXEIE;
        return;
    }

    dest->DR = queue->bytes[queue->tail & BRIDGE_QUEUE_MASK];
    queue->tail++;
}

void USART1_IRQHandler(void)
{
    bridge_receive(USART1, USART2, &to_pc, &bridge_esp_overflow);
    bridge_transmit(USART1, &to_esp);
}

void USART2_IRQHandler(void)
{
    bridge_receive(USART2, USART1, &to_esp, &bridge_pc_overflow);
    bridge_transmit(USART2, &to_pc);
}

static void uart_init(USART_TypeDef *uart)
{
    USART_InitTypeDef settings;
    USART_StructInit(&settings);
    settings.USART_BaudRate = BRIDGE_BAUD;
    settings.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(uart, &settings);
    USART_ITConfig(uart, USART_IT_RXNE, ENABLE);
    USART_Cmd(uart, ENABLE);
}

int main(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1,
                           ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    /* Hold ESP in reset before opening UART0; H3 must hold IO9 low. */
    GPIO_ResetBits(GPIOA, ESP_EN_PIN);
    gpio.GPIO_Pin = ESP_EN_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOA, &gpio);

    gpio.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_2; /* USART1/2 TX */
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);
    gpio.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_3; /* USART1/2 RX */
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);

    uart_init(USART1);
    uart_init(USART2);
    NVIC_SetPriority(USART1_IRQn, 0U);
    NVIC_SetPriority(USART2_IRQn, 0U);
    NVIC_EnableIRQ(USART1_IRQn);
    NVIC_EnableIRQ(USART2_IRQn);

    if (SysTick_Config(SystemCoreClock / 1000U) != 0U) {
        while (1) { }
    }
    while (ticks_ms < 150U) { }
    GPIO_SetBits(GPIOA, ESP_EN_PIN);
    SysTick->CTRL = 0U;

    while (1) {
        __WFI();
    }
}
