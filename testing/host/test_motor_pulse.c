/* Run the actual PWM/timer driver against register/kernel boundary fakes. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "motor_pulse.h"
#include "motor_io.h"
#define AAI05_APP_H
#define TK_SUPPORT_SMP 0
/* Skip machine/config selection only; use the REAL device register macros.
   A safer hand-written GPIO_CTRL fake masked a target precedence fault. */
#define __SYS_MACHINE_H__
#include "../../include/sys/sysdepend/cpu/rp2040/sysdef.h"
#define CLK_PLL_SYS_FREQ 125000000U /* Fake clock configuration, not register mapping. */
#define TEST_WATCHDOG_TICK UINT32_C(0x4005802C)
#define TEST_REF_CTRL UINT32_C(0x40008030)
#define TEST_REF_DIV UINT32_C(0x40008034)
#define E_OK 0
#define E_PAR (-17)
#define E_BUSY (-65)
#define E_TMOUT (-50)
#define E_CTX (-25)
#define TA_HLNG 1U
typedef unsigned int UINT;
typedef int ER;
typedef void (*FP)(void);
typedef struct {unsigned int intatr; FP inthdr;} T_DINT;

static uint32_t addresses[256], values[256];
static unsigned int register_count;
static uint32_t now_us;
static uint32_t latch;
static unsigned int expected_wheel, expected_direction;
static int definition_error;
static bool expire_on_enable;
static bool forbid_drive;
static bool pwm_reset_stuck;
static bool timer_advances_on_read;
static bool is_pair_test;
static int8_t expected_pair[2];
static int8_t previous_pair[2];
static bool pair_coasted[2];
static uint32_t gpio_write_count;
static bool expire_before_renew_clear;
static void (*alarm_handler)(UINT);

