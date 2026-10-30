#ifndef __BSP_MOTOR_H
#define __BSP_MOTOR_H
#include <stdint.h>

typedef enum {
    MOTOR_IDLE = 0,     /* 空闲 */
    MOTOR_RUNNING,      /* 运行中 */
    MOTOR_DONE,         /* 完成(触发限位) */
    MOTOR_TIMEOUT,      /* 超时异常 */
    MOTOR_FAULT,        /* 驱动器故障(FAULT_N) */
    MOTOR_BUSY          /* 忙(上一次出药未结束) */
} MotorStatus;

/* STEP=PB8(TIM4_CH3 脉冲), DIR=PB9, EN=PC12, FAULT=PC13
 * 对应 74HCT245 电平转换: A1=PB8->B1=STEP, A2=PB9->B2=DIR, A3=PC12->B3=EN
 * 注意: 原 PD2 在原理图上为空脚，切勿再使用 */
void Motor_Init(void);
/* 出药 N 步：使能 -> 发脉冲 -> 到位/超时/故障停止。返回最终状态（RUNNING=开始成功） */
MotorStatus Motor_Dispense(uint32_t steps);
/* 紧急停止 */
void Motor_Stop(void);
/* 使能/失能 */
void Motor_Enable(uint8_t en);
/* 读驱动器故障(低有效) */
uint8_t Motor_IsFault(void);
/* 后台刷新：需主循环周期性调用（用于超时判断） */
void Motor_Tick(void);
MotorStatus Motor_GetStatus(void);

/* TIM4 更新中断回调（每发一个脉冲调用一次，在 bsp_motor.c 内部已定义 IRQ） */
void Motor_OnPulse(void);

#endif
