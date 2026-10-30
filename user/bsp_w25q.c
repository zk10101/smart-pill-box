#include "bsp_w25q.h"
#include "pinmap.h"
#include "stm32f10x.h"

/* 复用 bsp_lcd.c 里已初始化的 SPI1 硬件，这里只实现 W25Q 的 CS 与读写时序。
 * SPI1 初始化由 LCD_Init 完成；若先于 LCD 使用，需自行确保 SPI1 已使能。 */

static void W25Q_CS(uint8_t sel)
{
    if (sel) GPIO_SetBits(FLASH_CS_PORT, FLASH_CS_PIN);
    else     GPIO_ResetBits(FLASH_CS_PORT, FLASH_CS_PIN);
}

static uint8_t W25Q_SPI_RW(uint8_t byte)
{
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET);
    SPI_I2S_SendData(SPI1, byte);
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET);
    return (uint8_t)SPI_I2S_ReceiveData(SPI1);
}

void W25Q_Init(void)
{
    GPIO_InitTypeDef g;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    g.GPIO_Mode  = GPIO_Mode_Out_PP;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    g.GPIO_Pin   = FLASH_CS_PIN;
    GPIO_Init(FLASH_CS_PORT, &g);
    W25Q_CS(1);
}

static void W25Q_WriteEnable(void)
{
    W25Q_CS(0);
    W25Q_SPI_RW(W25Q_CMD_WRITE_EN);
    W25Q_CS(1);
}

uint16_t W25Q_ReadID(void)
{
    uint16_t id;
    W25Q_CS(0);
    W25Q_SPI_RW(W25Q_CMD_READ_ID);
    id  = (uint16_t)W25Q_SPI_RW(0xFF) << 8;
    id |= W25Q_SPI_RW(0xFF);
    W25Q_CS(1);
    return id;
}

void W25Q_Read(uint32_t addr, uint8_t *buf, uint32_t length)
{
    W25Q_CS(0);
    W25Q_SPI_RW(W25Q_CMD_READ);
    W25Q_SPI_RW((uint8_t)(addr >> 16));
    W25Q_SPI_RW((uint8_t)(addr >> 8));
    W25Q_SPI_RW((uint8_t)(addr));
    while (length--) *buf++ = W25Q_SPI_RW(0xFF);
    W25Q_CS(1);
}

void W25Q_PageProgram(uint32_t addr, const uint8_t *buf, uint16_t length)
{
    W25Q_WriteEnable();
    W25Q_CS(0);
    W25Q_SPI_RW(W25Q_CMD_PAGE_PROG);
    W25Q_SPI_RW((uint8_t)(addr >> 16));
    W25Q_SPI_RW((uint8_t)(addr >> 8));
    W25Q_SPI_RW((uint8_t)(addr));
    while (length--) W25Q_SPI_RW(*buf++);
    W25Q_CS(1);
    /* 等待写入完成(状态寄存器 BUSY 清零)，此处简化延时 */
    { volatile uint32_t d; for (d = 0; d < 100000; d++); }
}

void W25Q_SectorErase(uint32_t addr)
{
    W25Q_WriteEnable();
    W25Q_CS(0);
    W25Q_SPI_RW(W25Q_CMD_SECTOR_ERASE);
    W25Q_SPI_RW((uint8_t)(addr >> 16));
    W25Q_SPI_RW((uint8_t)(addr >> 8));
    W25Q_SPI_RW((uint8_t)(addr));
    W25Q_CS(1);
    { volatile uint32_t d; for (d = 0; d < 2000000; d++); }
}
