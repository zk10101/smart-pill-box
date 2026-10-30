#include "bsp_lcd.h"
#include "pinmap.h"
#include "stm32f10x.h"
#include "Delay.h"

/* ---- SPI1 软总线接口（与 W25Q 共用 PA5/PA6/PA7，各自独立 CS） ---- */

static void SPI1_Init(void)
{
    GPIO_InitTypeDef g;
    SPI_InitTypeDef  s;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_SPI1, ENABLE);

    g.GPIO_Mode  = GPIO_Mode_AF_PP;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    g.GPIO_Pin   = SPI1_SCK_PIN | SPI1_MOSI_PIN;
    GPIO_Init(GPIOA, &g);

    g.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
    g.GPIO_Pin   = SPI1_MISO_PIN;
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

static uint8_t SPI1_RW(uint8_t byte)
{
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET);
    SPI_I2S_SendData(SPI1, byte);
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET);
    return (uint8_t)SPI_I2S_ReceiveData(SPI1);
}

/* ---- LCD 控制引脚 ---- */
static void LCD_Ctrl_Init(void)
{
    GPIO_InitTypeDef g;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);
    g.GPIO_Mode  = GPIO_Mode_Out_PP;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    g.GPIO_Pin   = LCD_CS_PIN;
    GPIO_Init(LCD_CS_PORT, &g);
    g.GPIO_Pin   = LCD_DC_PIN | LCD_RES_PIN | LCD_BL_PIN;
    GPIO_Init(GPIOB, &g);
}

static void LCD_CS(uint8_t sel)  { (sel) ? GPIO_SetBits(LCD_CS_PORT, LCD_CS_PIN) : GPIO_ResetBits(LCD_CS_PORT, LCD_CS_PIN); }
static void LCD_DC(uint8_t d)    { (d) ? GPIO_SetBits(LCD_DC_PORT, LCD_DC_PIN) : GPIO_ResetBits(LCD_DC_PORT, LCD_DC_PIN); }

static void LCD_WriteCmd(uint8_t cmd)
{
    LCD_CS(0);
    LCD_DC(0);
    SPI1_RW(cmd);
    LCD_CS(1);
}
static void LCD_WriteData(uint8_t dat)
{
    LCD_CS(0);
    LCD_DC(1);
    SPI1_RW(dat);
    LCD_CS(1);
}

void LCD_Init(void)
{
    SPI1_Init();
    LCD_Ctrl_Init();

    /* 硬件复位（ST7789 要求 RESX 低电平 >=10us，之后等待 >=120ms 再发命令） */
    GPIO_ResetBits(LCD_RES_PORT, LCD_RES_PIN);
    Delay_ms(10);
    GPIO_SetBits(LCD_RES_PORT, LCD_RES_PIN);
    Delay_ms(120);

    /* ST7789V 初始化序列 */
    LCD_WriteCmd(0x01);   /* 软件复位 */
    Delay_ms(120);        /* 软复位后必须等待 120ms */
    LCD_WriteCmd(0x11);   /* 退出休眠 */
    Delay_ms(120);
    LCD_WriteCmd(0x36); LCD_WriteData(0x00);   /* MADCTL */
    LCD_WriteCmd(0x3A); LCD_WriteData(0x05);   /* 16bit RGB565 */
    LCD_WriteCmd(0xB2); LCD_WriteData(0x0C); LCD_WriteData(0x0C);
    LCD_WriteData(0x00); LCD_WriteData(0x33); LCD_WriteData(0x33);
    LCD_WriteCmd(0xB7); LCD_WriteData(0x35);
    LCD_WriteCmd(0xBB); LCD_WriteData(0x19);
    LCD_WriteCmd(0xC0); LCD_WriteData(0x2C);
    LCD_WriteCmd(0xC2); LCD_WriteData(0x01);
    LCD_WriteCmd(0xC3); LCD_WriteData(0x12);
    LCD_WriteCmd(0xC4); LCD_WriteData(0x20);
    LCD_WriteCmd(0xC6); LCD_WriteData(0x0F);
    LCD_WriteCmd(0xD0); LCD_WriteData(0xA4); LCD_WriteData(0xA1);
    LCD_WriteCmd(0xE0); LCD_WriteData(0xD0); LCD_WriteData(0x04);
    LCD_WriteData(0x0D); LCD_WriteData(0x11); LCD_WriteData(0x13);
    LCD_WriteData(0x2B); LCD_WriteData(0x3F); LCD_WriteData(0x54);
    LCD_WriteData(0x4C); LCD_WriteData(0x18); LCD_WriteData(0x0D);
    LCD_WriteData(0x0B); LCD_WriteData(0x1F); LCD_WriteData(0x23);
    LCD_WriteCmd(0xE1); LCD_WriteData(0xD0); LCD_WriteData(0x04);
    LCD_WriteData(0x0C); LCD_WriteData(0x11); LCD_WriteData(0x13);
    LCD_WriteData(0x2C); LCD_WriteData(0x3F); LCD_WriteData(0x44);
    LCD_WriteData(0x51); LCD_WriteData(0x2F); LCD_WriteData(0x1F);
    LCD_WriteData(0x1F); LCD_WriteData(0x20); LCD_WriteData(0x23);
    LCD_WriteCmd(0x21);   /* 反显开 */
    LCD_WriteCmd(0x29);   /* 显示开 */

    LCD_SetBacklight(1);
    LCD_Fill(LCD_BLACK);
}

void LCD_SetBacklight(uint8_t on)
{
    if (on) GPIO_ResetBits(LCD_BL_PORT, LCD_BL_PIN);   /* 低电平点亮(PMOS) */
    else    GPIO_SetBits(LCD_BL_PORT, LCD_BL_PIN);
}

void LCD_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    /* 加偏移(见 bsp_lcd.h: 240x240 玻璃一般需要 Y 偏移 80) */
    x0 += LCD_X_OFFSET; x1 += LCD_X_OFFSET;
    y0 += LCD_Y_OFFSET; y1 += LCD_Y_OFFSET;

    LCD_WriteCmd(0x2A);
    LCD_WriteData(x0 >> 8); LCD_WriteData(x0 & 0xFF);
    LCD_WriteData(x1 >> 8); LCD_WriteData(x1 & 0xFF);
    LCD_WriteCmd(0x2B);
    LCD_WriteData(y0 >> 8); LCD_WriteData(y0 & 0xFF);
    LCD_WriteData(y1 >> 8); LCD_WriteData(y1 & 0xFF);
    LCD_WriteCmd(0x2C);
}

void LCD_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    LCD_SetWindow(x, y, x, y);
    LCD_WriteData(color >> 8);
    LCD_WriteData(color & 0xFF);
}

void LCD_Fill(uint16_t color)
{
    uint32_t i;
    LCD_SetWindow(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);
    for (i = 0; i < (uint32_t)LCD_WIDTH * LCD_HEIGHT; i++) {
        LCD_WriteData(color >> 8);
        LCD_WriteData(color & 0xFF);
    }
}
