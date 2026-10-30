#include "bsp_key.h"
#include "pinmap.h"
#include "stm32f10x.h"

/* 扫描周期 = 10ms（由主循环节拍保证） */
#define KEY_LONG_CNT    100     /* 100 * 10ms = 1s 判长按 */

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} KeyPinCfg;

static const KeyPinCfg key_cfg[KEY_ID_NUM] = {
    { KEY_UP_PORT,   KEY_UP_PIN   },
    { KEY_DN_PORT,   KEY_DN_PIN   },
    { KEY_OK_PORT,   KEY_OK_PIN   },
    { KEY_BACK_PORT, KEY_BACK_PIN },
};

static uint8_t  key_state[KEY_ID_NUM];   /* 当前消抖后状态 0=按下 1=松开 */
static uint16_t key_cnt[KEY_ID_NUM];     /* 持续按下计数 */
static uint8_t  key_long_flag[KEY_ID_NUM];
static KeyEvent key_event[KEY_ID_NUM];

void Key_Init(void)
{
    GPIO_InitTypeDef g;
    uint8_t i;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    g.GPIO_Mode  = GPIO_Mode_IPU;        /* 外部上拉，按下为低 */
    g.GPIO_Speed = GPIO_Speed_50MHz;
    g.GPIO_Pin   = KEY_UP_PIN | KEY_DN_PIN | KEY_OK_PIN | KEY_BACK_PIN;
    GPIO_Init(GPIOC, &g);

    for (i = 0; i < KEY_ID_NUM; i++) {
        key_state[i] = 1;
        key_cnt[i]  = 0;
        key_long_flag[i] = 0;
        key_event[i] = KEY_EVT_NONE;
    }
}

void Key_Scan(void)
{
    uint8_t i;
    for (i = 0; i < KEY_ID_NUM; i++) {
        uint8_t cur = (uint8_t)GPIO_ReadInputDataBit(key_cfg[i].port, key_cfg[i].pin);
        if (cur == 0 && key_state[i] == 1) {        /* 按下边沿 */
            key_state[i] = 0;
            key_cnt[i]  = 0;
            key_long_flag[i] = 0;
        } else if (cur == 0 && key_state[i] == 0) { /* 持续按下 */
            if (++key_cnt[i] >= KEY_LONG_CNT && !key_long_flag[i]) {
                key_long_flag[i] = 1;
                key_event[i] = KEY_EVT_LONG;
            }
        } else if (cur == 1 && key_state[i] == 0) { /* 松开 */
            key_state[i] = 1;
            if (!key_long_flag[i]) {
                key_event[i] = KEY_EVT_SHORT;
            }
            key_cnt[i] = 0;
        }
    }
}

KeyEvent Key_GetEvent(KeyId id)
{
    KeyEvent e = key_event[id];
    key_event[id] = KEY_EVT_NONE;
    return e;
}

uint8_t Key_IsPressed(KeyId id)
{
    return (uint8_t)(GPIO_ReadInputDataBit(key_cfg[id].port, key_cfg[id].pin) == 0);
}
