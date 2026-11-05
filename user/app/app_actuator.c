#include "app_actuator.h"
#include "app_port.h"
#include "app_config.h"

/**
 * ============================================================
 * 业务逻辑层：出药执行机构（状态机）
 *
 *   ACT_ST_IDLE   --HOME-->  ACT_ST_HOMING --找到原点--> BACKOFF --> IDLE(pos=0)
 *   ACT_ST_IDLE   --DISPENSE--> ACT_ST_MOVING --脉冲发完--> IDLE(pos+=steps)
 *   任何运动中：FAULT_N / 超时 / 急停 / 终点限位 --> ACT_ST_ERROR
 *
 *   位置单位：STEP 脉冲数（距原点）。粒数换算只在本层完成。
 * ============================================================
 */

typedef enum {
    MOVE_NONE = 0,
    MOVE_HOMING,
    MOVE_HOME_BACKOFF,
    MOVE_DISPENSE
} MoveKind;

/* ---- 内部状态 ---- */
static ActState   s_state;
static ActError   s_error;
static MoveKind   s_kind;
static uint8_t    s_homed;
static uint32_t   s_position;       /* 距原点步数 */
static uint32_t   s_move_steps;     /* 本次目标步数 */
static uint32_t   s_move_start_ms;  /* 本次动作开始时间 */
static uint8_t    s_pills_done;     /* 最近一次出药完成粒数 */
static uint8_t    s_moving_pills;   /* 本次出药请求粒数 */

/* ---- 参数（默认值来自 app_config.h，可被 SET_PARAMS 覆盖并掉电保存） ---- */
static ActParams  s_params;

/* ---- 日志环形缓冲（业务层数据） ---- */
static LogEntry   s_log[LOG_ENTRIES];
static uint8_t    s_log_count;
static uint8_t    s_log_head;       /* 最新一条的下一个位置 */

/* ------------------------------------------------------------ */
static void params_default(ActParams *p)
{
    p->steps_per_pill    = MOTOR_DEFAULT_STEPS_PER_PILL;
    p->speed_hz          = MOTOR_DEFAULT_SPEED_HZ;
    p->en_active_low     = MOTOR_DEFAULT_EN_ACTIVE_LOW;
    p->dir_invert        = MOTOR_DEFAULT_DIR_INVERT;
    p->home_backoff      = MOTOR_HOME_BACKOFF_STEPS;
    p->move_timeout_10ms = (uint16_t)(MOTOR_MOVE_TIMEOUT_MS / 10U);
    p->crc               = 0;
    p->crc               = Flash_ParamsCrc(p);
}

/* EN 有效电平 -> 实际引脚电平 */
static uint8_t en_level(void)
{
    return (uint8_t)(s_params.en_active_low ? 0U : 1U);
}

/* "正向"=出药方向，"反向"=回零方向；dir_invert 用于纠正装配方向 */
static uint8_t dir_forward_is_high(void)
{
    return (uint8_t)(s_params.dir_invert ? 0U : 1U);
}

static void set_dir(uint8_t forward)
{
    uint8_t fwd = dir_forward_is_high() ? forward : (uint8_t)(!forward);

    Motor_SetDir(fwd);
}

static void stop_motor(void)
{
    Motor_Stop();
    Motor_Enable((uint8_t)(s_params.en_active_low ? 1U : 0U));   /* 写"失能"电平 */
}

static void fail(ActError err, uint8_t ev)
{
    stop_motor();
    s_state = ACT_ST_ERROR;
    s_error = err;
    AppLog_Push(ev, (uint8_t)err, 0);
}

static void push_log(uint8_t ev, uint8_t a, uint8_t b)
{
#if FEATURE_LOGGING
    LogEntry *e = &s_log[s_log_head];

    e->ev   = ev;
    e->a    = a;
    e->b    = b;
    e->t_ms = (uint16_t)(Time_Ms() & 0xFFFFU);

    s_log_head = (uint8_t)((s_log_head + 1U) % LOG_ENTRIES);
    if (s_log_count < LOG_ENTRIES) s_log_count++;
#else
    (void)ev; (void)a; (void)b;
#endif
}

/* ------------------------------------------------------------ */
void Act_Init(void)
{
    uint8_t ok;

    s_state       = ACT_ST_IDLE;
    s_error       = ACT_ERR_NONE;
    s_kind        = MOVE_NONE;
    s_homed       = 0;
    s_position    = 0;
    s_pills_done  = 0;
    s_moving_pills = 0;
    s_log_count   = 0;
    s_log_head    = 0;

    params_default(&s_params);
    ok = Flash_LoadParams(&s_params);
    if (!ok) {
        params_default(&s_params);
        (void)Flash_SaveParams(&s_params);      /* 首次上电写入默认值 */
    }
    if (s_params.steps_per_pill == 0U) s_params.steps_per_pill = MOTOR_DEFAULT_STEPS_PER_PILL;
    if (s_params.speed_hz < MOTOR_MIN_SPEED_HZ || s_params.speed_hz > MOTOR_MAX_SPEED_HZ)
        s_params.speed_hz = MOTOR_DEFAULT_SPEED_HZ;

    Motor_SetSpeed(s_params.speed_hz);
    stop_motor();

    push_log(EVT_POWER_ON, (uint8_t)(ok ? 1U : 0U), 0);
}

