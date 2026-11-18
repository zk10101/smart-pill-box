#include "app_actuator.h"
#include "app_port.h"
#include "app_config.h"

static ActState s_state;
static ActError s_error;
static ActParams s_params;
static uint8_t s_current_slot;
static uint8_t s_target_slot;
static uint8_t s_slot_valid;
static uint32_t s_position;
static uint32_t s_move_steps;
static uint32_t s_move_start_ms;
static LogEntry s_log[LOG_ENTRIES];
static uint8_t s_log_count;
static uint8_t s_log_head;

static void params_default(ActParams *p)
{
    p->steps_per_slot = MOTOR_DEFAULT_STEPS_PER_SLOT;
    p->speed_hz = MOTOR_DEFAULT_SPEED_HZ;
    p->en_active_low = MOTOR_DEFAULT_EN_ACTIVE_LOW;
    p->dir_invert = MOTOR_DEFAULT_DIR_INVERT;
    p->home_backoff = 0;
    p->move_timeout_10ms = (uint16_t)(MOTOR_MOVE_TIMEOUT_MS / 10U);
    p->crc = 0;
    p->crc = Flash_ParamsCrc(p);
}

static void stop_motor(void)
{
    Motor_Stop();
    Motor_Enable((uint8_t)(s_params.en_active_low ? 1U : 0U));
}

static void push_log(uint8_t ev, uint8_t a, uint8_t b)
{
#if FEATURE_LOGGING
    LogEntry *e = &s_log[s_log_head];
    e->ev = ev;
    e->a = a;
    e->b = b;
    e->t_ms = (uint16_t)(Time_Ms() & 0xFFFFU);
    s_log_head = (uint8_t)((s_log_head + 1U) % LOG_ENTRIES);
    if (s_log_count < LOG_ENTRIES) s_log_count++;
#else
    (void)ev; (void)a; (void)b;
#endif
}

static void fail(ActError err, uint8_t ev)
{
    stop_motor();
    s_state = ACT_ST_ERROR;
    s_error = err;
    s_slot_valid = 0;
    s_current_slot = 0;
    push_log(ev, (uint8_t)err, s_target_slot);
}

void Act_Init(void)
{
    uint8_t loaded;
    s_state = ACT_ST_IDLE;
    s_error = ACT_ERR_NONE;
    s_current_slot = 0;
    s_target_slot = 0;
    s_slot_valid = 0;
    s_position = 0;
    s_move_steps = 0;
    s_log_count = 0;
    s_log_head = 0;

    params_default(&s_params);
    loaded = Flash_LoadParams(&s_params);
    if (!loaded) {
        params_default(&s_params);
        (void)Flash_SaveParams(&s_params);
    }
    if (s_params.steps_per_slot > 20000U ||
        s_params.speed_hz < MOTOR_MIN_SPEED_HZ ||
        s_params.speed_hz > MOTOR_MAX_SPEED_HZ) {
        params_default(&s_params);
    }
    Motor_SetSpeed(s_params.speed_hz);
    stop_motor();
    push_log(EVT_POWER_ON, loaded, 0);
}

uint8_t Act_IsBusy(void) { return (uint8_t)(s_state == ACT_ST_MOVING); }
uint8_t Act_IsHomed(void) { return s_slot_valid; }
ActState Act_GetState(void) { return s_state; }
ActError Act_GetError(void) { return s_error; }

void Act_GetStatus(ActStatus *st)
{
    uint8_t flags = 0;
    if (PwrHw_Is5V()) flags |= 0x01;
    if (PwrHw_IsUsb()) flags |= 0x02;
    if (Limit_Org()) flags |= 0x04;
    if (Limit_Pos()) flags |= 0x08;
    if (Motor_IsBusy()) flags |= 0x10;
    if (s_slot_valid) flags |= 0x20;
    st->state = (uint8_t)s_state;
    st->error = (uint8_t)s_error;
    st->flags = flags;
    st->current_slot = s_current_slot;
    st->target_slot = s_target_slot;
    st->slot_valid = s_slot_valid;
    st->position = s_position;
    st->steps_per_slot = s_params.steps_per_slot;
    st->speed_hz = s_params.speed_hz;
}

void Act_GetParams(ActParams *p) { *p = s_params; }

uint8_t Act_SetParams(const ActParams *p)
{
    if (Act_IsBusy()) return ACT_RC_BUSY;
    if (p->steps_per_slot == 0U || p->steps_per_slot > 20000U) return ACT_RC_PARAM;
    if (p->speed_hz < MOTOR_MIN_SPEED_HZ || p->speed_hz > MOTOR_MAX_SPEED_HZ) return ACT_RC_PARAM;
    if (p->move_timeout_10ms < 50U || p->move_timeout_10ms > 6000U) return ACT_RC_PARAM;
    s_params.steps_per_slot = p->steps_per_slot;
    s_params.speed_hz = p->speed_hz;
    s_params.en_active_low = (uint8_t)(p->en_active_low ? 1U : 0U);
    s_params.dir_invert = (uint8_t)(p->dir_invert ? 1U : 0U);
    s_params.home_backoff = 0;
    s_params.move_timeout_10ms = p->move_timeout_10ms;
    s_params.crc = Flash_ParamsCrc(&s_params);
    Motor_SetSpeed(s_params.speed_hz);
    if (!Flash_SaveParams(&s_params)) return ACT_RC_PARAM;
    push_log(EVT_PARAM_SAVED, (uint8_t)(p->steps_per_slot >> 8), (uint8_t)p->steps_per_slot);
    return ACT_RC_OK;
}

