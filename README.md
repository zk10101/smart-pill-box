# 智能药盒 · 执行机构固件（STM32F103RCT6）

本仓库是智能药盒的**动作与指令执行机构**（下位机）固件。
定时、提醒、云端业务全部移交服务器/ESP32 侧，本板只负责：

- 把收到的指令翻译成**步进电机的确定动作**（回零 / 出药 / 急停）
- 把执行结果、位置、故障**如实上报**给上位机
- 本地按键/屏幕/蜂鸣/指示灯提供**不依赖网络**的兜底操作

硬件基线：最终版单板原理图 `1_1`（EasyEDA Pro 工程 `智能药盒.eprj2`）。
所有引脚都按该原理图核对过，与旧版两份原理图的差异见 `docs/硬件评审.md`。

## 1. 三层架构

```
┌──────────────────── 表现层 (user/app) ────────────────────┐
│  app_command.c  串口指令：解析 0xAA55 帧、分发、回包、事件上报 │
│  app_ui.c       本地界面：按键、ST7789 彩屏、蜂鸣器、WS2812 灯 │
└───────────────────────────┬──────────────────────────────┘
                            │ 调用业务 API（不碰硬件）
┌──────────────────── 业务逻辑层 (user/app) ─────────────────┐
│  app_actuator.c 执行机构状态机：回零/出药/急停/超时/故障       │
│  app_power.c    IP5306 电源管理：先看反馈、再按键             │
└───────────────────────────┬──────────────────────────────┘
                            │ 只依赖接口
┌──────────────── 数据访问层 (user/app_port.h) ──────────────┐
│  user/bsp/*.c   唯一接触 pinmap.h / 寄存器 / 引脚的地方       │
│  （motor / limit / key / buzzer / led / lcd / uart / flash / │
│    power / system）                                          │
└───────────────────────────┬──────────────────────────────┘
                            │
                     STM32F103 外设
```

依赖方向严格单向：`app → app_port.h ← bsp`。
表现层与业务层**不得** `#include "pinmap.h"` 或 `stm32f10x.h`，也不得直接读寄存器；
新增硬件只改 `bsp/` 与 `app_port.h`，业务逻辑不受影响。

### 目录

```
smart-pill-box/
├── start/                 CMSIS + 启动文件 + system_stm32f10x.c
├── library/               STM32F10x 标准外设库 3.5.0
├── system/                Delay（DWT 版，可选）
├── user/
│   ├── main.c             平台启动：初始化顺序 + 时间片 + 看门狗
│   ├── pinmap.h           全部引脚定义（仅 bsp/ 包含）
│   ├── app_config.h       参数默认值与功能开关
│   ├── app_types.h        跨层数据类型（状态机/事件/参数/日志）
│   ├── app_port.h         数据访问层接口（分层契约）
│   ├── app/               表现层 + 业务逻辑层
│   │   ├── app_command.c/h
│   │   ├── app_ui.c/h
│   │   ├── app_actuator.c/h
│   │   └── app_power.c/h
│   └── bsp/               数据访问层实现
│       ├── bsp_system.c   SysTick/时钟/JTAG 释放/复位标志
│       ├── bsp_power.c    PC11 电源键、PC9/PC8 电源状态、PA8 ESP32 EN
│       ├── bsp_motor.c    TIM4_CH3 PWM 脉冲 + 方向/使能 + FAULT
│       ├── bsp_limit.c    PC6/PC7 限位
│       ├── bsp_key.c      PC0-PC3 四键，10ms 扫描、长短按
│       ├── bsp_buzzer.c   PB11 有源蜂鸣器
│       ├── bsp_led.c      PA11 WS2812×3（DWT 精确时序）
│       ├── bsp_lcd.c      SPI1 + ST7789 240×240 + 8×8 字库
│       ├── bsp_uart.c     USART1(ESP32) / USART2(摄像头) + 环形缓冲
│       ├── bsp_flash.c    内部 Flash 末页保存参数（CRC16 校验）
│       └── font8x8.h      8×8 ASCII 字库（PIL 离线生成）
├── docs/
│   ├── 三层架构与接口.md
│   ├── 串口指令协议.md
│   ├── 执行机构状态机.md
│   └── 硬件评审.md
└── smart-pill-box.uvprojx  Keil MDK 工程
```

## 2. 引脚表（最终版 1_1 原理图）

| 功能 | 引脚 | 说明 |
|---|---|---|
| 步进 STEP | PB8 / TIM4_CH3 | 74HCT245 A0→B0→U4.1，1 脉冲 = 1 微步 |
| 步进 DIR | PB9 | A1→B1→U4.2，高=正向（出药） |
| 步进 EN | PC12 | A2→B2→U4.3，默认低有效（10k 下拉），可用参数取反 |
| 驱动器 FAULT | PC13 | FAULT_N 经 R4 1k 进来，**低有效**，10k 上拉 |
| 原点限位 | PC6 | CN2，低有效，10k 上拉 + 100nF |
| 终点限位 | PC7 | CN3，低有效，10k 上拉 + 100nF |
| 电源键 PWR_KEY | PC11 | 高电平→Q3 拉低 IP5306 KEY，脉宽 ≥120ms |
| 5V 存在 | PC9 | STBY_STAT，5V 轨经 100k/100k 分压，高=有 5V |
| USB 插入 | PC8 | CHG_STAT，VBUS_IN 经 100k/100k 分压，高=插着 USB |
| 按键 ×4 | PC0/PC1/PC2/PC3 | UP/DN/OK/BACK，10k 上拉 + 100nF，低有效 |
| 蜂鸣器 | PB11 | 高电平响（NPN 驱动） |
| WS2812 | PA11 | 3 灯串联，GRB，3V3 供电（见硬件问题） |
| 屏 LCD_CS | PA4 | |
| 屏 LCD_DC / RES | PB0 / PB1 | RES 低有效 |
| 屏 背光 | PB10 | 低电平点亮（AO3401A PMOS） |
| 屏 SPI1 | PA5 SCK / PA7 MOSI | **MISO(PA6) 只给 W25Q64，屏不接 MISO** |
| ESP32 串口 | PA9 TX / PA10 RX | USART1，接 ESP32-C3 的 RXD0/TXD0（交叉经 1k） |
| ESP32 EN | PA8 | 高=使能 |
| 摄像头串口 | PA2 TX / PA3 RX | USART2，接 H1（PZ254V-11-04P）的 2/3 脚 |
| SWD | PA13 / PA14 | 上电即释放 JTAG（保留 SWD） |

