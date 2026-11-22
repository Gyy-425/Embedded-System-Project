/* RP2040 motor PWM: fixed-LOW input while running, coast on every stop. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include "../app.h"
#pragma GCC diagnostic pop
#include "motor_io.h"
#include "motor_pulse.h"
#include <limits.h>
#if TK_SUPPORT_SMP
#include <tk/smp.h>
static T_SPLOCK g_motor_pulse_lock;
#endif

#define PULSE_IRQ 0U
#define PULSE_IRQ_PRIORITY 1
#define PULSE_PWM_BASE(slice) (PWM_BASE + ((slice) * 0x14U))
#define PULSE_OUT_HIGH (UINT32_C(3) << 8U)
#define PULSE_TIMER_SET (TIMER_INTE + 0x2000U)
#define PULSE_TIMER_CLEAR (TIMER_INTE + 0x3000U)
#define PULSE_WATCHDOG_CTRL UINT32_C(0x40058000)
#define PULSE_WATCHDOG_TICK UINT32_C(0x4005802C)
#define PULSE_REF_CTRL UINT32_C(0x40008030)
#define PULSE_REF_DIV UINT32_C(0x40008034)
#define PULSE_TICK_1MHZ UINT32_C(0x20C)
#define PULSE_INIT_POLLS 10000U
_Static_assert(CLK_PLL_SYS_FREQ == 125000000U,
               "Motor PWM period assumes the configured 125 MHz clock");

static motor_pulse_status_t g_motor_pulse_status;
static uint32_t g_motor_pulse_started_us;
static int16_t g_motor_drive_duty[2]; /* Signed half-percentage points. */
static bool g_motor_drive_is_continuous;
static bool g_motor_drive_expired;
static uint32_t g_motor_drive_lease_us;

static bool motor_pulse_timebase_valid(void)
{
    return ((in_w(PULSE_REF_CTRL) & 3U) == 2U) &&
           ((in_w(PULSE_REF_DIV) & 0x300U) == 0x100U) &&
           ((in_w(PULSE_WATCHDOG_TICK) & 0x3FFU) == PULSE_TICK_1MHZ) &&
           ((in_w(RESETS_RESET) & RESETS_RESET_TIMER) == 0U) &&
           ((in_w(TIMER_PAUSE) & 1U) == 0U);
}

int32_t motor_timebase_init(void)
{
    /* Startup only: configure the tick without resetting the shared timer. */
    if (((in_w(PULSE_REF_CTRL) & 3U) != 2U) ||
        ((in_w(PULSE_REF_DIV) & 0x300U) != 0x100U) ||
        ((in_w(RESETS_RESET) & RESETS_RESET_TIMER) != 0U) ||
        ((in_w(TIMER_PAUSE) & 1U) != 0U))
    {
        return E_CTX;
    }
    uint32_t tick = (uint32_t)in_w(PULSE_WATCHDOG_TICK);
    if ((tick & 0x3FFU) != PULSE_TICK_1MHZ)
    {
        uint32_t cycles = tick & 0x1FFU;
        if (((cycles != 0U) && (cycles != 12U)) ||
            ((in_w(PULSE_WATCHDOG_CTRL) & (1U << 30U)) != 0U))
        {
            return E_BUSY;
        }
        out_w(PULSE_WATCHDOG_TICK, PULSE_TICK_1MHZ);
    }
    if (!motor_pulse_timebase_valid())
    {
        return E_CTX;
    }
    uint32_t first = (uint32_t)in_w(TIMER_TIMERAWL);
    for (uint32_t index = 0U; index < PULSE_INIT_POLLS; ++index)
    {
        if ((uint32_t)in_w(TIMER_TIMERAWL) != first)
        {
            return E_OK;
        }
    }
    return E_TMOUT;
}

static UINT motor_pulse_lock(void)
{
    UINT saved;
#if TK_SUPPORT_SMP
    (void)ISpinLock(&g_motor_pulse_lock, &saved);
#else
    DI(saved);
#endif
    return saved;
}

