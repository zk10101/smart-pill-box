#include "app_port.h"
#include "app_config.h"
#include "pinmap.h"
#include "stm32f10x.h"

/**
 * ============================================================
 * 数据访问层：有源蜂鸣器（PB11，NPN 驱动，高电平响）
 *   最终版原理图把蜂鸣器电源改为 3V3，可直接由 GPIO 驱动三极管
 * ============================================================
 */

static uint16_t s_remain_ms;
static uint8_t  s_alarm;
static uint16_t s_alarm_cnt;

void Buzzer_Init(void)
{
    GPIO_InitTypeDef g;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    g.GPIO_Mode  = GPIO_Mode_Out_PP;
    g.GPIO_Speed = GPIO_Speed_2MHz;
    g.GPIO_Pin   = BUZZER_PIN;
    GPIO_Init(BUZZER_PORT, &g);

    GPIO_ResetBits(BUZZER_PORT, BUZZER_PIN);
    s_remain_ms = 0;
    s_alarm     = 0;
    s_alarm_cnt = 0;
}

void Buzzer_Beep(uint16_t ms)
{
    if (s_alarm) return;                    /* 报警优先 */
    GPIO_SetBits(BUZZER_PORT, BUZZER_PIN);
    s_remain_ms = ms;
}

void Buzzer_Alarm(uint8_t on)
{
    if (on) {
        s_alarm     = 1;
        s_alarm_cnt = 0;
    } else {
        s_alarm     = 0;
        s_remain_ms = 0;
        GPIO_ResetBits(BUZZER_PORT, BUZZER_PIN);
    }
}

void Buzzer_Tick(void)
{
    if (s_alarm) {
        /* 300ms 通断交替，形成急促报警音 */
        if (++s_alarm_cnt >= 150U) {
            s_alarm_cnt = 0;
            if (GPIO_ReadOutputDataBit(BUZZER_PORT, BUZZER_PIN))
                GPIO_ResetBits(BUZZER_PORT, BUZZER_PIN);
            else
                GPIO_SetBits(BUZZER_PORT, BUZZER_PIN);
        }
        return;
    }

    if (s_remain_ms) {
        if (--s_remain_ms == 0U) GPIO_ResetBits(BUZZER_PORT, BUZZER_PIN);
    }
}
