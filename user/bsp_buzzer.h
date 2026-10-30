#ifndef __BSP_BUZZER_H
#define __BSP_BUZZER_H
#include <stdint.h>

void Buzzer_Init(void);
void Buzzer_On(void);
void Buzzer_Off(void);
/* 非阻塞提示: 短鸣一次(时长 ms)。内部用 ms 节拍，需主循环周期性调用 Buzzer_Tick() */
void Buzzer_Beep(uint16_t ms);
/* 报警: 连续 3 短鸣 2 长鸣，由 Buzzer_Tick 在后台执行 */
void Buzzer_Alarm(void);
void Buzzer_Stop(void);
void Buzzer_Tick(void);     /* 每 1ms 调用 */

#endif
