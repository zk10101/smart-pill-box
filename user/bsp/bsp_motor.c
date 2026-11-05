#include "app_port.h"
#include "app_config.h"
#include "pinmap.h"
#include "stm32f10x.h"

/**
 * ============================================================
 * 数据访问层：步进电机（TIM4_CH3 产生 STEP 脉冲，更新中断计数）
 *
 *   脉冲通道: PB8 = TIM4_CH3 -> 74HCT245 A1/B1 -> U4.1 STEP
 *   方向    : PB9  -> A2/B2 -> U4.2 DIR
 *   使能    : PC12 -> A3/B3 -> U4.3 EN（网络带 10k 下拉）
 *   故障    : PC13 <- R4 1k <- FAULT_N（低有效）
 *
 *   TIM4 计数时钟 1MHz，ARR 决定脉冲周期；每个更新事件 = 一个脉冲。
 *   本层只负责"发 N 个脉冲/停/方向/使能"，是否该停由业务层判断。
 * ============================================================
 */

static volatile uint32_t s_target;      /* 目标脉冲数 */
static volatile uint32_t s_done_cnt;    /* 已发脉冲数 */
static volatile uint8_t  s_busy;
static volatile uint8_t  s_done_flag;
static uint16_t s_speed_hz;

static void motor_set_period(uint16_t hz)
{
    uint32_t arr;

    if (hz < MOTOR_MIN_SPEED_HZ) hz = MOTOR_MIN_SPEED_HZ;
    if (hz > MOTOR_MAX_SPEED_HZ) hz = MOTOR_MAX_SPEED_HZ;

    arr = (1000000UL / (uint32_t)hz);
    if (arr < 2U) arr = 2U;

    s_speed_hz = hz;
    TIM_SetAutoreload(TIM4, arr - 1U);
    TIM_SetCompare3(TIM4, (arr / 2U));      /* 50% 占空比，驱动器只需上升沿 */
}

void Motor_Init(void)
{
    GPIO_InitTypeDef        g;
    TIM_TimeBaseInitTypeDef tb;
    TIM_OCInitTypeDef       oc;
    NVIC_InitTypeDef        nvic;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC, ENABLE);

    /* STEP: TIM4_CH3 复用推挽 */
    g.GPIO_Mode  = GPIO_Mode_AF_PP;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    g.GPIO_Pin   = MOTOR_STEP_PIN;
    GPIO_Init(MOTOR_STEP_PORT, &g);

    /* DIR / EN: 普通推挽 */
    g.GPIO_Mode = GPIO_Mode_Out_PP;
    g.GPIO_Pin  = MOTOR_DIR_PIN;
    GPIO_Init(MOTOR_DIR_PORT, &g);
    g.GPIO_Pin  = MOTOR_EN_PIN;
    GPIO_Init(MOTOR_EN_PORT, &g);

    /* FAULT: 输入（外部 R3 10k 上拉到 3V3） */
    g.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    g.GPIO_Pin  = MOTOR_FAULT_PIN;
    GPIO_Init(MOTOR_FAULT_PORT, &g);

    GPIO_ResetBits(MOTOR_DIR_PORT, MOTOR_DIR_PIN);
    GPIO_ResetBits(MOTOR_EN_PORT, MOTOR_EN_PIN);   /* 默认失能，由业务层决定极性 */

    /* TIM4: 1MHz 计数，默认 800Hz 脉冲 */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
    TIM_TimeBaseStructInit(&tb);
    tb.TIM_Prescaler     = (uint16_t)((SystemCoreClock / 1000000UL) - 1U);
    tb.TIM_Period        = (1000000UL / MOTOR_DEFAULT_SPEED_HZ) - 1U;
    tb.TIM_CounterMode   = TIM_CounterMode_Up;
    tb.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(TIM4, &tb);

    TIM_OCStructInit(&oc);
    oc.TIM_OCMode      = TIM_OCMode_PWM1;
    oc.TIM_OutputState = TIM_OutputState_Enable;
    oc.TIM_Pulse       = (1000000UL / MOTOR_DEFAULT_SPEED_HZ) / 2U;
    oc.TIM_OCPolarity  = TIM_OCPolarity_High;
    TIM_OC3Init(TIM4, &oc);

    TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
    TIM_ITConfig(TIM4, TIM_IT_Update, ENABLE);

    nvic.NVIC_IRQChannel                   = TIM4_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 1;    /* 高于 SysTick，低于串口 */
    nvic.NVIC_IRQChannelSubPriority        = 0;
    nvic.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&nvic);

    s_target    = 0;
    s_done_cnt  = 0;
    s_busy      = 0;
    s_done_flag = 0;
    s_speed_hz  = MOTOR_DEFAULT_SPEED_HZ;
}

void Motor_SetSpeed(uint16_t hz)
{
    motor_set_period(hz);
}

void Motor_SetDir(uint8_t forward)
{
    if (forward) GPIO_SetBits(MOTOR_DIR_PORT, MOTOR_DIR_PIN);
    else         GPIO_ResetBits(MOTOR_DIR_PORT, MOTOR_DIR_PIN);
}

void Motor_Enable(uint8_t on)
{
    /* PC12 -> 245 A3/B3 -> 驱动器 EN。
     * 驱动器 EN 极性由业务层的 en_active_low 参数决定，
     * 这里只负责"把想要的电平写到引脚上"：on 参数含义为"使能电平"，
     * 因此由业务层把 (en_active_low ? 0 : 1) 传给本函数。 */
    if (on) GPIO_SetBits(MOTOR_EN_PORT, MOTOR_EN_PIN);
    else    GPIO_ResetBits(MOTOR_EN_PORT, MOTOR_EN_PIN);
}

uint8_t Motor_IsEnabled(void)
{
    return (uint8_t)(GPIO_ReadOutputDataBit(MOTOR_EN_PORT, MOTOR_EN_PIN) != 0);
}

void Motor_Start(uint32_t steps)
{
    if (steps == 0U) return;

    s_target    = steps;
    s_done_cnt  = 0;
    s_done_flag = 0;
    s_busy      = 1;

    TIM_SetCounter(TIM4, 0);
    TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
    TIM_Cmd(TIM4, ENABLE);
}

void Motor_Stop(void)
{
    TIM_Cmd(TIM4, DISABLE);
    s_busy = 0;
}

uint8_t Motor_IsBusy(void)
{
    return s_busy;
}

uint32_t Motor_StepsDone(void)
{
    return s_done_cnt;
}

uint8_t Motor_TakeDone(void)
{
    uint8_t d = s_done_flag;

    s_done_flag = 0;
    return d;
}

uint8_t Motor_IsFault(void)
{
    return (uint8_t)(GPIO_ReadInputDataBit(MOTOR_FAULT_PORT, MOTOR_FAULT_PIN) == 0);
}

void Motor_Tick(void)
{
    /* 预留：如以后需要硬件看门狗式的堵转保护，可在此处累计运行时间 */
    (void)s_speed_hz;
}

/* TIM4 更新中断：一个周期 = 一个 STEP 上升沿 */
void TIM4_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM4, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit(TIM4, TIM_IT_Update);

        if (!s_busy) return;

        s_done_cnt++;
        if (s_done_cnt >= s_target) {
            TIM_Cmd(TIM4, DISABLE);
            s_busy      = 0;
            s_done_flag = 1;
        }
    }
}
