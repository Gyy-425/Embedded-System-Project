#include "../app.h"
#include "motor_io.h"
#include "motor_pulse.h"
#include <limits.h>
#if TK_SUPPORT_SMP
#include <tk/smp.h>
static T_SPLOCK pulse_lock;
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
#define PULSE_TICK_1MHZ UINT32_C(0x20C) /* enable + 12 clk_ref cycles */
#define PULSE_INIT_POLLS 10000U
_Static_assert(CLK_PLL_SYS_FREQ == 125000000U, "Motor PWM period assumes the configured 125 MHz system clock");

static motor_pulse_status_t pulse;
static uint32_t pulse_started_us;

static bool timer_configuration_valid(void)
{
    return ((in_w(PULSE_REF_CTRL) & 3U) == 2U) &&
           ((in_w(PULSE_REF_DIV) & 0x300U) == 0x100U) &&
           ((in_w(PULSE_WATCHDOG_TICK) & 0x3FFU) == PULSE_TICK_1MHZ) &&
           ((in_w(RESETS_RESET) & RESETS_RESET_TIMER) == 0U) &&
           ((in_w(TIMER_PAUSE) & 1U) == 0U);
}

int32_t motor_timebase_init(void)
{
    /* Cold flash boot does not guarantee the USB boot ROM's tick setup.
       Called before the initial task yields: never reset the shared counter. */
    if (((in_w(PULSE_REF_CTRL) & 3U) != 2U) ||
        ((in_w(PULSE_REF_DIV) & 0x300U) != 0x100U) ||
        ((in_w(RESETS_RESET) & RESETS_RESET_TIMER) != 0U) ||
        ((in_w(TIMER_PAUSE) & 1U) != 0U)) {
        return E_CTX;
    }
    uint32_t tick = (uint32_t)in_w(PULSE_WATCHDOG_TICK);
    if ((tick & 0x3FFU) != PULSE_TICK_1MHZ) {
        uint32_t cycles = tick & 0x1FFU;
        if ((cycles != 0U && cycles != 12U) ||
            ((in_w(PULSE_WATCHDOG_CTRL) & (1U << 30U)) != 0U)) {
            return E_BUSY;
        }
        out_w(PULSE_WATCHDOG_TICK, PULSE_TICK_1MHZ);
    }
    if (!timer_configuration_valid()) {
        return E_CTX;
    }
    uint32_t first = (uint32_t)in_w(TIMER_TIMERAWL);
    for (UINT i = 0U; i < PULSE_INIT_POLLS; ++i) {
        if ((uint32_t)in_w(TIMER_TIMERAWL) != first) {
            return E_OK;
        }
    }
    return E_TMOUT;
}

static UINT lock_pulse(void)
{
    UINT saved;
#if TK_SUPPORT_SMP
    (void)ISpinLock(&pulse_lock, &saved);
#else
    DI(saved);
#endif
    return saved;
}

static void unlock_pulse(UINT saved)
{
#if TK_SUPPORT_SMP
    (void)ISpinUnlock(&pulse_lock, saved);
#else
    EI(saved);
#endif
}

static void stop_locked(void)
{
    if (pulse.active) {
        pulse.elapsed_us = (uint32_t)in_w(TIMER_TIMERAWL) - pulse_started_us;
    }
    /* Coast via SIO before disabling PWM, whose output can retain its last level. */
    out_w(GPIO_OUT_SET, MOTOR_OUTPUT_MASK);
    for (UINT pin = 8U; pin <= 11U; ++pin) {
        out_w(GPIO_CTRL(pin), GPIO_CTRL_FUNCSEL_SIO);
    }
    out_w(PULSE_PWM_BASE(4U), 0U);
    out_w(PULSE_PWM_BASE(5U), 0U);
    out_w(PULSE_TIMER_CLEAR, 1U);
    out_w(TIMER_ARMED, 1U);
    out_w(TIMER_INTR, 1U);
    pulse.active = false;
}

static void pulse_inthdr(UINT intno)
{
    UINT saved = lock_pulse();
    (void)intno;
    if ((in_w(TIMER_INTS) & 1U) != 0U) {
        if (pulse.active && pulse.cutoff_count < UINT32_MAX) {
            ++pulse.cutoff_count;
        }
        stop_locked();
    }
    unlock_pulse(saved);
}

