#include "app_ui.h"
#include "app_actuator.h"
#include "app_power.h"
#include "app_port.h"
#include "app_types.h"
#include "app_config.h"
#include "app_command.h"

/**
 * ============================================================
 * 表现层：本地界面
 *   只通过 app_port.h（数据访问层接口）与业务层 API 工作，
 *   不直接操作任何寄存器/引脚。
 * ============================================================
 */

/* 屏幕上一次绘制的内容，用于避免整屏重绘造成的闪烁与卡顿 */
static struct {
    uint8_t  state;
    uint8_t  error;
    uint8_t  flags;
    uint8_t  current;
    uint8_t  target;
    uint8_t  valid;
    uint8_t  edit;
    uint32_t position;
    uint16_t spp;
    uint16_t speed;
    uint8_t  uart_ok;
    uint8_t  wifi_ok;
    uint8_t  mqtt_ok;
} s_last;

static uint8_t s_blink;
static uint8_t s_edit;
static uint16_t s_draft_steps;

static const char *state_text(uint8_t st)
{
    switch (st) {
    case ACT_ST_IDLE:   return "IDLE  ";
    case ACT_ST_HOMING: return "HOMING";
    case ACT_ST_MOVING: return "MOVING";
    case ACT_ST_ERROR:  return "ERROR ";
    default:            return "?     ";
    }
}

static const char *error_text(uint8_t err)
{
    switch (err) {
    case ACT_ERR_NONE:       return "OK      ";
    case ACT_ERR_FAULT:      return "DRV FAULT";
    case ACT_ERR_TIMEOUT:    return "TIMEOUT ";
    case ACT_ERR_NOT_HOMED:  return "NO SLOT ";
    case ACT_ERR_NO_LIMIT:   return "NO LIMIT";
    case ACT_ERR_5V_OFF:     return "5V OFF  ";
    case ACT_ERR_BUSY:       return "BUSY    ";
    case ACT_ERR_PARAM:      return "BAD PARM";
    case ACT_ERR_LIMIT_STOP: return "LIMIT   ";
    default:                 return "UNKNOWN ";
    }
}

/* ------------------------------------------------------------ */
void Ui_Init(void)
{
    s_last.state    = 0xFF;
    s_last.error    = 0xFF;
    s_last.flags    = 0xFF;
    s_last.current  = 0xFF;
    s_last.target   = 0xFF;
    s_last.valid    = 0xFF;
    s_last.edit     = 0xFF;
    s_last.position = 0xFFFFFFFFUL;
    s_last.spp      = 0xFFFFU;
    s_last.speed    = 0;
    s_last.uart_ok  = 0xFF;
    s_last.wifi_ok  = 0xFF;
    s_last.mqtt_ok  = 0xFF;
    s_blink         = 0;
    s_edit          = 0;
    s_draft_steps   = 0;

#if FEATURE_WS2812
    Led_SetAll(0, 0, 20);
    Led_Show();
#endif

#if FEATURE_LCD
    Lcd_Clear(LCD_BLACK);
    Lcd_Text(4, 4,  "PILL BOX", LCD_WHITE, LCD_BLACK, 2);
    Lcd_Text(4, 24, "ACTUATOR " FW_VERSION, LCD_GRAY, LCD_BLACK, 1);
    Lcd_HLine(0, 38, LCD_W, LCD_DARK);

    Lcd_Text(4, 44,  "STATE", LCD_GRAY, LCD_BLACK, 1);
    Lcd_Text(4, 58,  "ERR  ", LCD_GRAY, LCD_BLACK, 1);
    Lcd_Text(4, 72,  "CUR  ", LCD_GRAY, LCD_BLACK, 1);
    Lcd_Text(4, 86,  "TARGET", LCD_GRAY, LCD_BLACK, 1);
    Lcd_Text(4, 100, "STEP/30", LCD_GRAY, LCD_BLACK, 1);
    Lcd_Text(4, 114, "SPEED", LCD_GRAY, LCD_BLACK, 1);
    Lcd_HLine(0, 132, LCD_W, LCD_DARK);
    Lcd_Text(4, 138, "5V:", LCD_GRAY, LCD_BLACK, 1);
    Lcd_Text(60, 138, "USB:", LCD_GRAY, LCD_BLACK, 1);
    Lcd_Text(4, 152, "ORG:", LCD_GRAY, LCD_BLACK, 1);
    Lcd_Text(60, 152, "POS:", LCD_GRAY, LCD_BLACK, 1);
    Lcd_HLine(0, 168, LCD_W, LCD_DARK);
    Lcd_Text(4, 174, "LONG BK=CALIBRATE", LCD_GRAY, LCD_BLACK, 1);
    Lcd_Text(4, 186, "UP/DN=STEP OK=SAVE", LCD_GRAY, LCD_BLACK, 1);
    Lcd_Text(4, 200, "LONG OK=STOP", LCD_GRAY, LCD_BLACK, 1);
    Lcd_HLine(0, 210, LCD_W, LCD_DARK);
    Lcd_Text(4, 214, "ESP:", LCD_GRAY, LCD_BLACK, 1);
    Lcd_Text(76, 214, "WIFI:", LCD_GRAY, LCD_BLACK, 1);
    Lcd_Text(4, 226, "MQTT:", LCD_GRAY, LCD_BLACK, 1);
#endif
}

