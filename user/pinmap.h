#ifndef __PINMAP_H
#define __PINMAP_H
/**
 * ============================================================
 * 智能药盒 引脚分配表（依据嘉立创原理图 V1.0 逐脚核对）
 * 主控: STM32F103RCT6 (LQFP64, 256K Flash / 48K RAM)
 * 注意: PA15/PB3/PB4 是 JTAG 引脚，本工程已改用 SWD，
 *       关闭 JTAG 的动作在 bsp_audio.c 的 Audio_ReleaseJTAG()
 *       （GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE)）
 * ============================================================
 */

/* ---------------- 按键 ×4（外部10k上拉，按下为低） ---------------- */
#define KEY_UP_PORT         GPIOC
#define KEY_UP_PIN          GPIO_Pin_0      /* PC0  KEY_UP  */
#define KEY_DN_PORT         GPIOC
#define KEY_DN_PIN          GPIO_Pin_1      /* PC1  KEY_DN  */
#define KEY_OK_PORT         GPIOC
#define KEY_OK_PIN          GPIO_Pin_2      /* PC2  KEY_OK  */
#define KEY_BACK_PORT       GPIOC
#define KEY_BACK_PIN        GPIO_Pin_3      /* PC3  KEY_BACK */

/* ---------------- 步进电机（经 74HCT245 电平转换后输出） ----------------
 * 原理图实测: 245 的 A0/A1/A2(引脚3/4/5) 分别接网络 A1/A2/A3，
 *             B1/B2/B3(引脚17/16/15) 分别接 STEP/DIR/EN。
 * 网络对应 MCU: A1=PB8(pin61)、A2=PB9(pin62)、A3=PC12(pin53)
 * STEP 用定时器脉冲: PB8 = TIM4_CH3
 * 注意: 原代码把 EN 写成 PD2，但原理图 PD2(pin54) 为空脚，
 *       实际 EN 走 A3 = PC12，必须用 GPIOC Pin_12。 */
#define MOTOR_STEP_PORT     GPIOB
#define MOTOR_STEP_PIN      GPIO_Pin_8      /* PB8  A1 -> STEP (TIM4_CH3) */
#define MOTOR_DIR_PORT      GPIOB
#define MOTOR_DIR_PIN       GPIO_Pin_9      /* PB9  A2 -> DIR  */
#define MOTOR_EN_PORT       GPIOC
#define MOTOR_EN_PIN        GPIO_Pin_12     /* PC12 A3 -> EN   */
#define MOTOR_FAULT_PORT    GPIOC
#define MOTOR_FAULT_PIN     GPIO_Pin_13     /* PC13 FAULT_N 输入(低有效) */

/* ---------------- 限位传感器 ×2（外部10k上拉，触发为低） ---------------- */
#define LIM_ORG_PORT        GPIOC
#define LIM_ORG_PIN         GPIO_Pin_6      /* PC6  LIM_ORG 原点 */
#define LIM_POS_PORT        GPIOC
#define LIM_POS_PIN         GPIO_Pin_7      /* PC7  LIM_POS 药仓 */

/* ---------------- SPI LCD（ST7789 类 4 线 SPI，共用 SPI1 总线） -------- */
#define LCD_CS_PORT         GPIOA
#define LCD_CS_PIN          GPIO_Pin_4      /* PA4  LCD_CS (软件CS, 10k上拉) */
#define LCD_DC_PORT         GPIOB
#define LCD_DC_PIN          GPIO_Pin_0      /* PB0  LCD_DC (命令/数据) */
#define LCD_RES_PORT        GPIOB
#define LCD_RES_PIN         GPIO_Pin_1      /* PB1  LCD_RES(复位, 低有效) */
#define LCD_BL_PORT         GPIOB
#define LCD_BL_PIN          GPIO_Pin_10     /* PB10 LCD_BL (经AO3401, 低电平=亮!) */

/* ---------------- SPI1 总线（LCD + W25Q64 共用） ----------------
 * !! 硬件问题(需改板): 原理图 R46 把 LCD_SDA 接到了 SPI1_MISO(PA6)，
 *    而 ST7789 的数据脚必须接 MOSI(PA7)。现板子上屏无法收到数据，
 *    固件仍按 MOSI 驱动；硬件需把屏的 SDA 改到 PA7（飞线/改 R46）。 */