int32_t motor_pulse_init(void)
{
    const T_DINT attributes = {.intatr = TA_HLNG, .inthdr = (FP)pulse_inthdr};
    ER error;
#if TK_SUPPORT_SMP
    if (tk_get_prc() != 1) {
        return E_CTX;
    }
    error = InitSpinLock(&pulse_lock);
    if (error < E_OK) {
        return error;
    }
#endif
    pulse = (motor_pulse_status_t){0};
    if (!timer_configuration_valid()) {
        pulse.error = E_CTX;
        return E_CTX;
    }
    /* Deassert only PWM reset; leave the shared timer counter untouched. */
    out_w(RESETS_RESET + 0x3000U, RESETS_RESET_PWM);
    UINT polls;
    for (polls = 0U; polls < PULSE_INIT_POLLS; ++polls) {
        if ((in_w(RESETS_RESET_DONE) & RESETS_RESET_PWM) != 0U) {
            break;
        }
    }
    if (polls == PULSE_INIT_POLLS) {
        pulse.error = E_TMOUT;
        return E_TMOUT;
    }
    if (((in_w(TIMER_ARMED) | in_w(TIMER_INTE) | in_w(TIMER_INTF)) & 1U) != 0U ||
        ((in_w(PULSE_PWM_BASE(4U)) | in_w(PULSE_PWM_BASE(5U))) & 1U) != 0U ||
        (in_w(PWM_BASE + 0xA8U) & ((1U << 4U) | (1U << 5U))) != 0U) {
        pulse.error = E_BUSY;
        return E_BUSY;
    }
    DisableInt(PULSE_IRQ);
    error = tk_def_int(PULSE_IRQ, &attributes);
    if (error < E_OK) {
        pulse.error = error;
        return error;
    }
    drv_motor_coast_init();
    stop_locked();
    ClearInt(PULSE_IRQ);
    pulse.ready = true;
    EnableInt(PULSE_IRQ, PULSE_IRQ_PRIORITY);
    return E_OK;
}

int32_t motor_pulse_start(uint8_t wheel, uint8_t direction, uint8_t duty, uint32_t duration_ms)
{
    UINT saved = lock_pulse();
    ER error = E_OK;
    if (!pulse.ready) {
        error = E_CTX;
    } else if (pulse.active) {
        error = E_BUSY;
    } else if ((wheel < 1U) || (wheel > 2U) || (direction < 1U) || (direction > 2U) ||
               (duty == 0U) || (duty > MOTOR_PULSE_MAX_DUTY) ||
               (duration_ms == 0U) || (duration_ms > MOTOR_PULSE_MS)) {
        error = E_PAR;
    } else {
        uint32_t base = PULSE_PWM_BASE(wheel == 1U ? 4U : 5U);
        UINT pin_a = wheel == 1U ? 8U : 10U;
        UINT pin_b = pin_a + 1U;
        uint32_t variable_cc = MOTOR_PWM_PERIOD * (100U - duty) / 100U;
        uint32_t cc_a = direction == 1U ? MOTOR_PWM_PERIOD : variable_cc;
        uint32_t cc_b = direction == 1U ? variable_cc : MOTOR_PWM_PERIOD;
        stop_locked();
        out_w(base + 4U, 16U); /* Divider 1.0, not a task-driven PWM loop. */
        out_w(base + 8U, 0U);
        out_w(base + 0x10U, MOTOR_PWM_PERIOD - 1U);
        out_w(base + 0xCU, cc_a | (cc_b << 16U));
        /* Override both pins high while muxing: every transition remains coast. */
        out_w(GPIO_CTRL(pin_a), GPIO_CTRL_FUNCSEL_PWM | PULSE_OUT_HIGH);
        out_w(GPIO_CTRL(pin_b), GPIO_CTRL_FUNCSEL_PWM | PULSE_OUT_HIGH);
        pulse_started_us = (uint32_t)in_w(TIMER_TIMERAWL);
        pulse.wheel = wheel;
        pulse.direction = direction;
        pulse.duty = duty;
        pulse.elapsed_us = 0U;
        pulse.active = true;
        out_w(TIMER_INTR, 1U);
        out_w(TIMER_ALARM0, pulse_started_us + duration_ms * 1000U);
        out_w(PULSE_TIMER_SET, 1U);
        out_w(base, 1U);
        if (((uint32_t)in_w(TIMER_TIMERAWL) - pulse_started_us >= duration_ms * 1000U) ||
            ((in_w(TIMER_ARMED) & 1U) == 0U)) {
            stop_locked();
            error = E_TMOUT;
        } else {
            /* Do not release the coast overrides if setup already missed cutoff. */
#if !MOTOR_TEST_PROBE
            /* Keep the selected GPIOs explicit before changing motor muxes. */
            UINT constant_pin = direction == 1U ? pin_a : pin_b;
            UINT variable_pin = direction == 1U ? pin_b : pin_a;
            out_w(GPIO_CTRL(constant_pin), GPIO_CTRL_FUNCSEL_PWM);
            out_w(GPIO_CTRL(variable_pin), GPIO_CTRL_FUNCSEL_PWM);
#endif
            /* Probe retains both force-high overrides, including if execution stalls. */
            if ((uint32_t)in_w(TIMER_TIMERAWL) - pulse_started_us >= duration_ms * 1000U) {
                stop_locked();
                error = E_TMOUT;
            }
        }
    }
    pulse.error = error;
    unlock_pulse(saved);
    return error;
}

void motor_pulse_stop(void)
{
    UINT saved = lock_pulse();
    if (pulse.ready) {
        stop_locked();
    }
    unlock_pulse(saved);
}

void motor_pulse_read(motor_pulse_status_t *status)
{
    UINT saved = lock_pulse();
    *status = pulse;
    unlock_pulse(saved);
}