static uint32_t *reg(uint32_t address)
{
    for (unsigned int i = 0U; i < register_count; ++i) {
        if (addresses[i] == address) return &values[i];
    }
    assert(register_count < 256U);
    addresses[register_count] = address;
    return &values[register_count++];
}
static unsigned int pin_level(unsigned int pin)
{
    uint32_t ctrl = *reg(GPIO_CTRL(pin));
    if (((ctrl >> 8U) & 3U) == 3U) return 1U;
    if ((ctrl & 31U) == GPIO_CTRL_FUNCSEL_SIO) return (latch >> pin) & 1U;
    assert((ctrl & 31U) == GPIO_CTRL_FUNCSEL_PWM);
    if ((*reg(RESETS_RESET) & RESETS_RESET_PWM) != 0U) return 0U;
    uint32_t base = PWM_BASE + (pin / 2U) * 0x14U;
    uint32_t cc = *reg(base + 0xCU);
    uint32_t threshold = (pin & 1U) != 0U ? cc >> 16U : cc & 0xFFFFU;
    return *reg(base + 8U) < threshold;
}
static void assert_output_policy(void)
{
    for (unsigned int wheel = 1U; wheel <= 2U; ++wheel) {
        unsigned int pin = wheel == 1U ? 8U : 10U;
        unsigned int pair = pin_level(pin) | (pin_level(pin + 1U) << 1U);
        if (is_pair_test && !MOTOR_TEST_PROBE && !forbid_drive) {
            unsigned int index = wheel - 1U;
            int8_t requested = expected_pair[index];
            int8_t previous = previous_pair[index];
            if (pair == 3U) { pair_coasted[index] = true; continue; }
            if (!pair_coasted[index] && previous != 0) {
                assert(pair == 0U || pair == (previous > 0 ? 1U : 2U));
            } else {
                assert(requested != 0);
                assert(pair == 0U || pair == (requested > 0 ? 1U : 2U));
            }
            continue;
        }
        if (MOTOR_TEST_PROBE || forbid_drive || wheel != expected_wheel) {
            assert(pair == 3U);
        } else {
            unsigned int drive = expected_direction == 1U ? 1U : 2U;
            /* Requested polarity, brake or coast; never reverse transiently. */
            assert(pair == 0U || pair == 3U || pair == drive);
        }
    }
    assert(*reg(GPIO_CTRL(26U)) == 31U && *reg(GPIO_CTRL(27U)) == 31U);
}
static uint32_t read_register(uint32_t address)
{
    assert((address & 3U) == 0U);
    if (address == TIMER_TIMERAWL) {
        if (timer_advances_on_read && (*reg(TEST_WATCHDOG_TICK) & 0x3FFU) == 0x20CU &&
            (*reg(TIMER_PAUSE) & 1U) == 0U && (*reg(RESETS_RESET) & RESETS_RESET_TIMER) == 0U) ++now_us;
        return now_us;
    }
    if (address >= PWM_BASE && address < PWM_BASE + 0xB4U && (*reg(RESETS_RESET) & RESETS_RESET_PWM) != 0U) return 0U;
    if (address == TIMER_INTS) return *reg(TIMER_INTR) & *reg(TIMER_INTE);
    return *reg(address);
}
static void write_register(uint32_t address, uint32_t value)
{
    assert((address & 3U) == 0U);
    if (expire_before_renew_clear && address == TIMER_INTR) {
        now_us += 20U;
        *reg(TIMER_ARMED) &= ~1U;
        *reg(TIMER_INTR) |= 1U;
        expire_before_renew_clear = false;
    }
    if (address >= IO_BANK0_BASE && address < IO_BANK0_BASE + 0xF0U) {
        ++gpio_write_count;
        bool owned = false;
        for (unsigned int pin = 8U; pin <= 11U; ++pin) {
            if (address == (uint32_t)GPIO_CTRL(pin)) owned = true;
        }
        assert(owned); /* No GPIO1 write from a malformed conditional macro. */
    }
    assert(address != TIMER_BASE && address != TIMER_BASE + 4U && address != TIMER_TIMERAWL);
    if (address == RESETS_RESET + 0x3000U) {
        *reg(RESETS_RESET) &= ~value;
        if (!pwm_reset_stuck) *reg(RESETS_RESET_DONE) |= value;
    }
    else if (address >= PWM_BASE && address < PWM_BASE + 0xB4U && (*reg(RESETS_RESET) & RESETS_RESET_PWM) != 0U) { }
    else if (address == GPIO_OUT_SET) latch |= value;
    else if (address == TIMER_INTR) *reg(address) &= ~value;
    else if (address == TIMER_ARMED) *reg(address) &= ~value;
    else if (address == TIMER_INTE + 0x2000U) *reg(TIMER_INTE) |= value;
    else if (address == TIMER_INTE + 0x3000U) *reg(TIMER_INTE) &= ~value;
    else {
        *reg(address) = value;
        if (address == TIMER_ALARM0) *reg(TIMER_ARMED) |= 1U;
    }
    if (expire_on_enable && (address == PWM_BASE + 4U * 0x14U || address == PWM_BASE + 5U * 0x14U) && (value & 1U) != 0U) {
        now_us += MOTOR_PULSE_US + 1U;
        *reg(TIMER_ARMED) &= ~1U;
        *reg(TIMER_INTR) |= 1U;
        *reg(address + 8U) = 11000U;
        forbid_drive = true;
        expire_on_enable = false;
    }
    assert_output_policy();
}
#define in_w(address) read_register(address)
#define out_w(address,value) write_register((address),(value))
#define DI(saved) ((saved) = 17U)
#define EI(saved) ((void)(saved))
static void DisableInt(UINT irq) { assert(irq == 0U); }
static void ClearInt(UINT irq) { assert(irq == 0U); }
static void EnableInt(UINT irq, int priority) { assert(irq == 0U && priority == 1); }
static ER tk_def_int(UINT irq, const T_DINT *attributes)
{
    assert(irq == 0U && attributes->intatr == TA_HLNG);
    alarm_handler = (void (*)(UINT))attributes->inthdr;
    return definition_error;
}
void drv_motor_coast_init(void)
{
    latch = MOTOR_OUTPUT_MASK;
    for (unsigned int pin = 8U; pin <= 11U; ++pin) *reg(GPIO_CTRL(pin)) = GPIO_CTRL_FUNCSEL_SIO;
}

