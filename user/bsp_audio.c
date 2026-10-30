#include "bsp_audio.h"
#include "pinmap.h"
#include "stm32f10x.h"

/* 释放 JTAG 保留 SWD：PA15/PB3/PB4 才能作 GPIO/I2S 使用 */
static void Audio_ReleaseJTAG(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);   /* 关闭JTAG，保留SWD */
}

static void Audio_I2S3_TxInit(void)
{
    GPIO_InitTypeDef g;
    I2S_InitTypeDef  i2s;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI3, ENABLE);

    /* PA15=WS, PB3=CK, PB5=SD 复用为 SPI3 */
    g.GPIO_Mode  = GPIO_Mode_AF_PP;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    g.GPIO_Pin   = SPK_WS_PIN;
    GPIO_Init(GPIOA, &g);
    g.GPIO_Pin   = SPK_CK_PIN | SPK_SD_PIN;
    GPIO_Init(GPIOB, &g);

    I2S_StructInit(&i2s);
    i2s.I2S_Mode          = I2S_Mode_MasterTx;
    i2s.I2S_Standard      = I2S_Standard_Phillips;
    i2s.I2S_DataFormat    = I2S_DataFormat_16b;
    i2s.I2S_MCLKOutput    = I2S_MCLKOutput_Disable;
    i2s.I2S_AudioFreq     = I2S_AudioFreq_48k;
    i2s.I2S_CPOL          = I2S_CPOL_Low;
    I2S_Init(SPI3, &i2s);
    I2S_Cmd(SPI3, ENABLE);
}

/* 麦克风 I2S2（预留，当前未实现数据接收）
 * ICS-43434 是 I2S 从机：需要 MCU 作主机提供 BCLK/WS，
 * 因此 CK/WS 必须是复用推挽输出，SD 为输入。 */
static void Audio_I2S2_RxInit(void)
{
    GPIO_InitTypeDef g;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);

    g.GPIO_Mode  = GPIO_Mode_AF_PP;             /* 主机模式: CK/WS 输出 */
    g.GPIO_Speed = GPIO_Speed_50MHz;
    g.GPIO_Pin   = MIC_CK_PIN | MIC_WS_PIN;
    GPIO_Init(GPIOB, &g);
    g.GPIO_Mode  = GPIO_Mode_IN_FLOATING;       /* SD 数据输入 */
    g.GPIO_Pin   = MIC_SD_PIN;
    GPIO_Init(GPIOB, &g);
    /* TODO: 按 I2S_Mode_MasterRx + I2S2_Init 实现录音，
     *       当前仅保留引脚配置，不占用 I2S2 外设。 */
}

void Audio_Init(void)
{
    Audio_ReleaseJTAG();
    Audio_I2S3_TxInit();
    Audio_I2S2_RxInit();
}

void Audio_PlaySamples(const int16_t *samples, uint16_t count)
{
    /* I2S 每帧包含左、右两个声道槽；MAX98357A 取左/右任一即可，
     * 这里把单声道样本同时写入两个槽，保证 48kHz 采样率正确 */
    while (count--) {
        while (SPI_I2S_GetFlagStatus(SPI3, SPI_I2S_FLAG_TXE) == RESET);
        SPI_I2S_SendData(SPI3, (uint16_t)*samples);
        while (SPI_I2S_GetFlagStatus(SPI3, SPI_I2S_FLAG_TXE) == RESET);
        SPI_I2S_SendData(SPI3, (uint16_t)*samples);
        samples++;
    }
}

void Audio_Stop(void)
{
    I2S_Cmd(SPI3, DISABLE);
}
