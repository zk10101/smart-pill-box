#include "app_command.h"
#include "app_actuator.h"
#include "app_power.h"
#include "app_port.h"
#include "app_types.h"
#include "app_config.h"

/**
 * ============================================================
 * 表现层：指令帧解析与分发
 *
 *   两个通道各自维护一个解析状态机：
 *     UART1(ESP32)  -> 云端指令入口
 *     UART2(摄像头) -> 摄像头应答
 *   解析采用"帧超时自动丢弃"，避免半帧把后续数据带偏。
 * ============================================================
 */

/* 协议常量（帧格式见 app_command.h） */
#define FRAME_HEAD0     0xAA
#define FRAME_HEAD1     0x55

#define CMD_HEARTBEAT   0x01
#define CMD_GET_STATUS  0x02
#define CMD_DISPENSE    0x10
#define CMD_STOP        0x11
#define CMD_HOME        0x12
#define CMD_SET_PARAMS  0x13
#define CMD_GET_PARAMS  0x14
#define CMD_CLEAR_ERROR 0x15
#define CMD_PWR_5V      0x16
#define CMD_MOVE_SLOT   0x17
#define CMD_SYNC_SLOT   0x18
#define CMD_GET_LOG     0x30
#define CMD_CAM_TRIGGER 0x40
#define CMD_CAM_RESULT  0x41
#define CMD_ACK         0x80
#define CMD_STATUS      0x82
#define CMD_LOG         0x83
#define CMD_PARAMS      0x84
#define CMD_EVENT       0x85
#define CMD_SERVER_PUSH 0x90

#define LINK_TIMEOUT_MS 10000U

#define CH_ESP32        UART_CH_ESP32
#define CH_CAM          UART_CH_CAM

/* 解析状态 */
#define PS_HEAD0        0
#define PS_HEAD1        1
#define PS_CMD          2
#define PS_LEN          3
#define PS_DATA         4
#define PS_SUM          5

typedef struct {
    uint8_t  state;
    uint8_t  cmd;
    uint8_t  len;
    uint8_t  idx;
    uint8_t  data[PROTO_MAX_DATA];
    uint32_t t_last;
} parser_t;

static parser_t s_rx[2];
static uint32_t s_last_ping_ms;
static uint32_t s_last_net_ms;
static uint8_t s_ping_seen;
static uint8_t s_net_seen;
static uint8_t s_net_flags;

/* ------------------------------------------------------------ */

static uint8_t rc_to_status(uint8_t rc)
{
    switch (rc) {
    case ACT_RC_OK:       return CMD_ST_OK;
    case ACT_RC_BUSY:     return CMD_ST_BUSY;
    case ACT_RC_PARAM:    return CMD_ST_PARAM;
    case ACT_RC_NOT_HOMED:return CMD_ST_NOT_HOMED;
    case ACT_RC_FAULT:    return CMD_ST_FAULT;
    case ACT_RC_5V_OFF:   return CMD_ST_5V_OFF;
    case ACT_RC_TIMEOUT:  return CMD_ST_TIMEOUT;
    case ACT_RC_LIMIT:    return CMD_ST_LIMIT;
    case ACT_RC_POS_MISMATCH: return CMD_ST_POS_MISMATCH;
    case ACT_RC_NOT_CALIBRATED: return CMD_ST_NOT_CALIBRATED;
    default:              return CMD_ST_FAULT;
    }
}

static void send_frame(uint8_t ch, uint8_t cmd, const uint8_t *data, uint8_t n)
{
    uint8_t  buf[4 + PROTO_MAX_DATA + 1];
    uint8_t  i;
    uint8_t  sum = 0;

    buf[0] = FRAME_HEAD0;
    buf[1] = FRAME_HEAD1;
    buf[2] = cmd;
    buf[3] = n;

    for (i = 0; i < n; i++) {
        buf[4 + i] = data[i];
    }

    sum = (uint8_t)(cmd + n);
    for (i = 0; i < n; i++) {
        sum = (uint8_t)(sum + data[i]);
    }
    buf[4 + n] = sum;

    Uart_Write(ch, buf, (uint16_t)(5 + n));
}

