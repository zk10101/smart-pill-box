#include "app_port.h"
#include "app_config.h"
#include "pinmap.h"
#include "font8x8.h"
#include "stm32f10x.h"
#include "Delay.h"

/**
 * ============================================================
 * 数据访问层：SPI ST7789 240x240 彩屏（仅用于状态显示）
 *
 *   SCK=PA5  MOSI=PA7  CS=PA4(软件片选,10k上拉)  DC=PB0  RES=PB1
 *   BL=PB10 -> AO3401A PMOS 栅极：低电平点亮
 *
 *   硬件注意：屏 SDA 经 R46 22R 串到 SPI1_MOSI；若显示异常，先量 R46。
 *   本文件提供 FillRect/Text/TextNum 三个绘图原语，够状态页使用。
 * ============================================================
 */

#define LCD_CMD     0
#define LCD_DAT     1

static void lcd_spi_init(void)
{
    GPIO_InitTypeDef g;
    SPI_InitTypeDef  s;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_SPI1, ENABLE);

    g.GPIO_Mode  = GPIO_Mode_AF_PP;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    g.GPIO_Pin   = SPI1_SCK_PIN | SPI1_MOSI_PIN;
    GPIO_Init(GPIOA, &g);

    g.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    g.GPIO_Pin  = SPI1_MISO_PIN;
    GPIO_Init(GPIOA, &g);

    SPI_StructInit(&s);
    s.SPI_Mode              = SPI_Mode_Master;
    s.SPI_Direction         = SPI_Direction_2Lines_FullDuplex;
    s.SPI_DataSize          = SPI_DataSize_8b;
    s.SPI_CPOL              = SPI_CPOL_Low;
    s.SPI_CPHA              = SPI_CPHA_1Edge;
    s.SPI_NSS               = SPI_NSS_Soft;
    s.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4;   /* 18MHz */
    s.SPI_FirstBit          = SPI_FirstBit_MSB;
    SPI_Init(SPI1, &s);
    SPI_Cmd(SPI1, ENABLE);
}

static void lcd_gpio_init(void)
{
    GPIO_InitTypeDef g;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);
    g.GPIO_Mode  = GPIO_Mode_Out_PP;
    g.GPIO_Speed = GPIO_Speed_50MHz;

    g.GPIO_Pin = LCD_CS_PIN;
    GPIO_Init(LCD_CS_PORT, &g);

    g.GPIO_Pin = LCD_DC_PIN | LCD_RES_PIN | LCD_BL_PIN;
    GPIO_Init(GPIOB, &g);

    GPIO_SetBits(LCD_CS_PORT, LCD_CS_PIN);
    GPIO_SetBits(LCD_BL_PORT, LCD_BL_PIN);      /* 默认背光关（高=灭） */
}

static void lcd_select(uint8_t sel)
{
    if (sel) GPIO_ResetBits(LCD_CS_PORT, LCD_CS_PIN);
    else     GPIO_SetBits(LCD_CS_PORT, LCD_CS_PIN);
}

static void lcd_dc(uint8_t dat)
{
    if (dat) GPIO_SetBits(LCD_DC_PORT, LCD_DC_PIN);
    else     GPIO_ResetBits(LCD_DC_PORT, LCD_DC_PIN);
}

static void spi_byte(uint8_t b)
{
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET) { }
    SPI_I2S_SendData(SPI1, b);
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET) { }
    (void)SPI_I2S_ReceiveData(SPI1);
}

static void lcd_cmd(uint8_t cmd)
{
    lcd_select(1);
    lcd_dc(LCD_CMD);
    spi_byte(cmd);
    lcd_select(0);
}

static void lcd_data(uint8_t dat)
{
    lcd_select(1);
    lcd_dc(LCD_DAT);
    spi_byte(dat);
    lcd_select(0);
}

/* 一次片选内连续写 count 个像素（RGB565），减少片选开销 */
static void lcd_data16(uint16_t color, uint32_t count)
{
    uint8_t hi = (uint8_t)(color >> 8);
    uint8_t lo = (uint8_t)(color & 0xFF);

    lcd_select(1);
    lcd_dc(LCD_DAT);
    while (count--) {
        spi_byte(hi);
        spi_byte(lo);
    }
    lcd_select(0);
}

static void lcd_window(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    uint16_t x1 = (uint16_t)(x + w - 1U);
    uint16_t y1 = (uint16_t)(y + h - 1U);

    lcd_cmd(0x2A);
    lcd_data((uint8_t)(x >> 8));  lcd_data((uint8_t)(x & 0xFF));
    lcd_data((uint8_t)(x1 >> 8)); lcd_data((uint8_t)(x1 & 0xFF));

    lcd_cmd(0x2B);
    lcd_data((uint8_t)(y >> 8));  lcd_data((uint8_t)(y & 0xFF));
    lcd_data((uint8_t)(y1 >> 8)); lcd_data((uint8_t)(y1 & 0xFF));

    lcd_cmd(0x2C);
}

