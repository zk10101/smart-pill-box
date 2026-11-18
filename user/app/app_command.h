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
 *     0x02 GET_STATUS    无数据           -> 0x82 STATUS(14B)
 *     0x03 LINK_PING     [nonce]          -> 0x86 LINK_PONG(7B)
 *     0x10 DISPENSE      已停用
 *     0x11 STOP          无数据           -> 0x80 ACK
 *     0x12 HOME          已停用
 *     0x13 SET_PARAMS    [sppH,sppL,spdH,spdL,flags,backoffH,backoffL] -> ACK
 *     0x14 GET_PARAMS    无数据           -> 0x84 PARAMS(7B)
 *     0x15 CLEAR_ERROR   无数据           -> 0x80 ACK
 *     0x16 PWR_5V        [0/1]            -> 0x80 ACK
 *     0x19 NET_STATUS    [flags]          -> 0x80 ACK
 *     0x17 MOVE_SLOT     [当前格,目标格]  -> 0x80 ACK
 *     0x18 SYNC_SLOT     [当前格]         -> 0x80 ACK
 *     0x30 GET_LOG       [条数]           -> 0x83 LOG(1+n*5)
 *     0x40 CAM_TRIGGER   本期停用
 *     0x41 CAM_RESULT    本期停用
 *     0x80 ACK           [cmd,status]（回包）
 *     0x82 STATUS        [state,error,flags,current,pos(4),steps/slot(2),speed(2),target,valid]
 *     0x83 LOG           [count, {ev,a,b,t(2)}...]
 *     0x84 PARAMS        [spp(2),spd(2),flags,backoff(2)]
 *     0x85 EVENT         [ev,a,b]（主动上报）
 *     0x90 SERVER_PUSH   已停用；云端指令格式尚未确定
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
#define CMD_ST_POS_MISMATCH 0x08
#define CMD_ST_NOT_CALIBRATED 0x09

/* ESP32-C3 diagnostic exchange. Both sides use the same additive frame checksum. */
#define CMD_LINK_PING       0x03
#define CMD_NET_STATUS      0x19
#define CMD_LINK_PONG       0x86
#define LINK_PROTO_VERSION  0x01

void     Cmd_Init(void);
void     Cmd_Tick(void);                 /* 主循环调用：收帧 + 分发 */
void     Cmd_ReportEvent(uint8_t ev, uint8_t a, uint8_t b);   /* 主动上报到 ESP32 */
void     Cmd_GetLinkStatus(uint8_t *uart_ok, uint8_t *wifi_ok, uint8_t *mqtt_ok);

#endif /* __APP_COMMAND_H */
