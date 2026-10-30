#ifndef __BSP_LIMIT_H
#define __BSP_LIMIT_H
#include <stdint.h>

/* 限位传感器：外部上拉，触发（到位）为低电平 */
void Limit_Init(void);              /* GPIO 输入初始化 */
uint8_t Limit_Org_Tripped(void);    /* 原点限位是否触发 */
uint8_t Limit_Pos_Tripped(void);    /* 药仓/到位限位是否触发 */

#endif