static void motor_pulse_unlock(UINT saved)
{
#if TK_SUPPORT_SMP
    (void)ISpinUnlock(&g_motor_pulse_lock, saved);
#else
    EI(saved);
#endif
}

static void motor_pulse_stop_locked(void)
{
    out_w(GPIO_OUT_SET, MOTOR_OUTPUT_MASK);
    if (g_motor_pulse_status.active)
    {
        g_motor_pulse_status.elapsed_us =
            (uint32_t)in_w(TIMER_TIMERAWL) - g_motor_pulse_started_us;
        for (uint32_t wheel = 0U; wheel < 2U; ++wheel)
        {
            if (g_motor_drive_duty[wheel] != 0)
            {
                uint32_t pin_a = 8U + wheel * 2U;
                uint32_t pwm_pin = (g_motor_drive_duty[wheel] > 0) ?
                                   pin_a : pin_a + 1U;
                uint32_t low_pin = (g_motor_drive_duty[wheel] > 0) ?
                                   pin_a + 1U : pin_a;
                /* Raise PWM first: 00 -> requested drive -> 11, no reverse. */
                out_w(GPIO_CTRL(pwm_pin),
                      GPIO_CTRL_FUNCSEL_PWM | PULSE_OUT_HIGH);
                out_w(GPIO_CTRL(low_pin),
                      GPIO_CTRL_FUNCSEL_PWM | PULSE_OUT_HIGH);
            }
        }
    }
    /* Both bridges are now coasted before SIO handoff or PWM disable. */
    for (uint32_t pin = 8U; pin <= 11U; ++pin)
    {
        out_w(GPIO_CTRL(pin), GPIO_CTRL_FUNCSEL_SIO);
    }
    out_w(PULSE_PWM_BASE(4U), 0U);
    out_w(PULSE_PWM_BASE(5U), 0U);
    out_w(PULSE_TIMER_CLEAR, 1U);
    out_w(TIMER_ARMED, 1U);
    out_w(TIMER_INTR, 1U);
    g_motor_pulse_status.active = false;
    g_motor_drive_duty[0] = 0;
    g_motor_drive_duty[1] = 0;
    g_motor_drive_is_continuous = false;
}

static void motor_pulse_irq(UINT intno)
{
    UINT saved = motor_pulse_lock();
    (void)intno;
    if ((in_w(TIMER_INTS) & 1U) != 0U)
    {
        if (g_motor_pulse_status.active && g_motor_drive_is_continuous)
        {
            g_motor_drive_expired = true;
        }
        if (g_motor_pulse_status.active &&
            (g_motor_pulse_status.cutoff_count < UINT32_MAX))
        {
            ++g_motor_pulse_status.cutoff_count;
        }
        motor_pulse_stop_locked();
    }
    motor_pulse_unlock(saved);
}

int32_t motor_pulse_init(void)
{
    const T_DINT attributes =
        {.intatr = TA_HLNG, .inthdr = (FP)motor_pulse_irq};
    ER error;
#if TK_SUPPORT_SMP
    if (tk_get_prc() != 1)
    {
        return E_CTX;
    }
    error = InitSpinLock(&g_motor_pulse_lock);
    if (error < E_OK)
    {
        return error;
    }
#endif
    g_motor_pulse_status = (motor_pulse_status_t){0};
    g_motor_drive_expired = false;
    g_motor_drive_duty[0] = 0;
    g_motor_drive_duty[1] = 0;
    g_motor_drive_is_continuous = false;
    if (!motor_pulse_timebase_valid())
    {
        g_motor_pulse_status.error = E_CTX;
        return E_CTX;
    }
    /* Deassert only PWM reset; leave the shared timer counter untouched. */
    out_w(RESETS_RESET + 0x3000U, RESETS_RESET_PWM);
    uint32_t polls;
    for (polls = 0U; polls < PULSE_INIT_POLLS; ++polls)
    {
        if ((in_w(RESETS_RESET_DONE) & RESETS_RESET_PWM) != 0U)
        {
            break;
        }
    }
    if (polls == PULSE_INIT_POLLS)
    {
        g_motor_pulse_status.error = E_TMOUT;
        return E_TMOUT;
    }
    if ((((in_w(TIMER_ARMED) | in_w(TIMER_INTE) | in_w(TIMER_INTF)) & 1U)
         != 0U) ||
        (((in_w(PULSE_PWM_BASE(4U)) | in_w(PULSE_PWM_BASE(5U))) & 1U)
         != 0U) ||
        ((in_w(PWM_BASE + 0xA8U) & ((1U << 4U) | (1U << 5U))) != 0U))
    {
        g_motor_pulse_status.error = E_BUSY;
        return E_BUSY;
    }
    DisableInt(PULSE_IRQ);
    error = tk_def_int(PULSE_IRQ, &attributes);
    if (error < E_OK)
    {
        g_motor_pulse_status.error = error;
        return error;
    }
    drv_motor_coast_init();
    motor_pulse_stop_locked();
    ClearInt(PULSE_IRQ);
    g_motor_pulse_status.ready = true;
    EnableInt(PULSE_IRQ, PULSE_IRQ_PRIORITY);
    return E_OK;
}

