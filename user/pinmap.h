#ifndef __PINMAP_H
#define __PINMAP_H
/**
 * ============================================================
 * 智能药盒（执行机构版）引脚分配表
 * 依据最终版原理图：Schematic1_1 / Board1_1 / 单页 A4 / V1.0
 * 主控: STM32F103RCT6 (LQFP64, 256K Flash / 48K RAM, 72MHz)
 *
 * 【层次约定】本文件只允许被 bsp/ 目录（数据访问层）包含。
 *           业务层(app_actuator/app_power)与表现层(app_command/app_ui)
 *           不得直接引用任何引脚宏或寄存器。
 *
 * 注: PA15/PB3 是 JTAG 引脚，本工程只用 SWD，
 *     Sys_Init() 中调用 GPIO_Remap_SWJ_JTAGDisable 释放。
 * ============================================================
 */

/* ---------------- 按键 ×4（外部 10k 上拉 + 100nF，按下为低） -------- */
#define KEY_UP_PORT         GPIOC
#define KEY_UP_PIN          GPIO_Pin_0      /* PC0  KEY_UP  */
#define KEY_DN_PORT         GPIOC
#define KEY_DN_PIN          GPIO_Pin_1      /* PC1  KEY_DN  */
#define KEY_OK_PORT         GPIOC
#define KEY_OK_PIN          GPIO_Pin_2      /* PC2  KEY_OK  */
#define KEY_BACK_PORT       GPIOC
#define KEY_BACK_PIN        GPIO_Pin_3      /* PC3  KEY_BACK */

/* ---------------- 步进电机（经 74HCT245D 缓冲后送驱动器插槽 U4） -----
 * 245 的 A 侧分别接网络 A1/A2/A3，B 侧分别接 STEP/DIR/EN：
 *   A1 = PB8  (pin61) -> B1 = STEP  (TIM4_CH3 输出脉冲)
 *   A2 = PB9  (pin62) -> B2 = DIR
 *   A3 = PC12 (pin53) -> B3 = EN
 * 驱动插座 U4 = ZX-XH2.54-6PZZ: 1 STEP / 2 DIR / 3 EN / 4 FAULT_N / 5 VS / 6 GND
 * 本期信号引至外置 12V X28S PLC 输入版，VS 不接电机 V+；
 * COM 与 EN 极性待核对电机接线说明，详见 docs/12格转盘接线与联调.md。
 * FAULT_N 经 R4 1k 进 PC13，R3 10k 上拉到 3V3，低有效。
 * !! PC13 不是 5V 容忍脚，长期 5V 直连需硬件串阻/钳位复核 !! */
#define MOTOR_STEP_PORT     GPIOB
#define MOTOR_STEP_PIN      GPIO_Pin_8      /* PB8  A1 -> STEP (TIM4_CH3) */
#define MOTOR_DIR_PORT      GPIOB
#define MOTOR_DIR_PIN       GPIO_Pin_9      /* PB9  A2 -> DIR  */
#define MOTOR_EN_PORT       GPIOC
#define MOTOR_EN_PIN        GPIO_Pin_12     /* PC12 A3 -> EN   */
#define MOTOR_FAULT_PORT    GPIOC
#define MOTOR_FAULT_PIN     GPIO_Pin_13     /* PC13 FAULT_N（低=故障） */

/* ---------------- 限位开关 ×2（10k 上拉 + 100nF，触发为低） ---------
 * CN2 = LIM_ORG（原点），CN3 = LIM_POS（药仓/行程终点），均经 100R 串阻 */
#define LIM_ORG_PORT        GPIOC
#define LIM_ORG_PIN         GPIO_Pin_6      /* PC6  LIM_ORG */
#define LIM_POS_PORT        GPIOC
#define LIM_POS_PIN         GPIO_Pin_7      /* PC7  LIM_POS */

/* ---------------- SPI 彩屏（ST7789，SPI1 + 软件片选） ---------------
 * 注意(硬件待复核): 屏 SDA 与 SPI1_MOSI 之间串联 R46 22R；
 * 若实测屏无显示，请用万用表确认 R46 两端是否为 LCD_SDA <-> SPI1_MOSI。 */
#define LCD_CS_PORT         GPIOA
#define LCD_CS_PIN          GPIO_Pin_4      /* PA4  LCD_CS（10k 上拉） */
#define LCD_DC_PORT         GPIOB
#define LCD_DC_PIN          GPIO_Pin_0      /* PB0  LCD_DC */
#define LCD_RES_PORT        GPIOB
#define LCD_RES_PIN         GPIO_Pin_1      /* PB1  LCD_RES（低有效） */
#define LCD_BL_PORT         GPIOB
#define LCD_BL_PIN          GPIO_Pin_10     /* PB10 LCD_BL（经 AO3401A，低=亮） */

