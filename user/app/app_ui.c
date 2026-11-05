#include "app_ui.h"
#include "app_actuator.h"
#include "app_power.h"
#include "app_port.h"
#include "app_types.h"
#include "app_config.h"

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
    uint8_t  pills;
    uint32_t position;
    uint16_t spp;
    uint16_t speed;
} s_last;

static uint8_t s_blink;

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
    case ACT_ERR_NOT_HOMED:  return "NO HOME ";
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
    s_last.pills    = 0xFF;
    s_last.position = 0xFFFFFFFFUL;
    s_last.spp      = 0;
    s_last.speed    = 0;
    s_blink         = 0;

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
    Lcd_Text(4, 72,  "POS  ", LCD_GRAY, LCD_BLACK, 1);
    Lcd_Text(4, 86,  "PILLS", LCD_GRAY, LCD_BLACK, 1);
    Lcd_Text(4, 100, "SPP  ", LCD_GRAY, LCD_BLACK, 1);
    Lcd_Text(4, 114, "SPEED", LCD_GRAY, LCD_BLACK, 1);
    Lcd_HLine(0, 132, LCD_W, LCD_DARK);
    Lcd_Text(4, 138, "5V:", LCD_GRAY, LCD_BLACK, 1);
    Lcd_Text(60, 138, "USB:", LCD_GRAY, LCD_BLACK, 1);
    Lcd_Text(4, 152, "ORG:", LCD_GRAY, LCD_BLACK, 1);
    Lcd_Text(60, 152, "POS:", LCD_GRAY, LCD_BLACK, 1);
    Lcd_HLine(0, 168, LCD_W, LCD_DARK);
    Lcd_Text(4, 174, "OK=DOSE UP=HOME", LCD_GRAY, LCD_BLACK, 1);
    Lcd_Text(4, 186, "DN=CLR BK=5V", LCD_GRAY, LCD_BLACK, 1);
    Lcd_Text(4, 200, "LONG OK=STOP", LCD_GRAY, LCD_BLACK, 1);
#endif
}

/* 处理一次按键动作 */
static void ui_handle_key(uint8_t id, uint8_t ev)
{
    uint8_t rc = 0;

    if (ev == KEY_EVT_SHORT) {
        switch (id) {
        case KEY_ID_OK:
            rc = Act_Dispense(1);
            Buzzer_Beep(rc == ACT_RC_OK ? 80 : 300);
            AppLog_Push(EVT_CMD_DISPENSE, 1, rc);
            return;
        case KEY_ID_UP:
            rc = Act_Home();
            Buzzer_Beep(rc == ACT_RC_OK ? 80 : 300);
            AppLog_Push(EVT_CMD_HOME, 0, rc);
            return;
        case KEY_ID_DN:
            Act_ClearError();
            Buzzer_Beep(50);
            return;
        case KEY_ID_BACK:
            if (Pwr_Is5V()) {
                (void)Pwr_Shutdown5V();
            } else {
                (void)Pwr_Ensure5V();
            }
            Buzzer_Beep(50);
            return;
        default:
            return;
        }
    }

    if (ev == KEY_EVT_LONG) {
        if (id == KEY_ID_OK) {
            Act_Stop();
            Buzzer_Beep(200);
            AppLog_Push(EVT_CMD_STOP, 0, 0);
        } else if (id == KEY_ID_BACK) {
            (void)Pwr_Shutdown5V();
            Buzzer_Beep(200);
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

    Act_GetStatus(&st);
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
    if (st.position != s_last.position) {
        s_last.position = st.position;
        show_num(60, 72, (int32_t)st.position, LCD_WHITE);
    }
    if (st.pills_done != s_last.pills) {
        s_last.pills = st.pills_done;
        show_num(60, 86, (int32_t)st.pills_done, LCD_YELLOW);
    }
    if (st.steps_per_pill != s_last.spp) {
        s_last.spp = st.steps_per_pill;
        show_num(60, 100, (int32_t)st.steps_per_pill, LCD_WHITE);
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
#endif
}