#define SPI1_SCK_PORT       GPIOA
#define SPI1_SCK_PIN        GPIO_Pin_5      /* PA5  SPI1_SCK  */
#define SPI1_MISO_PORT      GPIOA
#define SPI1_MISO_PIN       GPIO_Pin_6      /* PA6  SPI1_MISO (W25Q DO) */
#define SPI1_MOSI_PORT      GPIOA
#define SPI1_MOSI_PIN       GPIO_Pin_7       /* PA7  SPI1_MOSI (W25Q DI + 应为 LCD SDA) */

/* ---------------- W25Q64 SPI Flash ----------------
 * !! 原理图问题: FLASH_CS 网络没有连到任何 MCU 引脚，画板前需补 !!
 * 临时用 PA15? 不行(PA15=I2S3_WS)。先用 PB4 占位，待硬件确认后修改 */
#define FLASH_CS_PORT       GPIOB
#define FLASH_CS_PIN        GPIO_Pin_4      /* TODO: 待硬件补 FLASH_CS 后修改 */

/* ---------------- ESP32-C3 无线模块（USART1） ---------------- */
#define NET_EN_PORT         GPIOA
#define NET_EN_PIN          GPIO_Pin_8      /* PA8  NET_EN  模块使能(高有效) */
#define NET_TX_PORT         GPIOA
#define NET_TX_PIN          GPIO_Pin_9      /* PA9  USART1_TX -> ESP32 RXD0 */
#define NET_RX_PORT         GPIOA
#define NET_RX_PIN          GPIO_Pin_10     /* PA10 USART1_RX <- ESP32 TXD0 */

/* ---------------- 音频: MAX98357A 功放 (I2S3 发送) ----------------
 * I2S3: WS=PA15, CK=PB3, SD=PB5 (需关闭 JTAG 释放 PA15/PB3) */
#define SPK_WS_PIN          GPIO_Pin_15     /* PA15 I2S3_WS -> LRCLK */
#define SPK_CK_PIN          GPIO_Pin_3      /* PB3  I2S3_CK -> BCLK  */
#define SPK_SD_PIN          GPIO_Pin_5      /* PB5  I2S3_SD -> DIN   */

/* ---------------- 音频: ICS-43434 麦克风 (I2S2 接收) ---------------- */
#define MIC_WS_PIN          GPIO_Pin_12     /* PB12 I2S2_WS */
#define MIC_CK_PIN          GPIO_Pin_13     /* PB13 I2S2_CK */
#define MIC_SD_PIN          GPIO_Pin_15     /* PB15 I2S2_SD */

/* ---------------- 蜂鸣器（有源 2.7kHz，NPN 驱动，高电平响） ---------- */
#define BUZZER_PORT         GPIOB
#define BUZZER_PIN          GPIO_Pin_11     /* PB11 BUZ */

/* ---------------- WS2812 彩灯 ×3（单线串行） ----------------
 * !! 硬件问题: 原理图把 WS2812B-2020 的 VDD 接在 3V3 上，
 *    该型号 VDD 规格为 3.7~5.3V，3.3V 供电会偏暗/颜色异常。
 *    建议改接 5V，并对 DIN 做电平转换(或用 3.3V 兼容灯珠)。 */
#define WS2812_PORT         GPIOA
#define WS2812_PIN          GPIO_Pin_11     /* PA11 WS2812_DIN */

/* ---------------- 电源管理状态（IP5306） ---------------- */
#define STBY_STAT_PORT      GPIOC
#define STBY_STAT_PIN       GPIO_Pin_9      /* PC9  STBY_STAT */
#define CHG_STAT_PORT       GPIOC
#define CHG_STAT_PIN        GPIO_Pin_8      /* PC8  CHG_STAT */
/* !! 原理图问题: BAT_ADC(电池分压)未接任何 MCU 引脚, PA0 空置
 *    建议硬件补: PA0(ADC12_IN0) <- BAT_ADC，补好后启用下行宏 !! */
/* #define BAT_ADC_ON */
#define BAT_ADC_PORT        GPIOA
#define BAT_ADC_PIN         GPIO_Pin_0      /* PA0  建议接 BAT_ADC */

/* ---------------- 时钟 ----------------
 * HSE: 8MHz (X1)  -> 系统时钟 72MHz
 * LSE: 32.768kHz (X2) -> 内部 RTC 走时 */

#endif /* __PINMAP_H */
