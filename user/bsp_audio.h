#ifndef __BSP_AUDIO_H
#define __BSP_AUDIO_H
#include <stdint.h>

/* 音频框架：
 *  播放: MAX98357A 功放，I2S3 (WS=PA15 CK=PB3 SD=PB5)，16bit/48k 单声道
 *  录音: ICS-43434 麦克风，I2S2 (WS=PB12 CK=PB13 SD=PB15)，I2S 从机
 * 需先关闭 JTAG 释放 PA15/PB3/PB4（AFIO_MAPR 配置），见 Audio_Init。
 * 实际 PCM 数据由 ESP32-C3 语音/云端提供，此处仅搭好 I2S 收发骨架。
 */

void Audio_Init(void);
/* 播放一帧 PCM(16bit)：把数据填入 I2S3 发送（简化：阻塞发送） */
void Audio_PlaySamples(const int16_t *samples, uint16_t count);
/* TODO: 麦克风录音缓冲回调 */
void Audio_Stop(void);

#endif