uint8_t Act_SyncSlot(uint8_t current)
{
    if (Act_IsBusy()) return ACT_RC_BUSY;
    if (s_state == ACT_ST_ERROR) return ACT_RC_FAULT;
    if (current < 1U || current > SLOT_COUNT) return ACT_RC_PARAM;
    s_current_slot = current;
    s_target_slot = current;
    s_slot_valid = 1;
    return ACT_RC_OK;
}

uint8_t Act_MoveToSlot(uint8_t current, uint8_t target)
{
    uint8_t delta;
    uint32_t steps;
    uint32_t expected_ms;
    if (Act_IsBusy()) return ACT_RC_BUSY;
    if (s_state == ACT_ST_ERROR) return ACT_RC_FAULT;
    if (current < 1U || current > SLOT_COUNT || target < 1U || target > SLOT_COUNT)
        return ACT_RC_PARAM;
    if (s_slot_valid && current != s_current_slot) return ACT_RC_POS_MISMATCH;
    if (s_params.steps_per_slot == 0U) return ACT_RC_NOT_CALIBRATED;
    if (!PwrHw_Is5V()) return ACT_RC_5V_OFF; /* Board 5V powers the 74HCT245. */
    if (Motor_IsFault()) return ACT_RC_FAULT;

    delta = (uint8_t)((target + SLOT_COUNT - current) % SLOT_COUNT);
    if (delta == 0U) {
        s_current_slot = current;
        s_target_slot = target;
        s_slot_valid = 1;
        return ACT_RC_OK;
    }
    steps = (uint32_t)delta * (uint32_t)s_params.steps_per_slot;
    expected_ms = (steps * 1000UL) / s_params.speed_hz + 2000UL;
    if (steps > MOTOR_MAX_STEPS_PER_MOVE ||
        expected_ms > (uint32_t)s_params.move_timeout_10ms * 10UL)
        return ACT_RC_PARAM;

    s_current_slot = current;
    s_target_slot = target;
    s_error = ACT_ERR_NONE;
    s_state = ACT_ST_MOVING;
    s_slot_valid = 0;
    s_move_steps = steps;
    s_move_start_ms = Time_Ms();
    Motor_SetSpeed(s_params.speed_hz);
    Motor_SetDir((uint8_t)(s_params.dir_invert ? 0U : 1U));
    Motor_Enable((uint8_t)(s_params.en_active_low ? 0U : 1U));
    Motor_Start(steps);
    push_log(EVT_MOVE_START, current, target);
    return ACT_RC_OK;
}

void Act_Stop(void)
{
    if (s_state == ACT_ST_MOVING) {
        stop_motor();
        s_state = ACT_ST_IDLE;
        s_current_slot = 0;
        s_slot_valid = 0;
        push_log(EVT_MOVE_ABORT, 0, s_target_slot);
    }
}

void Act_ClearError(void)
{
    if (s_state == ACT_ST_ERROR) {
        stop_motor();
        s_state = ACT_ST_IDLE;
        s_error = ACT_ERR_NONE;
        s_current_slot = 0;
        s_slot_valid = 0;
        Buzzer_Alarm(0);
    }
}

void Act_Tick(void)
{
    if (s_state != ACT_ST_MOVING) return;
    if (Motor_IsFault()) {
        fail(ACT_ERR_FAULT, EVT_FAULT);
    } else if (Time_Ms() - s_move_start_ms >=
               (uint32_t)s_params.move_timeout_10ms * 10UL) {
        fail(ACT_ERR_TIMEOUT, EVT_TIMEOUT);
    } else if (Motor_TakeDone()) {
        stop_motor();
        s_position += s_move_steps;
        s_current_slot = s_target_slot;
        s_slot_valid = 1;
        s_state = ACT_ST_IDLE;
        push_log(EVT_MOVE_DONE, s_current_slot, 0);
    }
}

void AppLog_Push(uint8_t ev, uint8_t a, uint8_t b) { push_log(ev, a, b); }
uint8_t AppLog_Count(void) { return s_log_count; }

uint8_t AppLog_GetNewest(uint8_t index, LogEntry *out)
{
    uint8_t i;
    if (index >= s_log_count) return 0;
    i = (uint8_t)((s_log_head + LOG_ENTRIES - 1U - index) % LOG_ENTRIES);
    *out = s_log[i];
    return 1;
}
