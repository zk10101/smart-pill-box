#ifndef __BSP_W25Q_H
#define __BSP_W25Q_H
#include <stdint.h>

/* W25Q64 (8MB) SPI Flash，与 LCD 共用 SPI1 总线
 * !! 注意: 原理图 FLASH_CS 尚未连到 MCU，见 pinmap.h TODO !!
 * 命令集 */
#define W25Q_CMD_READ_ID    0x9F
#define W25Q_CMD_READ       0x03
#define W25Q_CMD_FAST_READ  0x0B
#define W25Q_CMD_PAGE_PROG  0x02
#define W25Q_CMD_SECTOR_ERASE 0x20
#define W25Q_CMD_WRITE_EN   0x06
#define W25Q_CMD_READ_STATUS 0x05

void W25Q_Init(void);
uint16_t W25Q_ReadID(void);
/* 读取 length 字节到 buf（地址 addr，0~8M） */
void W25Q_Read(uint32_t addr, uint8_t *buf, uint32_t length);
/* 写一页（<=256 字节，不跨页） */
void W25Q_PageProgram(uint32_t addr, const uint8_t *buf, uint16_t length);
void W25Q_SectorErase(uint32_t addr);

#endif
