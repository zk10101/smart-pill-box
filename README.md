# 智能药盒（Smart Pill Box）

老人用药管理嵌入式系统。主控 **STM32F103RCT6**，开发环境 **Keil MDK + 标准外设库**。

## 功能概览

- 定时服药提醒（蜂鸣器 + 彩灯 + 屏幕）
- 步进电机自动出药（限位 + 超时保护）
- 按键确认 / 复检 / 漏服告警
- ESP32-C3 无线联网上报（USART1）
- SPI 彩屏显示、WS2812 彩灯、I2S 语音播报 + 麦克风
- 服药计划掉电保存（STM32 内部 Flash 兜底）

## 目录结构

```
smart-pill-box/
├── library/          ST 标准外设库
├── start/            启动文件 + CMSIS + system
├── system/           延时
├── user/             应用代码
│   ├── pinmap.h      引脚分配表（权威）
│   ├── app_config.h  业务常量 + 功能开关
│   ├── protocol.h/.c 串口帧协议
│   ├── bsp_*.c/.h    各外设驱动
│   ├── state_machine.c/.h  五态状态机
│   └── main.c        主循环
├── docs/             设计文档
└── smart-pill-box.uvprojx   Keil 工程
```

## 引脚分配（详见 `user/pinmap.h`）

| 功能 | 引脚 |
|------|------|
| 步进电机 | STEP=PB8(TIM4_CH3)、DIR=PB9、EN=PD2、FAULT=PC13 |
| SPI 彩屏 | CS=PA4、SCK=PA5、MOSI=PA7、DC=PB0、RES=PB1、背光=PB10(低有效) |
| ESP32-C3 | EN=PA8、TX=PA9、RX=PA10（USART1） |
| 功放 I2S3 | WS=PA15、CK=PB3、SD=PB5 |
| 麦克风 I2S2 | WS=PB12、CK=PB13、SD=PB15 |
| 蜂鸣器 | PB11（高电平响） |
| WS2812×3 | PA11 |
| 按键×4 | PC0~PC3 |
| 限位×2 | PC6(原点)、PC7(到位) |
| 电源状态 | PC8(充电)、PC9(待机) |

## 编译

1. Keil MDK 打开 `smart-pill-box.uvprojx`
2. 若提示找不到下载算法，在 Options → Debug → Flash Download 选 256K 算法
3. Rebuild → 烧录

## 功能开关（`user/app_config.h`）

| 宏 | 当前值 | 说明 |
|----|--------|------|
| `FEATURE_BAT_ADC` | 0 | 电池电量检测。原理图 BAT_ADC 未接 MCU，待硬件补线后置 1 |
| `FEATURE_EXT_FLASH` | 0 | 外部 W25Q64 存储。FLASH_CS 未接 MCU，待补线后置 1 |

> 服药计划持久化已用 STM32 内部 Flash（`bsp_storage.c`）兜底，不依赖外部 W25Q64。

## 待办

- [ ] 硬件补 BAT_ADC → PA0、FLASH_CS → 空闲 GPIO 两根线
- [ ] RTC（LSE 32.768k）走时接入，替换当前软件时钟
- [ ] 标定步进电机"每粒药对应步数"
- [ ] 中文字库与屏幕 UI
