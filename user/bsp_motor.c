#include "bsp_motor.h"
#include "bsp_limit.h"
#include "pinmap.h"
#include "app_config.h"
#include "stm32f10x.h"

static volatile MotorStatus motor_status = MOTOR_IDLE;
static volatile uint32_t    motor_target;   /* 目标脉冲数 */
static volatile uint32_t    motor_pulses;   /* 已发脉冲数 */
static volatile uint32_t    motor_tick_ms;  /* 运行计时(ms) */

void Motor_Init(void)
{
    GPIO_InitTypeDef g;
    TIM_TimeBaseInitTypeDef  tb;
    TIM_OCInitTypeDef        oc;
    NVIC_InitTypeDef         nvic;

    /* --- GPIO ---
     * STEP=PB8(TIM4_CH3) DIR=PB9  EN=PC12(经 245 的 A3 网络)
     * 原理图上 PD2 为空脚，EN 不在 PD2，见 pinmap.h 说明 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC, ENABLE);
    g.GPIO_Mode  = GPIO_Mode_AF_PP;                 /* STEP 用 TIM4_CH3 复用推挽 */
    g.GPIO_Speed = GPIO_Speed_50MHz;
    g.GPIO_Pin   = MOTOR_STEP_PIN;
    GPIO_Init(MOTOR_STEP_PORT, &g);

    g.GPIO_Mode  = GPIO_Mode_Out_PP;                /* DIR / EN */
    g.GPIO_Speed = GPIO_Speed_50MHz;
    g.GPIO_Pin   = MOTOR_DIR_PIN;
    GPIO_Init(MOTOR_DIR_PORT, &g);
    g.GPIO_Pin   = MOTOR_EN_PIN;
    GPIO_Init(MOTOR_EN_PORT, &g);

    g.GPIO_Mode  = GPIO_Mode_IPU;                   /* FAULT 上拉输入，低有效 */
    g.GPIO_Pin   = MOTOR_FAULT_PIN;
    GPIO_Init(MOTOR_FAULT_PORT, &g);

    GPIO_ResetBits(MOTOR_DIR_PORT, MOTOR_DIR_PIN);  /* 默认方向 */
    Motor_Enable(0);                                /* 默认失能 */

    /* --- TIM4: 产生 STEP 脉冲 (CH3 = PB8) --- */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
    TIM_TimeBaseStructInit(&tb);
    tb.TIM_Prescaler         = (SystemCoreClock / 1000000) - 1;  /* 1MHz 计数 */
    tb.TIM_Period            = (1000000UL / MOTOR_STEP_FREQ_HZ) - 1; /* 脉冲频率 */
    tb.TIM_CounterMode       = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM4, &tb);

    TIM_OCStructInit(&oc);
    oc.TIM_OCMode      = TIM_OCMode_PWM1;
    oc.TIM_OutputState = TIM_OutputState_Enable;
    oc.TIM_Pulse       = (1000000UL / MOTOR_STEP_FREQ_HZ) / 2;    /* 50% 占空比 */
    oc.TIM_OCPolarity  = TIM_OCPolarity_High;
    TIM_OC3Init(TIM4, &oc);

    /* 更新中断：每个周期=一个脉冲 */
    TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
    TIM_ITConfig(TIM4, TIM_IT_Update, ENABLE);
    nvic.NVIC_IRQChannel = TIM4_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 2;
    nvic.NVIC_IRQChannelSubPriority = 0;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);

    motor_status = MOTOR_IDLE;
}

void Motor_Enable(uint8_t en)
{
    /* 经 74HCT245(非反相) 到驱动器 EN。
     * 驱动器 EN 极性由 MOTOR_EN_ACTIVE_LOW 决定（见 app_config.h）：
     *   低有效(MOTOR_EN_ACTIVE_LOW=1): 使能输出低电平
     *   高有效(MOTOR_EN_ACTIVE_LOW=0): 使能输出高电平 */
#if MOTOR_EN_ACTIVE_LOW
    if (en) GPIO_ResetBits(MOTOR_EN_PORT, MOTOR_EN_PIN);
    else    GPIO_SetBits(MOTOR_EN_PORT, MOTOR_EN_PIN);
#else
    if (en) GPIO_SetBits(MOTOR_EN_PORT, MOTOR_EN_PIN);
    else    GPIO_ResetBits(MOTOR_EN_PORT, MOTOR_EN_PIN);
#endif
}

uint8_t Motor_IsFault(void)
{
    return (uint8_t)(GPIO_ReadInputDataBit(MOTOR_FAULT_PORT, MOTOR_FAULT_PIN) == 0);
}

MotorStatus Motor_Dispense(uint32_t steps)
{
    if (motor_status == MOTOR_RUNNING) return MOTOR_BUSY;

    motor_target = steps;
    motor_pulses = 0;
    motor_tick_ms = 0;
    Motor_Enable(1);
    TIM_SetCounter(TIM4, 0);
    TIM_Cmd(TIM4, ENABLE);                  /* 开始发脉冲 */
    motor_status = MOTOR_RUNNING;
    return motor_status;
}

void Motor_Stop(void)
{
    TIM_Cmd(TIM4, DISABLE);
    Motor_Enable(0);
    motor_status = MOTOR_IDLE;
}

void Motor_OnPulse(void)    /* 每发一个脉冲进入一次 */
{
    motor_pulses++;
    if (motor_pulses >= motor_target) {
        TIM_Cmd(TIM4, DISABLE);
        Motor_Enable(0);
        motor_status = MOTOR_DONE;
    }
}

void Motor_Tick(void)   /* 每 1ms 调用 */
{
    if (motor_status != MOTOR_RUNNING) return;
    if (Motor_IsFault()) {
        Motor_Stop();
        motor_status = MOTOR_FAULT;
        return;
    }
    if (Limit_Pos_Tripped()) {              /* 到位即停 */
        TIM_Cmd(TIM4, DISABLE);
        Motor_Enable(0);
        motor_status = MOTOR_DONE;
        return;
    }
    if (++motor_tick_ms >= MOTOR_TIMEOUT_MS) {  /* 超时保护 */
        TIM_Cmd(TIM4, DISABLE);
        Motor_Enable(0);
        motor_status = MOTOR_TIMEOUT;
    }
}

MotorStatus Motor_GetStatus(void) { return motor_status; }

/* TIM4 更新中断：产生一个 STEP 脉冲 */
void TIM4_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM4, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
        Motor_OnPulse();
    }
}
