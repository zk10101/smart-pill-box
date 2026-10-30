#ifndef __BSP_LCD_H
#define __BSP_LCD_H
#include <stdint.h>

/* ST7789 240x240 SPI LCD 驱动（SPI1，软件 CS）
 * 引脚: CS=PA4 DC=PB0 RES=PB1 BL=PB10(低电平点亮) SCK/MOSI 走 SPI1
 *
 * 注意: 常见 1.3" 240x240 玻璃的 visible area 在 ST7789 内部 RAM 的
 *       (0,80)~(239,319)，若显示整体偏上/底部黑边，把 LCD_Y_OFFSET 改为 80 */
#define LCD_WIDTH   240
#define LCD_HEIGHT  240
#define LCD_X_OFFSET  0
#define LCD_Y_OFFSET  0

/* 常用颜色 (RGB565) */
#define LCD_BLACK   0x0000
#define LCD_WHITE   0xFFFF
#define LCD_RED     0xF800
#define LCD_GREEN   0x07E0
#define LCD_BLUE    0x001F

void LCD_Init(void);
void LCD_SetBacklight(uint8_t on);      /* 1=亮 0=灭 */
void LCD_Fill(uint16_t color);
void LCD_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void LCD_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
/* TODO: 中文字库(W25Q) + ASCII 字模显示接口，后续补充 */
/* void LCD_ShowString(uint16_t x, uint16_t y, const char *str, uint16_t color); */

#endif