/* 处理一次按键动作 */
static void ui_handle_key(uint8_t id, uint8_t ev)
{
    uint8_t rc;

    if (ev == KEY_EVT_LONG && id == KEY_ID_OK) {
        Act_Stop();
        Buzzer_Beep(200);
        return;
    }
    if (ev == KEY_EVT_LONG && id == KEY_ID_BACK) {
        if (!Act_IsBusy()) {
            ActParams p;
            Act_GetParams(&p);
            s_draft_steps = p.steps_per_slot;
            s_edit = (uint8_t)!s_edit;
            s_last.edit = 0xFF;
            Buzzer_Beep(80);
        }
        return;
    }

    if (s_edit && ev == KEY_EVT_SHORT) {
        if (id == KEY_ID_UP && s_draft_steps < 20000U) s_draft_steps++;
        if (id == KEY_ID_DN && s_draft_steps > 1U) s_draft_steps--;
        if (id == KEY_ID_OK) {
            ActParams p;
            Act_GetParams(&p);
            p.steps_per_slot = s_draft_steps;
            rc = Act_SetParams(&p);
            if (rc == ACT_RC_OK) s_edit = 0;
            Buzzer_Beep(rc == ACT_RC_OK ? 80 : 300);
        }
        if (id == KEY_ID_BACK) s_edit = 0;
        return;
    }

    if (s_edit && ev == KEY_EVT_LONG) {
        if (id == KEY_ID_UP && s_draft_steps <= 19900U) s_draft_steps += 100U;
        if (id == KEY_ID_DN && s_draft_steps > 100U) s_draft_steps -= 100U;
        return;
    }

    if (ev == KEY_EVT_SHORT) {
        switch (id) {
        case KEY_ID_BACK:
            Act_ClearError();
            Buzzer_Beep(50);
            return;
        default:
            return;
        }
    }

}

void Ui_Tick(void)
{
    uint8_t id;

    Key_Scan();

    for (id = 0; id < KEY_ID_NUM; id++) {
        uint8_t ev = Key_GetEvent(id);

        if (ev != KEY_EVT_NONE) {
            (void)ui_handle_key(id, ev);
        }
    }
}

/* 绘制一个"字段变化才重画"的数字 */
static void show_num(uint16_t x, uint16_t y, int32_t v, uint16_t fg)
{
    Lcd_FillRect(x, y, (uint16_t)(6 * 8), 8, LCD_BLACK);
    Lcd_TextNum(x, y, v, fg, LCD_BLACK, 1);
}

static void show_str(uint16_t x, uint16_t y, const char *s, uint16_t fg)
{
    Lcd_Text(x, y, s, fg, LCD_BLACK, 1);
}

