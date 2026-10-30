#ifndef __BSP_STORAGE_H
#define __BSP_STORAGE_H
#include <stdint.h>
#include "state_machine.h"     /* 使用 ScheduleItem */

/* 内部 Flash 存储（替代未接线的外部 W25Q64）
 * 用 STM32F103RCT6 内部 Flash 最后 1 页(2KB) 保存服药计划，掉电不丢。
 * 注意: 写入会整页擦除，且写 Flash 期间会短暂暂停取指，注意操作时机。 */

void    Storage_Init(void);
/* 保存服药计划到内部 Flash，返回 1 成功 */
uint8_t Storage_SaveSchedule(const ScheduleItem *sched, uint8_t num);
/* 从内部 Flash 读取服药计划，返回 1 成功（无有效数据返回 0） */
uint8_t Storage_LoadSchedule(ScheduleItem *sched, uint8_t *num);

#endif
