# 智能药盒（STM32F103RCT6 + ESP32-C3）

基于 STM32F103RCT6 的定时提醒、步进出药、按键复检、漏服告警的智能药盒主控固件。
仓库对应嘉立创 EDA 工程 `ProPrj_智能药盒_2026-10-24.epro2` 的原理图（7 张分图）。

> ⚠️ 原理图存在若干**必须飞线/改板**的问题（5V 无源、屏数据线接错、ESP32 地脚悬空等），
> 详见 [`docs/硬件评审.md`](docs/硬件评审.md)。本文档的引脚表已按原理图实际连线修正。

## 功能概览

| 功能 | 状态 | 说明 |
|------|------|------|
| 定时提醒 | ✅ | 每天最多 8 条计划，到点蜂鸣 + 红灯 |
| 步进出药 | ✅ | 74HCT245 缓冲后驱动外部步进驱动器，限位/超时/故障保护 |
| 按键确认/静音 | ✅ | 4 键：确认 / 静音 / 静音(长按) / 备用 |
| 复检与漏服告警 | ✅ | 取药超时催促，3 次未取判漏服并持续告警 |
| 看门狗 | ✅ | 软件启用 IWDG，约 4s 未喂狗即复位；运行期不开关全局中断 |
| SPI 彩屏（ST7789 240×240） | ⚠️ | ST7789 驱动已写，但原理图 `LCD_SDA` 接在 `SPI1_MISO` 上，需飞线到 PA7 |
| WS2812 指示灯 | ✅ | 3 颗 WS2812B-2020（供电建议改 5V，见硬件评审） |
| ESP32-C3 无线 | ⚠️ | 串口协议已实现；ESP32 端固件**不在本仓库** |
| W25Q64 外部 Flash | ❌ | `FLASH_CS` 未接 MCU 引脚，功能禁用（`FEATURE_EXT_FLASH=0`） |
| 电池电压检测 | ❌ | `BAT_ADC` 未接 MCU，功能禁用（`FEATURE_BAT_ADC=0`） |
| 语音播报 | ⚠️ | MAX98357A 初始化 + 播放接口已写，尚无音频数据/字库 |
| 麦克风 | ❌ | ICS-43434 引脚已定义，I2S2 接收尚未实现 |
| RTC 实时时钟 | ❌ | 软件计时；`VBAT` 空脚，补线后可接 LSE + RTC |
| 出药记录/云端 | ❌ | `CMD_GET_LOG`、`CMD_SERVER_PUSH` 只有骨架 |

## 目录结构

```
smart-pill-box/
├── smart-pill-box.uvprojx      Keil MDK 工程
├── README.md
├── docs/
│   ├── 硬件评审.md              原理图问题清单（P0/P1/P2）
│   ├── 状态机设计.md            五态状态机与实现对照
│   └── 串口通信协议.md          主控 ↔ ESP32-C3 协议
├── start/                     CMSIS 内核 + 启动文件 + system_stm32f10x.c
├── library/                   STM32F10x 标准外设库 3.5.0
├── system/
│   └── Delay.c/h              DWT 周期计数器延时（不占用 SysTick）
└── user/
    ├── main.c                 初始化、1ms 节拍、主循环、IWDG
    ├── pinmap.h               全部引脚定义（按原理图核对）
    ├── app_config.h           功能开关与参数
    ├── state_machine.c/h      五态状态机 + 计划持久化
    ├── protocol.c/h           串口帧协议
    ├── bsp_motor.c/h          步进脉冲(TIM4) + 限位/故障
    ├── bsp_key.c/h            4 键扫描
    ├── bsp_buzzer.c/h         蜂鸣器
    ├── bsp_limit.c/h          限位输入
    ├── bsp_lcd.c/h            ST7789 SPI 屏
    ├── bsp_ws2812.c/h         WS2812 位操作驱动
    ├── bsp_esp32.c/h          USART1 通信
    ├── bsp_audio.c/h          MAX98357A(I2S3) / ICS-43434(I2S2)
    ├── bsp_w25q.c/h           W25Q64（当前禁用）
    ├── bsp_storage.c/h        内部 Flash 保存计划
    ├── stm32f10x_it.c/h       中断向量
    └── stm32f10x_conf.h       标准库配置
```

## 引脚表（按原理图实际连线）