static void send_ack(uint8_t ch, uint8_t cmd, uint8_t status)
{
    uint8_t d[2];

    d[0] = cmd;
    d[1] = status;
    send_frame(ch, CMD_ACK, d, 2);
}

static void send_status(uint8_t ch)
{
    ActStatus st;
    uint8_t   d[14];

    Act_GetStatus(&st);

    d[0] = st.state;
    d[1] = st.error;
    d[2] = st.flags;
    d[3] = st.current_slot;
    d[4] = (uint8_t)(st.position & 0xFF);
    d[5] = (uint8_t)((st.position >> 8) & 0xFF);
    d[6] = (uint8_t)((st.position >> 16) & 0xFF);
    d[7] = (uint8_t)((st.position >> 24) & 0xFF);
    d[8] = (uint8_t)(st.steps_per_slot & 0xFF);
    d[9] = (uint8_t)((st.steps_per_slot >> 8) & 0xFF);
    d[10] = (uint8_t)(st.speed_hz & 0xFF);
    d[11] = (uint8_t)((st.speed_hz >> 8) & 0xFF);

    d[12] = st.target_slot;
    d[13] = st.slot_valid;
    send_frame(ch, CMD_STATUS, d, 14);
}

static void send_params(uint8_t ch)
{
    ActParams p;
    uint8_t   d[7];

    Act_GetParams(&p);

    d[0] = (uint8_t)(p.steps_per_slot & 0xFF);
    d[1] = (uint8_t)((p.steps_per_slot >> 8) & 0xFF);
    d[2] = (uint8_t)(p.speed_hz & 0xFF);
    d[3] = (uint8_t)((p.speed_hz >> 8) & 0xFF);
    d[4] = (uint8_t)((p.en_active_low ? 0x01 : 0x00) | (p.dir_invert ? 0x02 : 0x00));
    d[5] = (uint8_t)(p.home_backoff & 0xFF);
    d[6] = (uint8_t)((p.home_backoff >> 8) & 0xFF);

    send_frame(ch, CMD_PARAMS, d, 7);
}

static void send_log(uint8_t ch, uint8_t want)
{
    uint8_t   d[1 + 20 * 5];
    uint8_t   total = AppLog_Count();
    uint8_t   n;
    uint8_t   i;
    LogEntry  e;

    if (want == 0) {
        want = 1;
    }
    if (want > 20) {
        want = 20;
    }
    n = (total < want) ? total : want;

    d[0] = n;
    for (i = 0; i < n; i++) {
        if (!AppLog_GetNewest(i, &e)) {
            break;
        }
        d[1 + i * 5 + 0] = e.ev;
        d[1 + i * 5 + 1] = e.a;
        d[1 + i * 5 + 2] = e.b;
        d[1 + i * 5 + 3] = (uint8_t)(e.t_ms & 0xFF);
        d[1 + i * 5 + 4] = (uint8_t)((e.t_ms >> 8) & 0xFF);
    }
    d[0] = i;

    send_frame(ch, CMD_LOG, d, (uint8_t)(1 + i * 5));
}