static bool motor_pulse_missed_deadline(uint32_t duration_us)
{
    return ((uint32_t)in_w(TIMER_TIMERAWL) - g_motor_pulse_started_us >=
            duration_us) || ((in_w(TIMER_ARMED) & 1U) == 0U);
}

static ER motor_pulse_start_locked(uint8_t wheel, uint8_t direction,
                                    uint8_t duty, uint32_t duration_ms)
{
    uint32_t base = PULSE_PWM_BASE(wheel == 1U ? 4U : 5U);
    uint32_t pin_a = (wheel == 1U) ? 8U : 10U;
    uint32_t pin_b = pin_a + 1U;
    uint32_t pwm_cc = MOTOR_PWM_PERIOD * (uint32_t)duty / 100U;
    uint32_t cc_a = (direction == 1U) ? pwm_cc : 0U;
    uint32_t cc_b = (direction == 1U) ? 0U : pwm_cc;
    uint32_t duration_us = duration_ms * 1000U;

    motor_pulse_stop_locked();
    out_w(base + 4U, 16U); /* Divider1.0; hardware PWM at10kHz. */
    out_w(base + 8U, 0U);
    out_w(base + 0x10U, MOTOR_PWM_PERIOD - 1U);
    out_w(base + 0xCU, cc_a | (cc_b << 16U));
    out_w(GPIO_CTRL(pin_a), GPIO_CTRL_FUNCSEL_PWM | PULSE_OUT_HIGH);
    out_w(GPIO_CTRL(pin_b), GPIO_CTRL_FUNCSEL_PWM | PULSE_OUT_HIGH);
    g_motor_pulse_started_us = (uint32_t)in_w(TIMER_TIMERAWL);
    g_motor_pulse_status.wheel = wheel;
    g_motor_pulse_status.direction = direction;
    g_motor_pulse_status.duty = duty;
    g_motor_drive_duty[wheel - 1U] = (int16_t)(
        (direction == 1U) ? 2 * (int32_t)duty : -2 * (int32_t)duty);
    g_motor_pulse_status.elapsed_us = 0U;
    g_motor_pulse_status.active = true;
    out_w(TIMER_INTR, 1U);
    out_w(TIMER_ALARM0, g_motor_pulse_started_us + duration_us);
    out_w(PULSE_TIMER_SET, 1U);
    out_w(base, 1U);
    if (motor_pulse_missed_deadline(duration_us))
    {
        motor_pulse_stop_locked();
        return E_TMOUT;
    }
#if !MOTOR_TEST_PROBE
    uint32_t pwm_pin = (direction == 1U) ? pin_a : pin_b;
    uint32_t low_pin = (direction == 1U) ? pin_b : pin_a;
    /* Drop fixed leg first: 11 -> requested drive -> direct PWM/00. */
    out_w(GPIO_CTRL(low_pin), GPIO_CTRL_FUNCSEL_PWM);
    out_w(GPIO_CTRL(pwm_pin), GPIO_CTRL_FUNCSEL_PWM);
#endif
    /* Probe leaves both force-high overrides intact, regardless of duty. */
    if ((uint32_t)in_w(TIMER_TIMERAWL) - g_motor_pulse_started_us >=
        duration_us)
    {
        motor_pulse_stop_locked();
        return E_TMOUT;
    }
    return E_OK;
}