uint8_t Act_IsBusy(void)
{
    return (uint8_t)(s_state == ACT_ST_HOMING || s_state == ACT_ST_MOVING);
}

uint8_t Act_IsHomed(void)
{
    return s_homed;
}

ActState Act_GetState(void)
{
    return s_state;
}

ActError Act_GetError(void)
{
    return s_error;
}

void Act_GetStatus(ActStatus *st)
{
    uint8_t flags = 0;

    if (PwrHw_Is5V())  flags |= 0x01;
    if (PwrHw_IsUsb()) flags |= 0x02;
    if (Limit_Org())   flags |= 0x04;
    if (Limit_Pos())   flags |= 0x08;
    if (Motor_IsBusy()) flags |= 0x10;

    st->state          = (uint8_t)s_state;
    st->error          = (uint8_t)s_error;
    st->flags          = flags;
    st->pills_done     = s_pills_done;
    st->position       = s_position;
    st->steps_per_pill = s_params.steps_per_pill;
    st->speed_hz       = s_params.speed_hz;
}

void Act_GetParams(ActParams *p)
{
    *p = s_params;
}

uint8_t Act_SetParams(const ActParams *p)
{
    if (p->steps_per_pill == 0U || p->steps_per_pill > 20000U) return ACT_RC_PARAM;
    if (p->speed_hz < MOTOR_MIN_SPEED_HZ || p->speed_hz > MOTOR_MAX_SPEED_HZ) return ACT_RC_PARAM;
    if (p->home_backoff > 2000U) return ACT_RC_PARAM;
    if (p->move_timeout_10ms < 50U || p->move_timeout_10ms > 6000U) return ACT_RC_PARAM;

    if (Act_IsBusy()) return ACT_RC_BUSY;

    s_params.steps_per_pill    = p->steps_per_pill;
    s_params.speed_hz          = p->speed_hz;
    s_params.en_active_low     = (uint8_t)(p->en_active_low ? 1U : 0U);
    s_params.dir_invert        = (uint8_t)(p->dir_invert ? 1U : 0U);
    s_params.home_backoff      = p->home_backoff;
    s_params.move_timeout_10ms = p->move_timeout_10ms;
    s_params.crc               = Flash_ParamsCrc(&s_params);

    Motor_SetSpeed(s_params.speed_hz);

    if (!Flash_SaveParams(&s_params)) return ACT_RC_PARAM;

    push_log(EVT_PARAM_SAVED, (uint8_t)(s_params.speed_hz >> 8), (uint8_t)s_params.speed_hz);
    return ACT_RC_OK;
}

uint8_t Act_Home(void)
{
    if (Act_IsBusy()) return ACT_RC_BUSY;
    if (!PwrHw_Is5V()) return ACT_RC_5V_OFF;

    s_error = ACT_ERR_NONE;
    s_state = ACT_ST_HOMING;
    s_kind  = MOVE_HOMING;

    Motor_SetSpeed(s_params.speed_hz);
    set_dir(0);                                   /* 反向 = 朝原点 */
    Motor_Enable(en_level());
    s_move_start_ms = Time_Ms();
    Motor_Start(MOTOR_HOME_MAX_STEPS);

    push_log(EVT_HOME_START, 0, 0);
    return ACT_RC_OK;
}

uint8_t Act_Dispense(uint8_t pills)
{
    uint32_t steps;

    if (Act_IsBusy()) return ACT_RC_BUSY;
    if (!s_homed)     return ACT_RC_NOT_HOMED;
    if (!PwrHw_Is5V()) return ACT_RC_5V_OFF;
    if (pills == 0U || pills > DISPENSE_MAX_PILLS) return ACT_RC_PARAM;

    steps = (uint32_t)pills * (uint32_t)s_params.steps_per_pill;
    if (steps == 0U || steps > MOTOR_MAX_STEPS_PER_MOVE) return ACT_RC_PARAM;

    s_error        = ACT_ERR_NONE;
    s_state        = ACT_ST_MOVING;
    s_kind         = MOVE_DISPENSE;
    s_move_steps   = steps;
    s_moving_pills = pills;
    s_move_start_ms = Time_Ms();

    Motor_SetSpeed(s_params.speed_hz);
    set_dir(1);                                   /* 正向 = 出药 */
    Motor_Enable(en_level());
    Motor_Start(steps);

    push_log(EVT_MOVE_START, pills, (uint8_t)(steps >> 8));
    return ACT_RC_OK;
}