/* 处理一条完整帧 */
static void dispatch(uint8_t ch, uint8_t cmd, const uint8_t *d, uint8_t n)
{
    uint8_t rc;

    if (ch == CH_CAM && cmd != CMD_CAM_RESULT) {
        send_ack(ch, cmd, CMD_ST_PARAM);
        return;
    }

    switch (cmd) {
    case CMD_HEARTBEAT:
        send_ack(ch, cmd, n == 0U ? CMD_ST_OK : CMD_ST_PARAM);
        break;

    case CMD_GET_STATUS:
        if (n == 0U) send_status(ch);
        else send_ack(ch, cmd, CMD_ST_PARAM);
        break;

    case CMD_LINK_PING:
        if (n == 1U) {
            uint8_t reply[7];
            uint32_t uptime = Time_Ms();
            s_last_ping_ms = uptime;
            s_ping_seen = 1U;
            reply[0] = d[0];
            reply[1] = LINK_PROTO_VERSION;
            reply[2] = (uint8_t)uptime;
            reply[3] = (uint8_t)(uptime >> 8);
            reply[4] = (uint8_t)(uptime >> 16);
            reply[5] = (uint8_t)(uptime >> 24);
            reply[6] = (uint8_t)Uart_RxOverflowCount(CH_ESP32);
            send_frame(ch, CMD_LINK_PONG, reply, sizeof(reply));
        } else {
            send_ack(ch, cmd, CMD_ST_PARAM);
        }
        break;

    case CMD_NET_STATUS:
        if (n == 1U && (d[0] & ~0x03U) == 0U) {
            s_net_flags = d[0];
            s_last_net_ms = Time_Ms();
            s_net_seen = 1U;
            send_ack(ch, cmd, CMD_ST_OK);
        } else {
            send_ack(ch, cmd, CMD_ST_PARAM);
        }
        break;

    case CMD_DISPENSE:
        send_ack(ch, cmd, CMD_ST_PARAM); /* 旧按粒数动作已停用 */
        break;

    case CMD_STOP:
        Act_Stop();
        send_ack(ch, cmd, CMD_ST_OK);
        break;

    case CMD_HOME:
        send_ack(ch, cmd, CMD_ST_PARAM); /* 无原点开关时禁止盲目回零 */
        break;

    case CMD_SET_PARAMS:
        if (n != 7) {
            send_ack(ch, cmd, CMD_ST_PARAM);
            break;
        } else {
            ActParams p;

            Act_GetParams(&p);
            p.steps_per_slot = (uint16_t)(((uint16_t)d[0] << 8) | d[1]);
            p.speed_hz       = (uint16_t)(((uint16_t)d[2] << 8) | d[3]);
            p.en_active_low  = (uint8_t)((d[4] & 0x01) ? 1 : 0);
            p.dir_invert     = (uint8_t)((d[4] & 0x02) ? 1 : 0);
            p.home_backoff   = (uint16_t)(((uint16_t)d[5] << 8) | d[6]);
            rc = Act_SetParams(&p);
            send_ack(ch, cmd, rc_to_status(rc));
        }
        break;

    case CMD_GET_PARAMS:
        send_params(ch);
        break;

    case CMD_MOVE_SLOT:
        if (ch != CH_ESP32 || n != 2) {
            send_ack(ch, cmd, CMD_ST_PARAM);
            break;
        }
        rc = Act_MoveToSlot(d[0], d[1]);
        send_ack(ch, cmd, rc_to_status(rc));
        break;

    case CMD_SYNC_SLOT:
        if (ch != CH_ESP32 || n != 1) {
            send_ack(ch, cmd, CMD_ST_PARAM);
            break;
        }
        rc = Act_SyncSlot(d[0]);
        send_ack(ch, cmd, rc_to_status(rc));
        break;

    case CMD_CLEAR_ERROR:
        Act_ClearError();
        send_ack(ch, cmd, CMD_ST_OK);
        break;

    case CMD_PWR_5V:
        if (n != 1) {
            send_ack(ch, cmd, CMD_ST_PARAM);
            break;
        }
        if (d[0] == 0U && Act_IsBusy()) {
            send_ack(ch, cmd, CMD_ST_BUSY);
            break;
        }
        if (d[0]) {
            send_ack(ch, cmd, Pwr_Ensure5V() ? CMD_ST_OK : CMD_ST_5V_OFF);
        } else {
            send_ack(ch, cmd, Pwr_Shutdown5V() ? CMD_ST_OK : CMD_ST_5V_OFF);
        }
        break;

    case CMD_GET_LOG:
        send_log(ch, (n >= 1) ? d[0] : 1);
        break;

    case CMD_CAM_TRIGGER:
        send_ack(ch, cmd, CMD_ST_PARAM); /* 摄像头流程本期不接入 */
        break;

    case CMD_CAM_RESULT:
        send_ack(ch, cmd, CMD_ST_PARAM);
        break;

    case CMD_SERVER_PUSH:
        send_ack(ch, cmd, CMD_ST_PARAM); /* 云端业务协议未定，不能假报执行成功 */
        break;

    default:
        send_ack(ch, cmd, CMD_ST_PARAM);
        break;
    }
}