#ifdef AAI05_NO_DRIVE_BASELINE
int32_t motor_timebase_init(void) { return -9; }
int32_t motor_pulse_init(void) { return -9; }
int32_t motor_pulse_start(uint8_t w, uint8_t d, uint8_t p, uint32_t ms)
{ (void)w; (void)d; (void)p; (void)ms; return -9; }
void motor_pulse_stop(void) { }
void motor_pulse_read(motor_pulse_status_t *status) { memset(status, 0, sizeof(*status)); }
/* Reference boundary functions so strict compilation checks the entire fixture. */
static void baseline_fixture(void)
{
    T_DINT attrs = {.intatr = TA_HLNG, .inthdr = NULL};
    (void)tk_def_int(0U, &attrs); DisableInt(0U); ClearInt(0U); EnableInt(0U, 1);
    (void)read_register(TIMER_TIMERAWL); write_register(GPIO_OUT_SET, MOTOR_OUTPUT_MASK);
}
#else
#include "../../app_program/subsystem2/motor_pulse.c"
#endif

static void reset_hardware(void)
{
    memset(addresses, 0, sizeof(addresses)); memset(values, 0, sizeof(values));
    register_count = 0U; now_us = 1000U; expected_wheel = expected_direction = 0U;
    definition_error = 0; alarm_handler = NULL; expire_on_enable = false; forbid_drive = false; pwm_reset_stuck = false; timer_advances_on_read = false;
    is_pair_test = false;
    memset(expected_pair, 0, sizeof(expected_pair));
    memset(previous_pair, 0, sizeof(previous_pair));
    memset(pair_coasted, 0, sizeof(pair_coasted));
    gpio_write_count = 0U;
    expire_before_renew_clear = false;
    *reg(TEST_WATCHDOG_TICK) = 0x20CU;
    *reg(TEST_REF_CTRL) = 2U;
    *reg(TEST_REF_DIV) = 0x100U;
    *reg(RESETS_RESET_DONE) = RESETS_RESET_PWM | RESETS_RESET_TIMER;
    *reg(GPIO_CTRL(26U)) = *reg(GPIO_CTRL(27U)) = 31U;
    drv_motor_coast_init();
}

static void test_cold_pwm_reset(void)
{
    reset_hardware();
    *reg(RESETS_RESET) = RESETS_RESET_PWM | 0x100U;
    *reg(RESETS_RESET_DONE) &= ~RESETS_RESET_PWM;
    assert(motor_pulse_init() == E_OK);
    assert((*reg(RESETS_RESET) & RESETS_RESET_PWM) == 0U);
    assert((*reg(RESETS_RESET_DONE) & RESETS_RESET_PWM) != 0U);
    assert((*reg(RESETS_RESET) & 0x100U) != 0U);
    assert_output_policy();
    reset_hardware();
    *reg(RESETS_RESET) = RESETS_RESET_PWM;
    *reg(RESETS_RESET_DONE) &= ~RESETS_RESET_PWM;
    pwm_reset_stuck = true;
    assert(motor_pulse_init() == E_TMOUT);
}

static void test_unprepared_timer_rejected(void)
{
    reset_hardware();
    *reg(TEST_WATCHDOG_TICK) = 0x200U; /* Cold reset: enabled, divider unconfigured. */
    assert(motor_pulse_init() == E_CTX);
    reset_hardware();
    *reg(TIMER_PAUSE) = 1U;
    assert(motor_pulse_init() == E_CTX);
    reset_hardware();
    *reg(RESETS_RESET) = RESETS_RESET_TIMER;
    assert(motor_pulse_init() == E_CTX);
    reset_hardware();
    *reg(TEST_REF_DIV) = 0x200U;
    assert(motor_pulse_init() == E_CTX);
}

