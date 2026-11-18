#include "app_port.h"
#include "app_config.h"
#include "stm32f10x.h"

/**
 * ============================================================
 * 数据访问层：内部 Flash 参数存储
 *
 *   STM32F103RCT6 的 Flash 为 256KB，页大小 2KB，
 *   最后一页 = 0x0803F800 ~ 0x0803FFFF，专用于保存 ActParams。
 *
 *   存储块格式（半字写入）：
 *     [0] magic  0x5042 ("PB")
 *     [1] len    参数长度
 *     [2..] ActParams 字段
 *     末尾 crc 字段由 app 计算（见 app_actuator.c 的 params_crc）
 * ============================================================
 */

#define PARAM_PAGE_ADDR     0x0803F800UL
#define PARAM_MAGIC         0x5043U  /* 旧版 steps_per_pill 不可当作已标定的格距 */

static uint16_t params_crc16(const uint8_t *p, uint16_t len)
{
    uint16_t crc = 0xA55AU;
    uint16_t i;

    for (i = 0; i < len; i++) {
        crc ^= (uint16_t)p[i];
        crc = (uint16_t)((crc << 1) ^ ((crc & 0x8000U) ? 0x1021U : 0U));
    }
    return crc;
}

/* 供业务层计算 CRC（app_actuator.c 保存前调用） */
uint16_t Flash_ParamsCrc(const ActParams *p)
{
    return params_crc16((const uint8_t *)p, (uint16_t)(sizeof(ActParams) - sizeof(uint16_t)));
}

uint8_t Flash_LoadParams(ActParams *p)
{
    const uint16_t *src = (const uint16_t *)PARAM_PAGE_ADDR;
    uint16_t magic = src[0];
    uint16_t len   = src[1];
    uint16_t i;
    uint16_t *dst = (uint16_t *)p;

    if (magic != PARAM_MAGIC) return 0;
    if (len != (uint16_t)sizeof(ActParams)) return 0;

    for (i = 0; i < (uint16_t)(sizeof(ActParams) / 2U); i++) dst[i] = src[2U + i];

    if (params_crc16((const uint8_t *)p, (uint16_t)(sizeof(ActParams) - sizeof(uint16_t)))
        != p->crc) {
        return 0;
    }
    return 1;
}

uint8_t Flash_SaveParams(const ActParams *p)
{
    const uint16_t *src = (const uint16_t *)p;
    uint16_t i;
    uint16_t n = (uint16_t)(sizeof(ActParams) / 2U);

    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
    if (FLASH_ErasePage(PARAM_PAGE_ADDR) != FLASH_COMPLETE) {
        FLASH_Lock();
        return 0;
    }

    if (FLASH_ProgramHalfWord(PARAM_PAGE_ADDR, PARAM_MAGIC) != FLASH_COMPLETE) {
        FLASH_Lock();
        return 0;
    }
    if (FLASH_ProgramHalfWord(PARAM_PAGE_ADDR + 2U, (uint16_t)sizeof(ActParams)) != FLASH_COMPLETE) {
        FLASH_Lock();
        return 0;
    }
    for (i = 0; i < n; i++) {
        if (FLASH_ProgramHalfWord(PARAM_PAGE_ADDR + 4U + i * 2U, src[i]) != FLASH_COMPLETE) {
            FLASH_Lock();
            return 0;
        }
    }

    FLASH_Lock();
    return 1;
}