/* 逐字节喂入解析状态机 */
static void parse_byte(uint8_t ch, uint8_t b)
{
    parser_t *p = &s_rx[ch];

    p->t_last = Time_Ms();

    switch (p->state) {
    case PS_HEAD0:
        if (b == FRAME_HEAD0) {
            p->state = PS_HEAD1;
        }
        break;

    case PS_HEAD1:
        if (b == FRAME_HEAD1) {
            p->state = PS_CMD;
        } else if (b == FRAME_HEAD0) {
            p->state = PS_HEAD1;      /* 重复的 AA，继续等 55 */
        } else {
            p->state = PS_HEAD0;
        }
        break;

    case PS_CMD:
        p->cmd   = b;
        p->state = PS_LEN;
        break;

    case PS_LEN:
        p->len = b;
        p->idx = 0;
        if (b == 0) {
            p->state = PS_SUM;        /* 无数据帧：下一个字节就是校验和 */
        } else if (b > PROTO_MAX_DATA) {
            p->state = PS_HEAD0;
        } else {
            p->state = PS_DATA;
        }
        break;

    case PS_DATA:
        p->data[p->idx++] = b;
        if (p->idx >= p->len) {
            p->state = PS_SUM;
        }
        break;

    case PS_SUM:
    default: {
        uint8_t sum = (uint8_t)(p->cmd + p->len);
        uint8_t i;

        for (i = 0; i < p->len; i++) {
            sum = (uint8_t)(sum + p->data[i]);
        }
        if (sum == b) {
            dispatch(ch, p->cmd, p->data, p->len);
        }
        p->state = PS_HEAD0;
        break;
    }
    }
}

/* ------------------------------------------------------------ */
void Cmd_Init(void)
{
    Uart_Init();

    s_ping_seen = 0U;
    s_net_seen = 0U;
    s_net_flags = 0U;
    s_last_ping_ms = 0U;
    s_last_net_ms = 0U;

    s_rx[0].state = PS_HEAD0;
    s_rx[1].state = PS_HEAD0;
    s_rx[0].t_last = Time_Ms();
    s_rx[1].t_last = Time_Ms();
}

void Cmd_Tick(void)
{
    uint8_t ch;
    uint8_t b;

    for (ch = 0; ch < 2; ch++) {
        while (Uart_Read(ch, &b)) {
            parse_byte(ch, b);
        }

        /* 半帧超时：直接丢弃，等待下一个帧头 */
        if (s_rx[ch].state != PS_HEAD0 &&
            Time_Elapsed(s_rx[ch].t_last, PROTO_FRAME_TIMEOUT_MS)) {
            s_rx[ch].state = PS_HEAD0;
        }
    }
}

void Cmd_ReportEvent(uint8_t ev, uint8_t a, uint8_t b)
{
    uint8_t d[3];

    d[0] = ev;
    d[1] = a;
    d[2] = b;
    send_frame(CH_ESP32, CMD_EVENT, d, 3);
}

void Cmd_GetLinkStatus(uint8_t *uart_ok, uint8_t *wifi_ok, uint8_t *mqtt_ok)
{
    uint8_t uart = (uint8_t)(s_ping_seen &&
        !Time_Elapsed(s_last_ping_ms, LINK_TIMEOUT_MS));
    uint8_t net = (uint8_t)(uart && s_net_seen &&
        !Time_Elapsed(s_last_net_ms, LINK_TIMEOUT_MS));

    *uart_ok = uart;
    *wifi_ok = (uint8_t)(net && (s_net_flags & 0x01U));
    *mqtt_ok = (uint8_t)(net && (s_net_flags & 0x02U));
}