static void test_early_timebase_setup_without_counter_reset(void)
{
    reset_hardware();
    *reg(TEST_WATCHDOG_TICK) = 0x200U;
    now_us = 7654321U;
    timer_advances_on_read = true;
    assert(motor_timebase_init() == E_OK);
    assert((*reg(TEST_WATCHDOG_TICK) & 0x3FFU) == 0x20CU && now_us > 7654321U);
    now_us = 1234567U;
    assert(motor_timebase_init() == E_OK && now_us > 1234567U);
    timer_advances_on_read = false;
    assert(motor_timebase_init() == E_TMOUT);
    *reg(TEST_WATCHDOG_TICK) = 0x218U;
    assert(motor_timebase_init() == E_BUSY);
    assert(*reg(TEST_WATCHDOG_TICK) == 0x218U);
    *reg(TEST_WATCHDOG_TICK) = 0x200U;
    *reg(UINT32_C(0x40058000)) = UINT32_C(1) << 30U;
    assert(motor_timebase_init() == E_BUSY);
}

static void test_polarity_and_independent_cutoff(void)
{
    motor_pulse_status_t status;
    reset_hardware();
    assert(motor_pulse_init() == E_OK);
    for (uint8_t wheel = 1U; wheel <= 2U; ++wheel) {
        for (uint8_t direction = 1U; direction <= 2U; ++direction) {
            expected_wheel = wheel; expected_direction = direction;
            assert(motor_pulse_start(wheel, direction, 50U, 400U) == E_OK);
            motor_pulse_read(&status);
            assert(status.active && status.ready);
            assert(status.duty == 50U);
            uint32_t slice = wheel == 1U ? 4U : 5U;
            /* Manufacturer pattern: first-half drive, second-half 00 brake. */
            if (!MOTOR_TEST_PROBE) {
                unsigned int drive_samples = 0U;
                unsigned int base_pin = wheel == 1U ? 8U : 10U;
                for (uint32_t counter = 0U; counter < 12500U; counter += 125U) {
                    *reg(PWM_BASE + slice * 0x14U + 8U) = counter;
                    assert_output_policy();
                    unsigned int pair = pin_level(base_pin) |
                                        (pin_level(base_pin + 1U) << 1U);
                    unsigned int drive = direction == 1U ? 1U : 2U;
                    assert(pair == (counter < 6250U ? drive : 0U));
                    if (pin_level(base_pin) != pin_level(base_pin + 1U)) ++drive_samples;
                }
                assert(drive_samples == 50U);
            }
            *reg(PWM_BASE + slice * 0x14U + 8U) = 11000U;
            assert_output_policy();
            if (MOTOR_TEST_PROBE) {
                assert(pin_level(wheel == 1U ? 8U : 10U) == 1U && pin_level(wheel == 1U ? 9U : 11U) == 1U);
            } else {
                assert(pin_level(wheel == 1U ? 8U : 10U) == 0U &&
                       pin_level(wheel == 1U ? 9U : 11U) == 0U);
            }
            /* No application task runs while the hardware deadline expires. */
            now_us += MOTOR_PULSE_US + 1U;
            *reg(TIMER_ARMED) &= ~1U; *reg(TIMER_INTR) |= 1U;
            assert(alarm_handler != NULL); alarm_handler(0U);
            expected_wheel = 0U; assert_output_policy();
            motor_pulse_read(&status);
            assert(!status.active && status.elapsed_us == MOTOR_PULSE_US + 1U);
            assert(status.cutoff_count == (uint32_t)((wheel - 1U) * 2U + direction));
            uint32_t duration = status.elapsed_us;
            motor_pulse_stop(); motor_pulse_read(&status);
            assert(status.elapsed_us == duration);
        }
    }
}

static void test_explicit_stop_in_both_phases(void)
{
    for (uint8_t wheel = 1U; wheel <= 2U; ++wheel) {
        for (uint8_t direction = 1U; direction <= 2U; ++direction) {
            for (uint32_t phase = 0U; phase <= 1U; ++phase) {
                reset_hardware();
                assert(motor_pulse_init() == E_OK);
                expected_wheel = wheel;
                expected_direction = direction;
                assert(motor_pulse_start(wheel, direction, 50U, 400U) == E_OK);
                uint32_t slice = wheel == 1U ? 4U : 5U;
                *reg(PWM_BASE + slice * 0x14U + 8U) = phase * 11000U;
                assert_output_policy();
                motor_pulse_stop();
                expected_wheel = 0U;
                assert_output_policy();
            }
        }
    }
}