int32_t motor_pulse_start(uint8_t wheel, uint8_t direction, uint8_t duty,
                         uint32_t duration_ms)
{
    UINT saved = motor_pulse_lock();
    ER error;
    if (!g_motor_pulse_status.ready)
    {
        error = E_CTX;
    }
    else if (g_motor_pulse_status.active)
    {
        error = E_BUSY;
    }
    else if ((wheel < 1U) || (wheel > 2U) ||
             (direction < 1U) || (direction > 2U) ||
             (duty == 0U) || (duty > MOTOR_PULSE_MAX_DUTY) ||
             (duration_ms == 0U) || (duration_ms > MOTOR_PULSE_MS))
    {
        error = E_PAR;
    }
    else
    {
        error = motor_pulse_start_locked(wheel, direction, duty, duration_ms);
    }
    g_motor_pulse_status.error = error;
    motor_pulse_unlock(saved);
    return error;
}

void motor_pulse_stop(void)
{
    UINT saved = motor_pulse_lock();
    if (g_motor_pulse_status.ready)
    {
        motor_pulse_stop_locked();
    }
    motor_pulse_unlock(saved);
}

void motor_pulse_read(motor_pulse_status_t *p_status)
{
    UINT saved = motor_pulse_lock();
    *p_status = g_motor_pulse_status;
    motor_pulse_unlock(saved);
}

static uint32_t motor_drive_magnitude(int16_t duty)
{
    return (uint32_t)((duty < 0) ? -(int32_t)duty : (int32_t)duty);
}

static void motor_drive_configure_wheel(uint32_t wheel, int16_t duty_half_pct)
{
    if (duty_half_pct != 0)
    {
        uint32_t base = PULSE_PWM_BASE(wheel + 4U);
        uint32_t pin_a = 8U + wheel * 2U;
        uint32_t cc = (MOTOR_PWM_PERIOD * motor_drive_magnitude(duty_half_pct) + 100U) / 200U;
        uint32_t cc_pair = (duty_half_pct > 0) ? cc : cc << 16U;

        out_w(base + 4U, 16U);
        out_w(base + 8U, 0U);
        out_w(base + 0x10U, MOTOR_PWM_PERIOD - 1U);
        out_w(base + 0xCU, cc_pair);
        out_w(GPIO_CTRL(pin_a), GPIO_CTRL_FUNCSEL_PWM | PULSE_OUT_HIGH);
        out_w(GPIO_CTRL(pin_a + 1U), GPIO_CTRL_FUNCSEL_PWM | PULSE_OUT_HIGH);
    }
}

static void motor_drive_release_wheel(uint32_t wheel, int16_t duty_half_pct)
{
#if !MOTOR_TEST_PROBE
    if (duty_half_pct != 0)
    {
        uint32_t pin_a = 8U + wheel * 2U;
        uint32_t pwm_pin = (duty_half_pct > 0) ? pin_a : pin_a + 1U;
        uint32_t low_pin = (duty_half_pct > 0) ? pin_a + 1U : pin_a;
        out_w(GPIO_CTRL(low_pin), GPIO_CTRL_FUNCSEL_PWM);
        out_w(GPIO_CTRL(pwm_pin), GPIO_CTRL_FUNCSEL_PWM);
    }
#else
    (void)wheel;
    (void)duty_half_pct;
#endif
}

static uint32_t constrain_deadline(uint32_t anchor_us, uint32_t lease_us,
                                    bool absolute, uint32_t deadline_us)
{
    if (absolute)
    {
        uint32_t remaining = deadline_us - anchor_us;
        if ((remaining == 0U) ||
            (remaining > MOTOR_DRIVE_MAX_LEASE_MS * 1000U))
        {
            return 0U; /* Includes expired deadlines across timer wrap. */
        }
        if (lease_us > remaining) {lease_us = remaining;}
    }
    return lease_us;
}

