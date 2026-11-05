#ifndef __APP_CONFIG_H
#define __APP_CONFIG_H
/**
 * ============================================================
 * 应用层可调参数（业务层/表现层使用，不含任何寄存器内容）
 * 运行时可用 SET_PARAMS 指令覆盖（见 app_command.c），
 * 掉电保存在内部 Flash 最后一页（bsp_flash.c）。
 * ============================================================
 */

#define FW_VERSION              "V1.0"
#define APP_NAME                "SMART-PILL-BOX-ACTUATOR"

/* ================= 出药机构（步进） ================= */
#define MOTOR_DEFAULT_STEPS_PER_PILL   800U    /* 一粒药 = 多少 STEP 脉冲（按机构标定） */
#define MOTOR_DEFAULT_SPEED_HZ         800U    /* 默认脉冲频率 */
#define MOTOR_MIN_SPEED_HZ             100U
#define MOTOR_MAX_SPEED_HZ             4000U

/* 驱动器 EN 有效电平：U4.3(EN) 网络带 R24 10k 下拉，A4988/DRV8825/TMC 均为低有效 */
#define MOTOR_DEFAULT_EN_ACTIVE_LOW    1U
/* 方向取反：装配方向相反时置 1（可用 SET_PARAMS 的 flags.bit0 覆盖） */
#define MOTOR_DEFAULT_DIR_INVERT       0U

/* 回零：朝 LIM_ORG 走的最大步数与超时，触发后回退的步数 */
#define MOTOR_HOME_MAX_STEPS           40000UL
#define MOTOR_HOME_TIMEOUT_MS          15000UL
#define MOTOR_HOME_BACKOFF_STEPS       40U

/* 单次运动超时（按 800 步/粒、800Hz 估算留足余量） */
#define MOTOR_MOVE_TIMEOUT_MS          8000UL
/* 单次指令允许的最大步数，防止参数错误把机构顶死 */
#define MOTOR_MAX_STEPS_PER_MOVE       200000UL

/* 出药粒数上限（一条指令） */
#define DISPENSE_MAX_PILLS             10U

/* ================= 电源（IP5306） ================= */
#define POWER_KEY_PULSE_MS             120U    /* PWR_KEY 脉宽 */
#define POWER_ON_WAIT_MS               800U    /* 等待 5V 建立 */
#define POWER_OFF_WAIT_MS              400U

/* ================= 时间 / 看门狗 ================= */
#define IWDG_RELOAD_VALUE              2500U   /* LSI 40kHz / 64 ≈ 625Hz -> 4s */
#define LOOP_TICK_MS                   1U
#define UI_REFRESH_MS                  100U

/* ================= 串口 ================= */
#define UART_BAUDRATE                  115200U
#define UART_RX_RING_SIZE              128U    /* 每通道接收环形缓冲(2的幂) */
#define PROTO_MAX_DATA                 128U    /* 单帧数据区上限（LOG 帧最大 1+20*5=101） */
#define PROTO_FRAME_TIMEOUT_MS         200U    /* 半帧超时，超时丢弃 */

/* ================= 显示 / 提示 ================= */
#define FEATURE_LCD                    1
#define FEATURE_WS2812                 1
#define FEATURE_LOGGING                1
#define LOG_ENTRIES                    32U
#define LOG_SEND_MAX                   20U     /* 单次 GET_LOG 最多回传条数 */

/* ================= 电池采样（硬件未连，保持关闭） ================= */
#define FEATURE_BAT_ADC                0

#endif /* __APP_CONFIG_H */