| 功能 | 引脚 | 说明 |
|------|------|------|
| STEP | PB8 | TIM4_CH3 PWM，经 74HCT245 A1→B1 |
| DIR | PB9 | 经 245 A2→B2 |
| EN | **PC12** | 经 245 A3→B3；PD2(pin54) 原理图悬空，**不是** EN |
| FAULT_N | PC13 | 低有效，R3 10k 上拉、R4 1k 串联 |
| 限位 ORG | PC6 | 低有效（CN2） |
| 限位 POS | PC7 | 低有效（CN3） |
| 按键 UP/DN/OK/BACK | PC0 / PC1 / PC2 / PC3 | 10k 上拉 + 100nF，按下接地 |
| LCD CS/DC/RES/BL | PA4 / PB0 / PB1 / PB10 | BL 低电平点亮（AO3401A PMOS） |
| SPI1 SCK/MISO/MOSI | PA5 / PA6 / PA7 | 屏与 W25Q64 共用 |
| ESP32 TX/RX/EN | PA9 / PA10 / PA8 | USART1 115200，交叉接 RXD0/TXD0 |
| 扬声器 WS/CK/SD | PA15 / PB3 / PB5 | I2S3（MAX98357A） |
| 麦克风 WS/CK/SD | PB12 / PB13 / PB15 | I2S2（尚未实现） |
| 蜂鸣器 | PB11 | 高电平经 MMBT3904 驱动 |
| WS2812 | PA11 | 单总线，3 颗级联 |
| 充电状态 | PC8 | `VBUS_IN` 分压，仅表示 USB 在位 |
| 待机状态 | PC9 | `5V` 分压，仅表示 5V 有电 |
| 电池电压 | PA0 | 原理图 `BAT_ADC` 未接，预留 |
| 外部 Flash CS | PB4 | 原理图 `FLASH_CS` 未接，预留（PB4 默认 JTAG NJTRST） |
| SWD | PA13 / PA14 | H2 排针 3V3/SWDIO/SWCLK/GND |

## 参数（`user/app_config.h`）

| 宏 | 默认 | 含义 |
|----|------|------|
| `MOTOR_STEP_FREQ_HZ` | 800 | STEP 频率 |
| `MOTOR_STEPS_PER_PILL` | 800 | 每粒药步数（需标定） |
| `MOTOR_TIMEOUT_MS` | 5000 | 出药超时 |
| `MOTOR_EN_ACTIVE_LOW` | 1 | 驱动器 EN 低有效（实测为准） |
| `RECHECK_MAX_TIMES` / `RECHECK_INTERVAL_S` | 3 / 60 | 取药催促次数/间隔 |
| `REMIND_BEEP_TIMES` / `REMIND_RETRY_S` | 3 / 120 | 提醒轮数/重响间隔 |
| `SCHEDULE_MAX` | 8 | 计划条数 |
| `FEATURE_EXT_FLASH` / `FEATURE_BAT_ADC` | 0 / 0 | 硬件问题，默认关闭 |

## 构建

- Keil MDK 5（工程 `smart-pill-box.uvprojx`）：Device STM32F103RC、IRAM `0x20000000/0xC000`、IROM `0x08000000/0x40000`，
  宏 `STM32F10X_HD, USE_STDPERIPH_DRIVER`，Include `.\library;.\start;.\user;.\system`；
- 命令行：`UV4.exe -b smart-pill-box.uvprojx -j0 -o build.log`。

## 上电行为

1. 初始化时钟、GPIO、串口、屏、按键、限位、电机、ESP32、音频、WS2812；
2. 从内部 Flash 读取计划（无有效数据则写默认 08:00、19:30 各 1 粒）；
3. 启动 IWDG（约 4s 超时），进入主循环：每 10ms 扫描按键 + 跑状态机，每 1s 进一次软件时钟，每轮喂狗。

## 待办

- [ ] 原理图/PCB 修正：见 `docs/硬件评审.md`（5V 源、LCD_SDA、ESP32 地、FLASH_CS、BAT_ADC、VBAT、WS2812 供电、EN 极性）；
- [ ] 飞线验证后打开 `FEATURE_EXT_FLASH` / `FEATURE_BAT_ADC`；
- [ ] ESP32-C3 端固件（配网、对时、上报、检测结果）；
- [ ] RTC（LSE）+ 掉电保持时间；
- [ ] 屏幕中文字库与界面；
- [ ] 出药步数标定、每粒药步数配置化；
- [ ] 音频提示音数据、录音上传。
