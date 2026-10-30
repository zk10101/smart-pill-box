#include "bsp_buzzer.h"
#include "pinmap.h"
#include "stm32f10x.h"

static uint16_t buzz_remain_ms;     /* 剩余鸣响时间 */
static uint8_t  buzz_alarm;         /* 报警模式标志 */

void Buzzer_Init(void)
{
    GPIO_InitTypeDef g;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    g.GPIO_Mode  = GPIO_Mode_Out_PP;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    g.GPIO_Pin   = BUZZER_PIN;
    GPIO_Init(BUZZER_PORT, &g);
    Buzzer_Off();
    buzz_remain_ms = 0;
    buzz_alarm = 0;
}

void Buzzer_On(void)  { GPIO_SetBits(BUZZER_PORT, BUZZER_PIN); }
void Buzzer_Off(void) { GPIO_ResetBits(BUZZER_PORT, BUZZER_PIN); }

void Buzzer_Beep(uint16_t ms)
{
    Buzzer_On();
    buzz_remain_ms = ms;
}

void Buzzer_Alarm(void) { buzz_alarm = 1; }
void Buzzer_Stop(void)  { buzz_alarm = 0; buzz_remain_ms = 0; Buzzer_Off(); }

void Buzzer_Tick(void)
{
    if (buzz_alarm) {
        /* 简单周期: 每 300ms 反转一次，形成急促报警音 */
        static uint16_t cnt;
        if (++cnt >= 150) { cnt = 0; GPIO_WriteBit(BUZZER_PORT, BUZZER_PIN,
                            (BitAction)(1 - GPIO_ReadOutputDataBit(BUZZER_PORT, BUZZER_PIN))); }
        return;
    }
    if (buzz_remain_ms) {
        if (--buzz_remain_ms == 0) Buzzer_Off();
    }
}
