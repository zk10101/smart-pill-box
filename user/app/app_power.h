#ifndef __APP_POWER_H
#define __APP_POWER_H
/**
 * ============================================================
 * 业务逻辑层：电源（IP5306）管理
 *
 *   3V3 由电池 BAT 经 SY8089 降压得到，只要电池在位 MCU 就上电；
 *   5V 由 IP5306 升压得到，默认是关闭的，需要"按一下"KEY 才会打开。
 *   PWR_KEY(PC11) 高电平 = 经 Q3 拉低 KEY，脉宽 ~120ms。
 *
 *   本层把"按一下"这一物理动作封装成 Ensure5V/Shutdown5V，
 *   并负责等待 5V 建立（PC9 反馈）。
 * ============================================================
 */
#include <stdint.h>

void    Pwr_Init(void);
uint8_t Pwr_Ensure5V(void);       /* 1=5V 已建立 */
uint8_t Pwr_Shutdown5V(void);     /* 1=5V 已关闭 */
uint8_t Pwr_Is5V(void);
uint8_t Pwr_IsUsb(void);

#endif /* __APP_POWER_H */