static ER motor_drive_start_locked(int16_t left_half_pct, int16_t right_half_pct,
                                    uint32_t lease_us, bool absolute,
                                    uint32_t deadline_us)
{
    uint32_t started_us = (uint32_t)in_w(TIMER_TIMERAWL);
    lease_us = constrain_deadline(started_us, lease_us, absolute, deadline_us);
    if (lease_us == 0U)
    {
        g_motor_drive_expired = true;
        motor_pulse_stop_locked();
        return E_TMOUT;
    }
    motor_pulse_stop_locked();
    motor_drive_configure_wheel(0U, left_half_pct);
    motor_drive_configure_wheel(1U, right_half_pct);
    g_motor_drive_duty[0] = left_half_pct;
    g_motor_drive_duty[1] = right_half_pct;
    g_motor_drive_is_continuous = true;
    g_motor_drive_lease_us = lease_us;
    g_motor_pulse_started_us = started_us;
    g_motor_pulse_status.wheel = 0U;
    g_motor_pulse_status.direction = 0U;
    uint32_t left_magnitude = motor_drive_magnitude(left_half_pct);
    uint32_t right_magnitude = motor_drive_magnitude(right_half_pct);
    g_motor_pulse_status.duty = (uint8_t)(
        (left_magnitude > right_magnitude ? left_magnitude : right_magnitude) / 2U);
    g_motor_pulse_status.elapsed_us = 0U;
    g_motor_pulse_status.active = true;
    out_w(TIMER_INTR, 1U);
    out_w(TIMER_ALARM0, g_motor_pulse_started_us + lease_us);
    out_w(PULSE_TIMER_SET, 1U);
    if (left_half_pct != 0)
    {
        out_w(PULSE_PWM_BASE(4U), 1U);
    }
    if (right_half_pct != 0)
    {
        out_w(PULSE_PWM_BASE(5U), 1U);
    }
    if (motor_pulse_missed_deadline(lease_us))
    {
        g_motor_drive_expired = true;
        motor_pulse_stop_locked();
        return E_TMOUT;
    }
    motor_drive_release_wheel(0U, left_half_pct);
    motor_drive_release_wheel(1U, right_half_pct);
    if (motor_pulse_missed_deadline(lease_us))
    {
        g_motor_drive_expired = true;
        motor_pulse_stop_locked();
        return E_TMOUT;
    }
    return E_OK;
}

static ER motor_drive_renew_locked(uint32_t lease_us, bool absolute,
                                    uint32_t deadline_us)
{
    uint32_t previous_started_us = g_motor_pulse_started_us;
    uint32_t previous_lease_us = g_motor_drive_lease_us;
    uint32_t renewed_us = (uint32_t)in_w(TIMER_TIMERAWL);
    lease_us = constrain_deadline(renewed_us, lease_us, absolute, deadline_us);

    if ((lease_us == 0U) ||
        ((uint32_t)(renewed_us - previous_started_us) >= previous_lease_us) ||
        ((in_w(TIMER_ARMED) & 1U) == 0U))
    {
        g_motor_drive_expired = true;
        motor_pulse_stop_locked();
        return E_TMOUT;
    }
    g_motor_pulse_started_us = renewed_us;
    g_motor_drive_lease_us = lease_us;
    out_w(TIMER_INTR, 1U);
    out_w(TIMER_ALARM0, renewed_us + lease_us);
    /* IRQs are masked here. Do not erase expiry that crossed during rearm. */
    if (((uint32_t)((uint32_t)in_w(TIMER_TIMERAWL) - previous_started_us) >=
         previous_lease_us) || motor_pulse_missed_deadline(lease_us))
    {
        g_motor_drive_expired = true;
        motor_pulse_stop_locked();
        return E_TMOUT;
    }
    return E_OK;
}