static void test_limits_conflicts_failure_and_wrap(void)
{
    motor_pulse_status_t status;
    reset_hardware(); assert(motor_pulse_init() == E_OK);
    assert(motor_pulse_start(0U, 1U, 25U, 400U) == E_PAR);
    assert(motor_pulse_start(1U, 3U, 25U, 400U) == E_PAR);
    assert(motor_pulse_start(1U, 1U, 51U, 400U) == E_PAR);
    assert(motor_pulse_start(1U, 1U, 25U, 401U) == E_PAR);
    expected_wheel = expected_direction = 1U;
    now_us = UINT32_MAX - 200000U;
    assert(motor_pulse_start(1U, 1U, 25U, 400U) == E_OK);
    assert(motor_pulse_start(2U, 1U, 25U, 400U) == E_BUSY);
    now_us += MOTOR_PULSE_US; *reg(TIMER_ARMED) &= ~1U; *reg(TIMER_INTR) |= 1U; alarm_handler(0U);
    expected_wheel = 0U; motor_pulse_read(&status);
    assert(!status.active && status.elapsed_us == MOTOR_PULSE_US);
    reset_hardware(); *reg(TIMER_ARMED) = 1U; assert(motor_pulse_init() == E_BUSY);
    reset_hardware(); *reg(PWM_BASE + 4U * 0x14U) = 1U; assert(motor_pulse_init() == E_BUSY);
    reset_hardware(); definition_error = -5; assert(motor_pulse_init() == -5);
    motor_pulse_read(&status); assert(!status.ready && !status.active);
    reset_hardware(); assert(motor_pulse_init() == E_OK);
    expected_wheel = expected_direction = 1U; expire_on_enable = true;
    assert(motor_pulse_start(1U, 1U, 25U, 400U) == E_TMOUT);
    expected_wheel = 0U; motor_pulse_read(&status); assert(!status.active);
}

static void expect_pair(int8_t left_pct, int8_t right_pct)
{
    is_pair_test = true;
    memcpy(previous_pair, expected_pair, sizeof(previous_pair));
    expected_pair[0] = left_pct;
    expected_pair[1] = right_pct;
    memset(pair_coasted, 0, sizeof(pair_coasted));
}

static void test_paired_drive_and_lease(void)
{
    const int8_t duties[] = {-50, 0, 50};
    motor_pulse_status_t status;

    for (unsigned int left = 0U; left < 3U; ++left) {
        for (unsigned int right = 0U; right < 3U; ++right) {
            reset_hardware();
            assert(motor_pulse_init() == E_OK);
            expect_pair(duties[left], duties[right]);
            assert(motor_drive_set(duties[left], duties[right], 100U) == E_OK);
            motor_pulse_read(&status);
            assert(status.active == (duties[left] != 0 || duties[right] != 0));
            if (!status.active) continue;
            uint32_t writes = gpio_write_count;
            now_us += 20000U;
            assert(motor_drive_set(duties[left], duties[right], 100U) == E_OK);
            assert(gpio_write_count == writes);
            assert(*reg(TIMER_ALARM0) == now_us + 100000U);
            for (unsigned int wheel = 0U; wheel < 2U; ++wheel) {
                unsigned int pin = 8U + wheel * 2U;
                unsigned int pair = pin_level(pin) | (pin_level(pin + 1U) << 1U);
                assert(pair == (MOTOR_TEST_PROBE || expected_pair[wheel] == 0 ?
                                3U : expected_pair[wheel] > 0 ? 1U : 2U));
            }
            *reg(PWM_BASE + 4U * 0x14U + 8U) = 11000U;
            *reg(PWM_BASE + 5U * 0x14U + 8U) = 11000U;
            assert_output_policy();
            now_us += 100000U;
            *reg(TIMER_ARMED) &= ~1U;
            *reg(TIMER_INTR) |= 1U;
            alarm_handler(0U);
            motor_pulse_read(&status);
            assert(!status.active && status.cutoff_count == 1U);
            assert(pin_level(8U) && pin_level(9U));
            assert(pin_level(10U) && pin_level(11U));
        }
    }

    reset_hardware();
    assert(motor_pulse_init() == E_OK);
    expect_pair(50, -50);
    assert(motor_drive_set(50, -50, 100U) == E_OK);
    expect_pair(-35, 40);
    assert(motor_drive_set(-35, 40, 80U) == E_OK);
    assert(*reg(PWM_BASE + 4U * 0x14U + 0xCU) == (4375U << 16U));
    assert(*reg(PWM_BASE + 5U * 0x14U + 0xCU) == 5000U);
    motor_pulse_stop();
    assert(pin_level(8U) && pin_level(9U) && pin_level(10U) && pin_level(11U));

    reset_hardware();
    assert(motor_pulse_init() == E_OK);
    expect_pair(50, -50);
    now_us = UINT32_MAX - 50000U;
    assert(motor_drive_set(50, -50, 100U) == E_OK);
    now_us += 100000U;
    assert(motor_drive_set(50, -50, 100U) == E_TMOUT);
    motor_pulse_read(&status);
    assert(!status.active); /* Expired lease cannot be renewed before IRQ runs. */

    reset_hardware();
    assert(motor_pulse_init() == E_OK);
    expect_pair(50, 50);
    expire_on_enable = true;
    assert(motor_drive_set(50, 50, 100U) == E_TMOUT);
    motor_pulse_read(&status);
    assert(!status.active);
    reset_hardware();
    assert(motor_pulse_init() == E_OK);
    assert(motor_drive_set(INT8_MIN, 0, 100U) == E_PAR);
    assert(motor_drive_set(0, 51, 100U) == E_PAR);
    assert(motor_drive_set(0, 0, 0U) == E_PAR);
    assert(motor_drive_set(0, 0, 101U) == E_PAR);
}