void Lcd_Init(void)
{
    lcd_spi_init();
    lcd_gpio_init();

    /* 硬件复位：RES 低 >=10us，拉高后等待 >=120ms */
    GPIO_ResetBits(LCD_RES_PORT, LCD_RES_PIN);
    Delay_ms(10);
    GPIO_SetBits(LCD_RES_PORT, LCD_RES_PIN);
    Delay_ms(120);

    lcd_cmd(0x01);          /* 软复位 */
    Delay_ms(120);
    lcd_cmd(0x11);          /* 退出睡眠 */
    Delay_ms(120);

    lcd_cmd(0x36); lcd_data(0x00);   /* MADCTL：竖屏，RGB */
    lcd_cmd(0x3A); lcd_data(0x05);   /* COLMOD：16bit RGB565 */

    lcd_cmd(0xB2); lcd_data(0x0C); lcd_data(0x0C); lcd_data(0x00);
                   lcd_data(0x33); lcd_data(0x33);
    lcd_cmd(0xB7); lcd_data(0x35);
    lcd_cmd(0xBB); lcd_data(0x19);
    lcd_cmd(0xC0); lcd_data(0x2C);
    lcd_cmd(0xC2); lcd_data(0x01);
    lcd_cmd(0xC3); lcd_data(0x12);
    lcd_cmd(0xC4); lcd_data(0x20);
    lcd_cmd(0xC6); lcd_data(0x0F);
    lcd_cmd(0xD0); lcd_data(0xA4); lcd_data(0xA1);
    lcd_cmd(0xE0); lcd_data(0xD0); lcd_data(0x04); lcd_data(0x0D);
                   lcd_data(0x11); lcd_data(0x13); lcd_data(0x2B);
                   lcd_data(0x3F); lcd_data(0x54); lcd_data(0x4C);
                   lcd_data(0x18); lcd_data(0x0D); lcd_data(0x0B);
                   lcd_data(0x1F); lcd_data(0x23);
    lcd_cmd(0xE1); lcd_data(0xD0); lcd_data(0x04); lcd_data(0x0C);
                   lcd_data(0x11); lcd_data(0x13); lcd_data(0x2C);
                   lcd_data(0x3F); lcd_data(0x44); lcd_data(0x51);
                   lcd_data(0x2F); lcd_data(0x1F); lcd_data(0x1F);
                   lcd_data(0x20); lcd_data(0x23);
    lcd_cmd(0x21);          /* 反显开 */
    lcd_cmd(0x29);          /* 显示开 */

    Lcd_Backlight(1);
    Lcd_Clear(LCD_BLACK);
}

void Lcd_Backlight(uint8_t on)
{
    /* PMOS 栅极：低电平点亮 */
    if (on) GPIO_ResetBits(LCD_BL_PORT, LCD_BL_PIN);
    else    GPIO_SetBits(LCD_BL_PORT, LCD_BL_PIN);
}

void Lcd_Clear(uint16_t color)
{
    Lcd_FillRect(0, 0, LCD_W, LCD_H, color);
}

void Lcd_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    if (x >= LCD_W || y >= LCD_H || w == 0U || h == 0U) return;
    if ((uint32_t)x + w > LCD_W) w = (uint16_t)(LCD_W - x);
    if ((uint32_t)y + h > LCD_H) h = (uint16_t)(LCD_H - y);

    lcd_window(x, y, w, h);
    lcd_data16(color, (uint32_t)w * h);
}

void Lcd_HLine(uint16_t x, uint16_t y, uint16_t w, uint16_t color)
{
    Lcd_FillRect(x, y, w, 1, color);
}

/* 画一个字符：8x8 点阵，按 scale 放大 */
static void lcd_char(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg, uint8_t scale)
{
    const uint8_t *g;
    uint8_t col, row;

    if (c < 32 || c > 126) c = '?';
    g = font8x8[(uint8_t)c - 32];

    Lcd_FillRect(x, y, (uint16_t)(8U * scale), (uint16_t)(8U * scale), bg);

    for (col = 0; col < 8U; col++) {
        uint8_t bits = g[col];

        for (row = 0; row < 8U; row++) {
            if (bits & (uint8_t)(1U << row)) {
                Lcd_FillRect((uint16_t)(x + col * scale),
                             (uint16_t)(y + row * scale),
                             scale, scale, fg);
            }
        }
    }
}

void Lcd_Text(uint16_t x, uint16_t y, const char *s, uint16_t fg, uint16_t bg, uint8_t scale)
{
    uint16_t cx = x;

    if (scale == 0U) scale = 1U;

    while (*s) {
        if (*s == '\n') {
            y = (uint16_t)(y + 8U * scale + 2U);
            cx = x;
        } else {
            lcd_char(cx, y, *s, fg, bg, scale);
            cx = (uint16_t)(cx + 8U * scale);
            if (cx + 8U * scale > LCD_W) {
                cx = x;
                y = (uint16_t)(y + 8U * scale + 2U);
            }
        }
        s++;
    }
}

void Lcd_TextNum(uint16_t x, uint16_t y, int32_t v, uint16_t fg, uint16_t bg, uint8_t scale)
{
    char buf[12];
    uint8_t i = 0;
    uint32_t u;

    if (v < 0) {
        buf[i++] = '-';
        u = (uint32_t)(-v);
    } else {
        u = (uint32_t)v;
    }

    {
        char tmp[10];
        uint8_t n = 0;

        do {
            tmp[n++] = (char)('0' + (u % 10U));
            u /= 10U;
        } while (u && n < 10U);

        while (n && i < 11U) buf[i++] = tmp[--n];
    }
    buf[i] = '\0';

    Lcd_Text(x, y, buf, fg, bg, scale);
}