int32_t motor_drive_rearm(void)
{
    UINT saved = motor_pulse_lock();
    ER error = E_OK;

    if (!g_motor_pulse_status.ready)
    {
        error = E_CTX;
    }
    else if (g_motor_pulse_status.active)
    {
        error = E_BUSY;
    }
    else
    {
        for (uint32_t pin = 8U; pin <= 11U; ++pin)
        {
            if ((in_w(GPIO_CTRL(pin)) & 31U) != GPIO_CTRL_FUNCSEL_SIO)
            {
                error = E_CTX;
            }
        }
        if (((in_w(GPIO_OUT) & MOTOR_OUTPUT_MASK) != MOTOR_OUTPUT_MASK) ||
            (((in_w(TIMER_ARMED) | in_w(TIMER_INTE) | in_w(TIMER_INTF)) &
              1U) != 0U) ||
            (((in_w(PULSE_PWM_BASE(4U)) | in_w(PULSE_PWM_BASE(5U))) &
              1U) != 0U))
        {
            error = E_CTX;
        }
        if (error == E_OK)
        {
            g_motor_drive_expired = false;
            g_motor_pulse_status.error = E_OK;
        }
    }
    motor_pulse_unlock(saved);
    return error;
}

static int32_t motor_drive_set_internal(int16_t left_half_pct, int16_t right_half_pct,
                                         uint32_t lease_ms, bool absolute,
                                         uint32_t deadline_us)
{
    UINT saved = motor_pulse_lock();
    ER error = E_OK;
    const int32_t max_half_pct = 2 * (int32_t)MOTOR_PULSE_MAX_DUTY;

    if (!g_motor_pulse_status.ready)
    {
        error = E_CTX;
    }
    else if (g_motor_drive_expired)
    {
        /* Only startup or an explicit coast-verified rearm clears this. */
        motor_pulse_stop_locked();
        error = E_TMOUT;
    }
    else if (g_motor_pulse_status.active && !g_motor_drive_is_continuous)
    {
        error = E_BUSY;
    }
    else if ((left_half_pct < -max_half_pct) || (left_half_pct > max_half_pct) ||
             (right_half_pct < -max_half_pct) || (right_half_pct > max_half_pct) ||
             (lease_ms == 0U) || (lease_ms > MOTOR_DRIVE_MAX_LEASE_MS))
    {
        motor_pulse_stop_locked();
        error = E_PAR;
    }
    else if (g_motor_pulse_status.active &&
             motor_pulse_missed_deadline(g_motor_drive_lease_us))
    {
        /* Do not renew an expired alarm while its IRQ is pending/masked. */
        g_motor_drive_expired = true;
        motor_pulse_stop_locked();
        error = E_TMOUT;
    }
    else if ((left_half_pct == 0) && (right_half_pct == 0))
    {
        motor_pulse_stop_locked();
    }
    else if (g_motor_pulse_status.active &&
             (left_half_pct == g_motor_drive_duty[0]) &&
             (right_half_pct == g_motor_drive_duty[1]))
    {
        error = motor_drive_renew_locked(lease_ms * 1000U, absolute, deadline_us);
    }
    else
    {
        error = motor_drive_start_locked(left_half_pct, right_half_pct, lease_ms * 1000U,
                                          absolute, deadline_us);
    }
    g_motor_pulse_status.error = error;
    motor_pulse_unlock(saved);
    return error;
}

int32_t motor_drive_set(int8_t left_pct, int8_t right_pct, uint32_t lease_ms)
{
    return motor_drive_set_internal((int16_t)(2 * left_pct), (int16_t)(2 * right_pct),
                                    lease_ms, false, 0U);
}

uint32_t motor_drive_now_us(void)
{
    return (uint32_t)in_w(TIMER_TIMERAWL);
}

int32_t motor_drive_set_until(int8_t left_pct, int8_t right_pct,
                               uint32_t deadline_us)
{
    return motor_drive_set_internal((int16_t)(2 * left_pct), (int16_t)(2 * right_pct),
                                    MOTOR_DRIVE_MAX_LEASE_MS, true, deadline_us);
}

int32_t motor_drive_set_until_half_pct(int8_t left_half_pct, int8_t right_half_pct,
                                       uint32_t deadline_us)
{
    return motor_drive_set_internal(left_half_pct, right_half_pct,
                                    MOTOR_DRIVE_MAX_LEASE_MS, true, deadline_us);
}
