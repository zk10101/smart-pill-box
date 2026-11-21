# ESP32-C3 与 STM32 通信联调

本仓库的 ESP 固件在 `esp32/zhinengyaohe_esp/`。顶层相邻目录 `zhinengyaohe_esp/` 的旧 `.ino` 是原始草稿，命令码、校验和和在线判断均与当前 STM32 不兼容，不应烧录。`云端/stm32_pillbox_comm_v4.c` 也是旧设计示例，使用 `OPEN:1\r\n` 等文本命令和不同的引脚，不能直接并入当前 Keil 工程。

## 已实现的数据流

`ESP32-C3 -> USART1 -> STM32`：每 3 秒发含 nonce 的 `LINK_PING`，STM32 回 `LINK_PONG`；ESP 校验帧、nonce 和协议版本。ESP 每 3 秒再发 `NET_STATUS`，让 STM32 屏幕显示 Wi-Fi/MQTT 的连接结果。ESP 每 10 秒请求一次 `GET_STATUS`，读取 STM32 的状态、故障和当前格号。MQTT 配置完成后，ESP 发布带 `stm32_online`、运行时间、串口溢出计数和状态的 JSON 遥测；云端下行目前只允许文本 `GET_STATUS`，不会触发电机。

## 烧录前

1. STM32 使用本仓库 Keil 工程烧录。ESP 使用 Arduino-ESP32 的 ESP32-C3 板型；依赖 `PubSubClient`。请将真实 Wi-Fi/MQTT 参数写入草图同目录的 `local_config.h`，字段模板见 `local_config.example.h`。该文件已被 Git 忽略，不要把密码提交到 GitHub。没有 Wi-Fi/MQTT 参数时，ESP 仍可完成 UART 诊断。
2. 板上 ESP 的 RXD0/TXD0 分别经 1k 接 STM32 PA9 TX/PA10 RX；ESP 的 GPIO20=RXD0、GPIO21=TXD0，固件用这两脚跑 115200-8N1。STM32 PA8 控制 ESP EN，高电平使能。不要把 12V 接入这条串口。
3. 板上 H3 只引出 IO9/GND，不能单独传送烧录数据。首次烧录可使用 [STM32 临时串口烧录桥](../tools/esp_flash_bridge/README.md)：电脑 USB-TTL 接 H1 的 USART2，STM32 透传到已连好的 ESP UART0；H3 把 IO9 拉低。桥程序烧入 STM32 后再用 esptool 烧 ESP，最后恢复正常 STM32 固件。
4. 原理图评审记录 ESP32-C3 地焊盘阵列可能未接 GND；如实板确实不导通，先补接地再做 Wi-Fi 稳定性测试。

## 分段证明

| 步骤 | 操作 | 可观察证据 | 证明范围 |
|---|---|---|---|
| 1 | 只烧 STM32、上电 | 屏幕 `ESP:OFF WIFI:OFF MQTT:OFF` | STM32 程序与显示工作 |
| 2 | 烧 ESP、两芯片上电 | 屏幕约 3 秒内 `ESP:ON` | STM32 确实收到了格式正确的 ESP PING；ESP 还需收到匹配 PONG 才认定双向在线 |
| 3 | 配置 Wi-Fi | 屏幕 `WIFI:ON` | ESP 已连到 Wi-Fi AP；不等于已接云端 |
| 4 | 云端参数和证书确定后配置 MQTT | 屏幕 `MQTT:ON` | ESP 与 broker 建立 MQTT 连接；不等于云端业务已处理 |
| 5 | 云端订阅配置的状态 topic | 连续收到 JSON，`stm32_online=true` 且 `stm32_uptime_ms` 增长 | ESP 收到校验和、nonce、版本都正确的 STM32 回包，完成端到端双向链路验证 |
| 6 | 向配置的命令 topic 发 `GET_STATUS` | 状态 topic 更新 STM32 状态 | 云端下行至 ESP、ESP 请求 STM32、STM32 回传、ESP 上报的只读闭环 |

拔掉 ESP 或让 ESP 停止发送后，STM32 屏幕最多 10 秒显示 `ESP:OFF`；Wi-Fi/MQTT 一同失效。若 ESP 仍能联网但 STM32 无响应，云端 JSON 应为 `stm32_online=false`。`stm32_rx_overflow` 非零且持续增长说明 STM32 接收缓冲发生丢字节，应检查串口波特率、接线和主循环负载。

没有 MQTT 账号时，可以在 ESP GPIO20/21 对应的 UART0 信号线上用逻辑分析仪或 3.3V USB-TTL 分别监听两向数据（共地，监听器只接 RX，勿与板载发送端并联驱动）：ESP→STM32 可见 `AA 55 03 01 nonce sum`，STM32→ESP 可见 `AA 55 86 07 nonce 01 uptime(4) overflow sum`。两向 nonce 相同且校验和正确，才是串口双向通信证据。STM32 屏幕 `ESP:ON` 单独只能证明其收到 ESP 的有效 PING，不能证明 ESP 收到了回包。当前未实板烧录，因此这些是可执行验收步骤，不是已测结果。

## 给云端同伴确认

请对方提供脱敏接口说明，不要在群聊或仓库提交设备密钥：实际 MQTT broker 域名与端口、是否必须 TLS 及 CA 证书、client ID/username/password 的生成规则、状态上报 topic、命令订阅 topic、下行 payload 示例、是否要求请求 ID 与应答 topic、离线重发和重复命令处理规则。还需确认手机 BLE 直连板载 ESP32-C3，还是接 H1 的外置模块；目前两条方案均未实现。当前 `local_config.example.h` 只是配置入口，**不表示 OneNET 旧示例中的 topic 或 JSON 格式已经得到云端确认**。收到这些信息后再启用云端转格指令。

旧资料要求的 `BAT:百分比,充电状态` 目前不能由本版 STM32 固件如实给出：原理图中的 `BAT_ADC` 未接主控 ADC，PC8/PC9 只是 USB/5V 是否存在的分压检测，不能代表“充电中/已充满”。云端与 APP 暂时不要把这些字段当成已实现功能。
