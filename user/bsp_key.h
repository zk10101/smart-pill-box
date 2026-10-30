#ifndef __BSP_KEY_H
#define __BSP_KEY_H
#include <stdint.h>

typedef enum {
    KEY_ID_UP = 0,
    KEY_ID_DN,
    KEY_ID_OK,
    KEY_ID_BACK,
    KEY_ID_NUM
} KeyId;

typedef enum {
    KEY_EVT_NONE = 0,
    KEY_EVT_SHORT,      /* 短按 */
    KEY_EVT_LONG,       /* 长按(>1s) */
} KeyEvent;

/* 初始化 4 个按键 GPIO（上拉输入），需每 10ms 由主循环调用一次 */
void Key_Init(void);
void Key_Scan(void);
/* 读取某个按键的事件（读后自动清除） */
KeyEvent Key_GetEvent(KeyId id);
/* 直接读取按键当前电平（0=按下，1=松开） */
uint8_t Key_IsPressed(KeyId id);

#endif