/* ---------------- SPI1 总线（屏 + W25Q64 预留） ---------------------
 * R45 22R 串在 SPI1_SCK -> LCD_SCL */
#define SPI1_SCK_PORT       GPIOA
#define SPI1_SCK_PIN        GPIO_Pin_5      /* PA5  SPI1_SCK  */
#define SPI1_MISO_PORT      GPIOA
#define SPI1_MISO_PIN       GPIO_Pin_6      /* PA6  SPI1_MISO */
#define SPI1_MOSI_PORT      GPIOA
#define SPI1_MOSI_PIN       GPIO_Pin_7      /* PA7  SPI1_MOSI */

/* ---------------- 串口 1：ESP32-C3（上行 / 云端链路，本期只保留收发） ----
 * PA9 TX -> 1k(R33) -> ESP32 RXD0(pin30)
 * PA10 RX <- 1k(R32) <- ESP32 TXD0(pin31)
 * PA8  NET_EN：ESP32 EN，10k 上拉，高电平使能 */
#define UART1_TX_PORT       GPIOA
#define UART1_TX_PIN        GPIO_Pin_9      /* PA9  USART1_TX */
#define UART1_RX_PORT       GPIOA
#define UART1_RX_PIN        GPIO_Pin_10     /* PA10 USART1_RX */
#define NET_EN_PORT         GPIOA
#define NET_EN_PIN          GPIO_Pin_8      /* PA8  NET_EN */

/* ---------------- 串口 2：摄像头（Sipeed MaixBit / K210，H1 接口） ----
 * H1 = PZ254V-11-04P：1=5V_SW（SW3 手动开关供电）2=USART2_TX 3=USART2_RX 4=GND
 * PA2/PA3 经 1k(R2/R47) 串阻连到 H1；K210 IO 为 3.3V，可直接相连。 */
#define UART2_TX_PORT       GPIOA
#define UART2_TX_PIN        GPIO_Pin_2      /* PA2  USART2_TX -> H1.2 */
#define UART2_RX_PORT       GPIOA
#define UART2_RX_PIN        GPIO_Pin_3      /* PA3  USART2_RX <- H1.3 */

/* ---------------- 电源管理（IP5306） --------------------------------
 * PWR_KEY(PC11) -> R1 10k -> Q3(MMBT3904) 基极；
 * Q3 发射极接 IP5306 KEY(pin5)，集电极接 GND，
 * 因此 PC11 输出高电平 = 按下 KEY（拉低），脉宽 100~150ms 实现开/关机。
 * STBY_STAT(PC9) = 5V 轨经 100k/100k 分压 -> 高=5V 已建立（IP5306 升压输出正常）
 * CHG_STAT (PC8) = VBUS_IN 经 100k/100k 分压 -> 高=USB 已插入
 * 注意：两路都只是"电平存在"检测，不是 IP5306 的真实状态寄存器。 */
#define PWR_KEY_PORT        GPIOC
#define PWR_KEY_PIN         GPIO_Pin_11     /* PC11 PWR_KEY（输出，高=按下） */
#define STBY_STAT_PORT      GPIOC
#define STBY_STAT_PIN       GPIO_Pin_9      /* PC9  5V 存在检测 */
#define CHG_STAT_PORT       GPIOC
#define CHG_STAT_PIN        GPIO_Pin_8      /* PC8  USB 插入检测 */

/* ---------------- 蜂鸣器（有源 2.7kHz，NPN 驱动，高电平响） ----------
 * 最终版把蜂鸣器电源从 5V 改成了 3V3 */
#define BUZZER_PORT         GPIOB
#define BUZZER_PIN          GPIO_Pin_11     /* PB11 BUZ */

/* ---------------- WS2812 彩灯 ×3（PA11 DIN，3V3 供电） --------------
 * 硬件提示: WS2812B 系列 VDD 规格 3.7~5.3V，3V3 供电会偏暗/偏色 */
#define WS2812_PORT         GPIOA
#define WS2812_PIN          GPIO_Pin_11     /* PA11 WS2812_DIN */

/* ---------------- 电池电压采样（原理图未走线） ----------------------
 * BAT -> R13 100k -> BAT_ADC -> R14 100k -> GND，BAT_ADC 未接到任何 MCU 脚；
 * PA0(ADC12_IN0) 空闲。硬件补线后把 FEATURE_BAT_ADC 打开即可。 */
#define BAT_ADC_PORT        GPIOA
#define BAT_ADC_PIN         GPIO_Pin_0      /* PA0（建议接 BAT_ADC） */

/* ---------------- 时钟 --------------------------------------------
 * HSE: 8MHz(X1) -> PLL -> 72MHz
 * LSE: 32.768kHz(X2)，本版固件未使用 RTC */

#endif /* __PINMAP_H */
