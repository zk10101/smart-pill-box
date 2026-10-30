#ifndef __BSP_WS2812_H
#define __BSP_WS2812_H
#include <stdint.h>

/* WS2812B 单线彩灯：PA11 DIN，共 WS2812_NUM 颗，GRB 8bit */
void WS2812_Init(void);
void WS2812_SetPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b);
void WS2812_Show(void);             /* 发送刷新 */
void WS2812_Clear(void);            /* 全灭 */

#endif