void Ui_Refresh(void)
{
    ActStatus st;
    uint8_t   err_on;
    uint8_t   uart_ok;
    uint8_t   wifi_ok;
    uint8_t   mqtt_ok;

    Act_GetStatus(&st);
    Cmd_GetLinkStatus(&uart_ok, &wifi_ok, &mqtt_ok);
    err_on = (uint8_t)(st.state == ACT_ST_ERROR);

    /* ---- WS2812 状态灯 ---- */
#if FEATURE_WS2812
    switch (st.state) {
    case ACT_ST_HOMING:
        Led_SetAll(20, 20, 0);
        Led_Show();
        break;
    case ACT_ST_MOVING:
        Led_SetAll(0, 20, 0);
        Led_Show();
        break;
    case ACT_ST_ERROR:
        s_blink ^= 1U;                       /* 每次刷新翻转一次 -> 红闪 */
        Led_SetAll(s_blink ? 30 : 0, 0, 0);
        Led_Show();
        break;
    default:
        Led_SetAll(0, 0, (uint8_t)(Pwr_Is5V() ? 8 : 24));
        Led_Show();
        break;
    }
#endif

    /* ---- 蜂鸣器报警 ---- */
    Buzzer_Alarm(err_on);

    /* ---- 屏幕文字 ---- */
#if FEATURE_LCD
    if (st.state != s_last.state) {
        s_last.state = st.state;
        show_str(60, 44, state_text(st.state),
                 (st.state == ACT_ST_ERROR) ? LCD_RED : LCD_GREEN);
    }
    if (st.error != s_last.error) {
        s_last.error = st.error;
        show_str(60, 58, error_text(st.error),
                 (st.error == ACT_ERR_NONE) ? LCD_GREEN : LCD_RED);
    }
    if (st.current_slot != s_last.current || st.slot_valid != s_last.valid) {
        s_last.current = st.current_slot;
        s_last.valid = st.slot_valid;
        if (st.slot_valid) {
            show_num(60, 72, (int32_t)st.current_slot, LCD_GREEN);
        } else {
            Lcd_FillRect(60, 72, 48, 8, LCD_BLACK);
            show_str(60, 72, "--", LCD_YELLOW);
        }
    }
    if (st.target_slot != s_last.target) {
        s_last.target = st.target_slot;
        if (st.target_slot) {
            show_num(60, 86, (int32_t)st.target_slot, LCD_YELLOW);
        } else {
            Lcd_FillRect(60, 86, 48, 8, LCD_BLACK);
            show_str(60, 86, "--", LCD_GRAY);
        }
    }
    if (st.steps_per_slot != s_last.spp || s_edit != s_last.edit ||
        (s_edit && s_draft_steps != s_last.spp)) {
        s_last.spp = s_edit ? s_draft_steps : st.steps_per_slot;
        s_last.edit = s_edit;
        show_num(72, 100, (int32_t)s_last.spp, s_edit ? LCD_YELLOW : LCD_WHITE);
    }
    if (st.speed_hz != s_last.speed) {
        s_last.speed = st.speed_hz;
        show_num(60, 114, (int32_t)st.speed_hz, LCD_WHITE);
    }
    if (st.flags != s_last.flags) {
        s_last.flags = st.flags;
        show_str(32, 138, (st.flags & 0x01) ? "ON " : "OFF", (st.flags & 0x01) ? LCD_GREEN : LCD_GRAY);
        show_str(96, 138, (st.flags & 0x02) ? "IN " : "-- ", (st.flags & 0x02) ? LCD_BLUE : LCD_GRAY);
        show_str(40, 152, (st.flags & 0x04) ? "1" : "0", (st.flags & 0x04) ? LCD_GREEN : LCD_GRAY);
        show_str(96, 152, (st.flags & 0x08) ? "1" : "0", (st.flags & 0x08) ? LCD_RED : LCD_GRAY);
        show_str(160, 152, (st.flags & 0x10) ? "RUN" : "STP",
                 (st.flags & 0x10) ? LCD_YELLOW : LCD_GRAY);
    }
    if (uart_ok != s_last.uart_ok) {
        s_last.uart_ok = uart_ok;
        show_str(40, 214, uart_ok ? "ON " : "OFF", uart_ok ? LCD_GREEN : LCD_RED);
    }
    if (wifi_ok != s_last.wifi_ok) {
        s_last.wifi_ok = wifi_ok;
        show_str(116, 214, wifi_ok ? "ON " : "OFF", wifi_ok ? LCD_GREEN : LCD_GRAY);
    }
    if (mqtt_ok != s_last.mqtt_ok) {
        s_last.mqtt_ok = mqtt_ok;
        show_str(44, 226, mqtt_ok ? "ON " : "OFF", mqtt_ok ? LCD_GREEN : LCD_GRAY);
    }
#endif
}
