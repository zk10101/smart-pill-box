#ifndef __APP_ACTUATOR_H
#define __APP_ACTUATOR_H
/**
 * ============================================================
 * 业务逻辑层：出药执行机构
 *
 *   职责：把"回零 / 出药 N 粒 / 急停 / 清故障"翻译成电机动作，
 *         维护位置与状态，处理限位、超时、驱动器故障。
 *   依赖：app_port.h（数据访问层接口）+ app_types.h + app_config.h
 *   被依赖：app_command（指令执行）、app_ui（按键/显示）
 * ============================================================
 */
#include "app_types.h"

/* 指令返回码（与串口 ACK 一致） */
#define ACT_RC_OK           0
#define ACT_RC_BUSY         1
#define ACT_RC_PARAM        2
#define ACT_RC_NOT_HOMED    3
#define ACT_RC_FAULT        4
#define ACT_RC_5V_OFF       5
#define ACT_RC_TIMEOUT      6
#define ACT_RC_LIMIT        7

void     Act_Init(void);
void     Act_Tick(void);                     /* 1ms */

uint8_t  Act_Home(void);                     /* 0=已启动 其他=错误码 */
uint8_t  Act_Dispense(uint8_t pills);        /* 0=已启动 其他=错误码 */
void     Act_Stop(void);
void     Act_ClearError(void);

uint8_t  Act_IsBusy(void);
uint8_t  Act_IsHomed(void);
ActState Act_GetState(void);
ActError Act_GetError(void);
void     Act_GetStatus(ActStatus *st);
void     Act_GetParams(ActParams *p);
uint8_t  Act_SetParams(const ActParams *p);  /* 0=成功 其他=错误码 */

#endif /* __APP_ACTUATOR_H */
