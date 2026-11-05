#include "app_port.h"
#include "app_config.h"
#include "pinmap.h"
#include "stm32f10x.h"

/**
 * ============================================================
 * 数据访问层：4 个按键
 *   PC0..PC3，外部 10k 上拉 + 100nF，按下为低
 *   10ms 扫描一次；短按在"松开"沿产生，长按在按住 1s 时产生
 * ============================================================
 */

#define KEY_LONG_CNT    100U    /* 100 * 10ms = 1s */

typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
} KeyPinCfg;

static const KeyPinCfg s_cfg[KEY_ID_NUM] = {
    { KEY_UP_PORT,   KEY_UP_PIN   },
    { KEY_DN_PORT,   KEY_DN_PIN   },
    { KEY_OK_PORT,   KEY_OK_PIN   },
    { KEY_BACK_PORT, KEY_BACK_PIN }
};

static uint8_t  s_state[KEY_ID_NUM];    /* 1=松开 0=按下 */
static uint16_t s_cnt[KEY_ID_NUM];
static uint8_t  s_long[KEY_ID_NUM];
static uint8_t  s_event[KEY_ID_NUM];

void Key_Init(void)
{
    GPIO_InitTypeDef g;
    uint8_t i;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    g.GPIO_Mode  = GPIO_Mode_IPU;
    g.GPIO_Speed = GPIO_Speed_2MHz;
    g.GPIO_Pin   = KEY_UP_PIN | KEY_DN_PIN | KEY_OK_PIN | KEY_BACK_PIN;
    GPIO_Init(GPIOC, &g);

    for (i = 0; i < KEY_ID_NUM; i++) {
        s_state[i] = 1;
        s_cnt[i]   = 0;
        s_long[i]  = 0;
        s_event[i] = KEY_EVT_NONE;
    }
}

void Key_Scan(void)
{
    uint8_t i;

    for (i = 0; i < KEY_ID_NUM; i++) {
        uint8_t cur = (uint8_t)GPIO_ReadInputDataBit(s_cfg[i].port, s_cfg[i].pin);

        if (cur == 0 && s_state[i] == 1) {          /* 按下沿 */
            s_state[i] = 0;
            s_cnt[i]   = 0;
            s_long[i]  = 0;
        } else if (cur == 0 && s_state[i] == 0) {   /* 持续按住 */
            if (s_cnt[i] < 0xFFFFU) s_cnt[i]++;
            if (s_cnt[i] >= KEY_LONG_CNT && !s_long[i]) {
                s_long[i]  = 1;
                s_event[i] = KEY_EVT_LONG;
            }
        } else if (cur == 1 && s_state[i] == 0) {   /* 松开沿 */
            s_state[i] = 1;
            if (!s_long[i]) s_event[i] = KEY_EVT_SHORT;
            s_cnt[i] = 0;
        }
    }
}

uint8_t Key_GetEvent(uint8_t id)
{
    uint8_t e;

    if (id >= KEY_ID_NUM) return KEY_EVT_NONE;
    e = s_event[id];
    s_event[id] = KEY_EVT_NONE;
    return e;
}