> H1 四脚定义：1=5V_SW（SW3 手动切换的 5V）、2=摄像头 RX（板 TX）、3=摄像头 TX（板 RX）、4=GND。

## 3. 自上电到出药

1. 电池给 U7(SY8089) 供电 → 3V3 建立，MCU 启动；`main` 依次初始化各层，约 4s 看门狗开始计时（主循环持续喂狗）。
2. `Pwr_Init()` 检查 PC9：无 5V 就按一次 IP5306 KEY，等 5V 起来（**USB 单独供电不足以点亮本板**，必须接电池，见硬件评审 P1-2）。
3. 上电后电机**未回零**，位置未知；第一件事必须是回零：
   - 本地：按 `UP`
   - 远程：发 `0x12 HOME`
4. 回零：反向走 → 触发 PC6 → 正向退 `home_backoff` 步 → 位置清零、`homed=1`。
5. 出药：`Act_Dispense(n)` → 正向发 `n × steps_per_pill` 个脉冲 → 完成回调、位置累加、日志记录。
6. 任何时刻 FAULT/超时/顶到终点限位/长按 OK 急停 → 进入 ERROR，电机失能，红灯闪 + 报警音。

## 4. 操作与显示

| 操作 | 效果 |
|---|---|
| OK 短按 | 出药 1 粒 |
| OK 长按 1s | 急停 |
| UP 短按 | 回零 |
| DOWN 短按 | 清除故障 |
| BACK 短按 | 5V 开/关切换 |
| BACK 长按 | 关闭 5V（软关机） |

WS2812：空闲蓝 / 回零黄 / 出药绿 / 故障红闪。
屏幕显示：状态、错误码、位置步数、最近出药粒数、每粒步数、脉冲频率、5V/USB/两限位/运行标志。

## 5. 串口指令

帧格式 `0xAA 0x55 | CMD | LEN | DATA | SUM`，`SUM=(CMD+LEN+ΣDATA)&0xFF`。
USART1 对接 ESP32-C3（云端指令入口），USART2 对接 K210 摄像头。
完整指令表、回包格式、示例帧见 [`docs/串口指令协议.md`](docs/串口指令协议.md)。

## 6. 编译

Keil MDK（本机 V5.06，`D:\Keil_v5\UV4\UV4.exe`）：

```
UV4.exe -r smart-pill-box.uvprojx -j0 -o build.log
```

当前状态：`0 Error(s), 0 Warning(s)`，Code≈13.5KB、RW≈124B、ZI≈2.4KB。

## 7. 参数与掉电保存

`steps_per_pill / speed_hz / en_active_low / dir_invert / home_backoff / move_timeout` 保存在内部 Flash 末页
（`0x0803F800`，magic `0x5042` + CRC16）。首次上电写入 `app_config.h` 的默认值；
可用 `0x13 SET_PARAMS` 或 `0x14 GET_PARAMS` 读写，写入后立即生效并掉电保持。

## 8. 已知硬件问题（固件已规避，改板时需修）

| 编号 | 问题 | 影响 | 固件规避 |
|---|---|---|---|
| P0-1 | ESP32-C3 的 36-53 脚（GND 大焊盘阵列）只互联、未接 GND | 射频不稳定/复位异常 | 无法规避，必须改板 |
| P0-2 | W25Q64 的 CS# (`FLASH_CS`) 没有连到 MCU 任何引脚 | 外部 Flash 不可用 | 本期不使用 W25Q |
| P1-1 | 3V3 只由电池经 U7 产生，USB 单独供电无法点亮 | 插 USB 无电池不开机 | 上电提示 + 文档说明 |
| P1-2 | WS2812B-XF02/W 的 VDD 接 3V3（规格 3.7~5.3V） | 偏暗、颜色不准 | 提高亮度补偿，长期需改 5V |
| P1-3 | R46 接 `LCD_SDA`，另一端在低分辨率图上无法确认是 MISO 还是 MOSI | 若接 MISO，屏收不到数据 | 固件按 MOSI(PA7) 写；请在 EasyEDA 里确认 |
| P2 | 摄像头 5V_SW 由 SW3 手动切换，MCU 不可控 | 无法软开关摄像头 | 上电前手动打开 SW3 |

## 9. 待办

- K210 侧 Python 程序（通过 H1 收 `0x40` 触发、回 `0x41` 识别结果）尚未编写
- ESP32-C3 侧透传/联网固件不在本仓库
- 屏幕中文字库、出药粒数闭环（光电计数）待做
- `Act_Stop()` 急停后位置会失准，恢复后需重新回零（已写入状态机文档）
