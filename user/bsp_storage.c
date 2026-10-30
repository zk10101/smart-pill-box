#include "bsp_storage.h"
#include "app_config.h"
#include "stm32f10x.h"
#include <string.h>

/* RCT6 共 256K Flash(0x08000000~0x0803FFFF)，高密度器件页大小 2KB。
 * 使用最后 1 页 0x0803F800~0x0803FFFF 存服药计划，避开程序区。 */
#define STORAGE_ADDR    0x0803F800UL
#define STORAGE_MAGIC   0x50425831UL   /* "P1BX" 自定义魔数 */

/* 存储块布局: magic(4) + num(1) + 保留(3) + items[num] */
typedef struct {
    uint32_t      magic;
    uint8_t       num;
    uint8_t       rsvd[3];
    ScheduleItem  items[SCHEDULE_MAX];
} StorageBlock;

void Storage_Init(void)
{
    /* 预留：可在此做首次上电初始化判断 */
}

uint8_t Storage_SaveSchedule(const ScheduleItem *sched, uint8_t num)
{
    StorageBlock blk;
    uint16_t *pw;
    uint32_t i, words;

    if (num > SCHEDULE_MAX) return 0;

    memset(&blk, 0, sizeof(blk));
    blk.magic = STORAGE_MAGIC;
    blk.num   = num;
    if (num) memcpy(blk.items, sched, num * sizeof(ScheduleItem));

    FLASH_Unlock();
    FLASH_ErasePage(STORAGE_ADDR);              /* 整页擦除 */

    pw = (uint16_t *)&blk;
    words = (sizeof(blk) + 1) / 2;              /* 按半字写入 */
    for (i = 0; i < words; i++) {
        if (FLASH_ProgramHalfWord(STORAGE_ADDR + i * 2, pw[i]) != FLASH_COMPLETE) {
            FLASH_Lock();
            return 0;
        }
    }
    FLASH_Lock();
    return 1;
}

uint8_t Storage_LoadSchedule(ScheduleItem *sched, uint8_t *num)
{
    StorageBlock *blk = (StorageBlock *)STORAGE_ADDR;
    if (blk->magic != STORAGE_MAGIC) return 0;
    if (blk->num > SCHEDULE_MAX) return 0;
    *num = blk->num;
    if (blk->num) memcpy(sched, blk->items, blk->num * sizeof(ScheduleItem));
    return 1;
}
