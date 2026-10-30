#ifndef __PROTOCOL_H
#define __PROTOCOL_H
#include <stdint.h>

/**
 * ============================================================
 * 串口通信协议（STM32 <-> ESP32-C3 单路 USART1）
 * 帧格式: 帧头(2) + 命令(1) + 长度(1) + 数据(N) + 校验(1)
 *   帧头   : 0xAA 0x55
 *   长度   : 数据段字节数(0~255)
 *   校验   : 从命令字节到数据末字节的累加和取低8位
 * 本文件与 ESP32 侧、APP 侧保持一致（成员间接口契约）
 * ============================================================
 */

#define PROTO_HEAD0         0xAA
#define PROTO_HEAD1         0x55
#define PROTO_MAX_LEN       128     /* 单帧最大数据长度 */

/* ---------------- 命令码（STM32 -> ESP32 / APP） ---------------- */
#define CMD_HEARTBEAT       0x01    /* 心跳 */
#define CMD_GET_STATUS      0x02    /* 查询设备状态 */
#define CMD_DISPENSE        0x10    /* 出药指令 */
#define CMD_STOP            0x11    /* 紧急停止 */
#define CMD_SET_SCHEDULE    0x20    /* 下发服药计划 */
#define CMD_GET_LOG         0x30    /* 读取日志 */
#define CMD_ACK             0x80    /* 通用应答 */

/* ---------------- 命令码（ESP32 -> STM32） ---------------- */
#define CMD_SERVER_PUSH     0x90    /* 云端下发(透传) */
#define CMD_DETECT_RESULT   0x91    /* 视觉检测结果 */

/* ---------------- 状态码 ---------------- */
#define STATUS_OK           0x00
#define STATUS_BUSY         0x01
#define STATUS_ERROR        0x02
#define STATUS_TIMEOUT      0x03
#define STATUS_MISSED       0x04    /* 漏服 */
#define STATUS_TAKEN        0x05    /* 已取药 */

/* 帧结构 */
typedef struct {
    uint8_t head0;
    uint8_t head1;
    uint8_t cmd;
    uint8_t len;
    uint8_t data[PROTO_MAX_LEN];
} ProtoFrame;

/* 字节流解析器状态 */
typedef struct {
    uint8_t state;              /* 0=等帧头0 1=等帧头1 2=收命令 3=收长度 4=收数据 5=收校验 */
    uint8_t idx;
    uint8_t cmd;
    uint8_t len;
    uint8_t data[PROTO_MAX_LEN];
    uint8_t check_sum;
} ProtoParser;

/* 校验: 累加和(命令+长度+数据) 取低8位 */
uint8_t Protocol_Checksum(const uint8_t *pkt, uint8_t len); /* len = 数据长度 */
/* 打包一帧到 buf，返回总帧长；buf 需 >= len+5 */
uint16_t Protocol_Pack(uint8_t *buf, uint8_t cmd, const uint8_t *data, uint8_t len);
/* 喂入一个字节，返回 1 表示解析出一整帧(结果存入 out) */
uint8_t Protocol_ParseByte(ProtoParser *ps, uint8_t byte, ProtoFrame *out);

#endif /* __PROTOCOL_H */
