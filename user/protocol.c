#include "protocol.h"

uint8_t Protocol_Checksum(const uint8_t *pkt, uint8_t len)
{
    uint32_t sum = 0;
    uint8_t i;
    for (i = 0; i < len; i++) {
        sum += pkt[i];
    }
    return (uint8_t)(sum & 0xFF);
}

uint16_t Protocol_Pack(uint8_t *buf, uint8_t cmd, const uint8_t *data, uint8_t len)
{
    uint8_t i;
    buf[0] = PROTO_HEAD0;
    buf[1] = PROTO_HEAD1;
    buf[2] = cmd;
    buf[3] = len;
    for (i = 0; i < len; i++) {
        buf[4 + i] = data[i];
    }
    buf[4 + len] = Protocol_Checksum(&buf[2], 2 + len);
    return (uint16_t)(5 + len);
}

uint8_t Protocol_ParseByte(ProtoParser *ps, uint8_t byte, ProtoFrame *out)
{
    switch (ps->state) {
    case 0: /* 等帧头 0 */
        if (byte == PROTO_HEAD0) {
            ps->state = 1;
        }
        break;
    case 1: /* 等帧头 1 */
        if (byte == PROTO_HEAD1) {
            ps->state = 2;
        } else if (byte != PROTO_HEAD0) {
            ps->state = 0;          /* 允许 AA AA 55 这种连续帧头 */
        }
        break;
    case 2: /* 命令 */
        ps->cmd = byte;
        ps->state = 3;
        break;
    case 3: /* 长度 */
        ps->len = byte;
        ps->idx = 0;
        if (ps->len > PROTO_MAX_LEN) {
            ps->state = 0;          /* 非法长度，重同步 */
        } else {
            ps->state = 4;
        }
        break;
    case 4: /* 数据 */
        ps->data[ps->idx++] = byte;
        if (ps->idx >= ps->len) {
            ps->state = 5;
        }
        break;
    case 5: /* 校验 */
        ps->check_sum = byte;
        ps->state = 0;
        {
            uint8_t sum = ps->cmd + ps->len;
            uint8_t i;
            for (i = 0; i < ps->len; i++) {
                sum += ps->data[i];
            }
            if (((uint8_t)(sum & 0xFF)) == ps->check_sum) {
                out->head0 = PROTO_HEAD0;
                out->head1 = PROTO_HEAD1;
                out->cmd   = ps->cmd;
                out->len   = ps->len;
                for (i = 0; i < ps->len; i++) {
                    out->data[i] = ps->data[i];
                }
                return 1;
            }
        }
        break;
    default:
        ps->state = 0;
        break;
    }
    return 0;
}