void Act_Stop(void)
{
    if (s_state == ACT_ST_HOMING || s_state == ACT_ST_MOVING) {
        stop_motor();
        s_state = ACT_ST_IDLE;
        s_kind  = MOVE_NONE;
        push_log(EVT_MOVE_ABORT, (uint8_t)s_state, 0);
    }
}

void Act_ClearError(void)
{
    if (s_state == ACT_ST_ERROR) {
        stop_motor();
        s_state = ACT_ST_IDLE;
        s_error = ACT_ERR_NONE;
        s_kind  = MOVE_NONE;
        Buzzer_Alarm(0);
    }
}

void Act_Tick(void)
{
    uint32_t elapsed_ms;

    switch (s_state) {
    case ACT_ST_HOMING:
        if (Motor_IsFault()) {
            fail(ACT_ERR_FAULT, EVT_FAULT);
            break;
        }

        elapsed_ms = Time_Ms() - s_move_start_ms;

        if (s_kind == MOVE_HOMING) {
            if (Limit_Org()) {                       /* 到达原点 */
                Motor_Stop();
                if (s_params.home_backoff) {
                    s_kind = MOVE_HOME_BACKOFF;
                    s_move_start_ms = Time_Ms();
                    set_dir(1);
                    Motor_Start(s_params.home_backoff);
                } else {
                    s_position = 0;
                    s_homed    = 1;
                    s_state    = ACT_ST_IDLE;
                    s_kind     = MOVE_NONE;
                    stop_motor();
                    push_log(EVT_HOME_DONE, 0, 0);
                }
            } else if (elapsed_ms >= MOTOR_HOME_TIMEOUT_MS) {
                push_log(EVT_HOME_FAIL, (uint8_t)ACT_ERR_TIMEOUT, 0);
                fail(ACT_ERR_NO_LIMIT, EVT_TIMEOUT);
            } else if (!Motor_IsBusy()) {             /* 走满最大行程仍没触发 */
                push_log(EVT_HOME_FAIL, (uint8_t)ACT_ERR_NO_LIMIT, 0);
                fail(ACT_ERR_NO_LIMIT, EVT_HOME_FAIL);
            }
        } else if (s_kind == MOVE_HOME_BACKOFF) {
            if (Limit_Pos()) {
                fail(ACT_ERR_LIMIT_STOP, EVT_HOME_FAIL);
            } else if (elapsed_ms >= MOTOR_HOME_TIMEOUT_MS) {
                fail(ACT_ERR_TIMEOUT, EVT_TIMEOUT);
            } else if (Motor_TakeDone() || !Motor_IsBusy()) {
                stop_motor();
                s_position = 0;
                s_homed    = 1;
                s_state    = ACT_ST_IDLE;
                s_kind     = MOVE_NONE;
                push_log(EVT_HOME_DONE, 0, 0);
            }
        }
        break;

    case ACT_ST_MOVING:
        if (Motor_IsFault()) {
            fail(ACT_ERR_FAULT, EVT_FAULT);
            break;
        }
        if (Limit_Pos()) {                            /* 顶到行程终点，立即停机 */
            fail(ACT_ERR_LIMIT_STOP, EVT_FAULT);
            break;
        }

        elapsed_ms = Time_Ms() - s_move_start_ms;
        if (elapsed_ms >= (uint32_t)s_params.move_timeout_10ms * 10U) {
            fail(ACT_ERR_TIMEOUT, EVT_TIMEOUT);
            break;
        }

        if (Motor_TakeDone() || !Motor_IsBusy()) {
            stop_motor();
            s_position  = (uint32_t)(s_position + s_move_steps);
            s_pills_done = s_moving_pills;
            s_state     = ACT_ST_IDLE;
            s_kind      = MOVE_NONE;
            push_log(EVT_MOVE_DONE, s_moving_pills, 0);
        }
        break;

    case ACT_ST_ERROR:
        /* 错误保持：驱动器仍在故障时持续报警由 UI 层处理 */
        break;

    default:
        break;
    }
}

/* ------------------------------------------------------------
 * 日志：通过 app_types.h 中的声明给表现层读取
 * ------------------------------------------------------------ */
void AppLog_Push(uint8_t ev, uint8_t a, uint8_t b)
{
    push_log(ev, a, b);
}

uint8_t AppLog_Count(void)
{
    return s_log_count;
}

uint8_t AppLog_GetNewest(uint8_t index, LogEntry *out)
{
    uint8_t i;

    if (index >= s_log_count) return 0;

    i = (uint8_t)((s_log_head + LOG_ENTRIES - 1U - index) % LOG_ENTRIES);
    *out = s_log[i];
    return 1;
}