static void test_pair_expiry_cannot_restart(void)
{
    motor_pulse_status_t status;

    reset_hardware();
    assert(motor_pulse_init() == E_OK);
    expect_pair(50, -50);
    assert(motor_drive_set(50, -50, 100U) == E_OK);
    now_us += 100000U;
    *reg(TIMER_ARMED) &= ~1U;
    *reg(TIMER_INTR) |= 1U;
    alarm_handler(0U);
    /* Caller may still hold an older active=true snapshot from before IRQ. */
    assert(motor_drive_set(50, -50, 100U) == E_TMOUT);
    motor_pulse_read(&status);
    assert(!status.active && status.cutoff_count == 1U);
}

static void test_pair_expiry_during_renewal(void)
{
    motor_pulse_status_t status;

    reset_hardware();
    assert(motor_pulse_init() == E_OK);
    expect_pair(50, -50);
    assert(motor_drive_set(50, -50, 100U) == E_OK);
    now_us += 99990U;
    expire_before_renew_clear = true;
    assert(motor_drive_set(50, -50, 100U) == E_TMOUT);
    motor_pulse_read(&status);
    assert(!status.active);
    assert(motor_drive_set(50, -50, 100U) == E_TMOUT);
}

int main(int argc, char **argv)
{
    reset_hardware();
    if (argc == 2) {
        if (strcmp(argv[1], "drive-after-irq") == 0) test_pair_expiry_cannot_restart();
        else if (strcmp(argv[1], "renewal-race") == 0) test_pair_expiry_during_renewal();
        else if (strcmp(argv[1], "reset") == 0) test_cold_pwm_reset();
        else test_unprepared_timer_rejected();
        return 0;
    }
#ifdef AAI05_NO_DRIVE_BASELINE
    baseline_fixture();
#endif
    test_polarity_and_independent_cutoff();
    test_explicit_stop_in_both_phases();
    test_limits_conflicts_failure_and_wrap();
    test_cold_pwm_reset();
    test_unprepared_timer_rejected();
    test_early_timebase_setup_without_counter_reset();
    test_paired_drive_and_lease();
    test_pair_expiry_cannot_restart();
    test_pair_expiry_during_renewal();
    puts(MOTOR_TEST_PROBE ?
         "PASS: PROBE always coast for pulse/pair, lease, caps/conflicts and wrap" :
         "PASS: pulse/pair PWM, ordered coast, lease renewal/cutoff and wrap");
    return 0;
}
