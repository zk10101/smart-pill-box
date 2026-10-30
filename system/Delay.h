#ifndef __DELAY_H
#define __DELAY_H

#include <stdint.h>

/* 初始化 DWT 周期计数器（可重复调用，也可在使用 Delay_* 前不必手动调用） */
void Delay_Init(void);

void Delay_us(uint32_t us);
void Delay_ms(uint32_t ms);
void Delay_s(uint32_t s);

#endif
