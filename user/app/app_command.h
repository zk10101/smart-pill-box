#ifndef __APP_COMMAND_H
#define __APP_COMMAND_H
/**
 * ============================================================
 * 表现层：串口指令接口（对上位机 / 云端 / 摄像头）
 *
 *   通道：UART1 = ESP32-C3（上行，云端指令从这里进来）
 *         UART2 = 摄像头 MaixBit(K210)（下行，发触发、收识别结果）
 *
 *   帧格式：0xAA 0x55 | CMD | LEN | DATA[LEN] | SUM
 *           SUM = (CMD + LEN + ΣDATA) & 0xFF
 *
 *   指令表（完整说明见 docs/串口指令协议.md）：
 *     0x01 HEARTBEAT     无数据           -> 0x80 ACK
 *     0x02 GET_STATUS    无数据           -> 0x82 STATUS(12B)
 *     0x10 DISPENSE      [粒数]           -> 0x80 ACK
 *     0x11 STOP          无数据           -> 0x80 ACK
 *     0x12 HOME          无数据           -> 0x80 ACK
 *     0x13 SET_PARAMS    [sppH,sppL,spdH,spdL,flags,backoffH,backoffL] -> ACK
 *     0x14 GET_PARAMS    无数据           -> 0x84 PARAMS(7B)
 *     0x15 CLEAR_ERROR   无数据           -> 0x80 ACK
 *     0x16 PWR_5V        [0/1]            -> 0x80 ACK
 *     0x30 GET_LOG       [条数]           -> 0x83 LOG(1+n*5)
 *     0x40 CAM_TRIGGER   无数据           -> ACK，并转发给摄像头
 *     0x41 CAM_RESULT    [found,count,conf]（摄像头 -> 主板）
 *     0x80 ACK           [cmd,status]（回包）
 *     0x82 STATUS        [state,error,flags,pills,pos(4),spp(2),spd(2)]
 *     0x83 LOG           [count, {ev,a,b,t(2)}...]
 *     0x84 PARAMS        [spp(2),spd(2),flags,backoff(2)]
 *     0x85 EVENT         [ev,a,b]（主动上报）
 *     0x90 SERVER_PUSH   [..]（云端下发，本期仅记录）
 * ============================================================
 */
#include <stdint.h>

/* ACK 状态码 */
#define CMD_ST_OK           0x00
#define CMD_ST_BUSY         0x01
#define CMD_ST_PARAM        0x02
#define CMD_ST_NOT_HOMED    0x03
#define CMD_ST_FAULT        0x04
#define CMD_ST_5V_OFF       0x05
#define CMD_ST_TIMEOUT      0x06
#define CMD_ST_LIMIT        0x07

void     Cmd_Init(void);
void     Cmd_Tick(void);                 /* 主循环调用：收帧 + 分发 */
void     Cmd_ReportEvent(uint8_t ev, uint8_t a, uint8_t b);   /* 主动上报到 ESP32 */
uint8_t  Cmd_TriggerCamera(void);        /* 1=已发出触发帧 */

#endif /* __APP_COMMAND_H */
