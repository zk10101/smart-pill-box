#ifndef __BSP_ESP32_H
#define __BSP_ESP32_H
#include <stdint.h>
#include "protocol.h"

/* ESP32-C3 无线模块（USART1 115200-8-N-1）
 * NET_EN=PA8(高使能)  TX=PA9  RX=PA10 */

void ESP32_Init(void);
void ESP32_Power(uint8_t on);       /* 模块电源/使能 */
/* 发送原始字节 */
void ESP32_Send(const uint8_t *buf, uint16_t len);
/* 打包发送一帧 */
void ESP32_SendFrame(uint8_t cmd, const uint8_t *data, uint8_t len);
/* 非阻塞轮询：处理接收到的帧（需主循环周期性调用） */
void ESP32_Poll(void);
/* 收到一帧时的回调（由应用层实现/注册） */
typedef void (*ESP32_RxHandler)(const ProtoFrame *frame);
void ESP32_SetRxHandler(ESP32_RxHandler handler);

/* 供中断调用的字节接收入口 */
void ESP32_OnRxByte(uint8_t byte);
/* 接收环形缓冲溢出计数（0 表示没丢过字节，调试用） */
uint16_t ESP32_GetRxOverflow(void);

#endif
