#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "motor_test.h"

#ifdef AAI05_NO_DRIVE_BASELINE
void motor_test_init(motor_test_t *test, uint32_t now) { (void)now; memset(test, 0, sizeof(*test)); }
uint8_t motor_test_update(motor_test_t *test, uint32_t now, bool next, bool stop, bool healthy, bool active)
{ (void)test; (void)now; (void)next; (void)stop; (void)healthy; (void)active; return MOTOR_TEST_NONE; }
void motor_test_abort(motor_test_t *test, uint8_t reason) { (void)test; (void)reason; }
#endif

static void release_button(motor_test_t *test, uint32_t now)
{
    assert(motor_test_update(test, now, false, false, true, false) == MOTOR_TEST_NONE);
    assert(motor_test_update(test, now + 40U, false, false, true, false) == MOTOR_TEST_NONE);
}
static uint8_t press_button(motor_test_t *test, uint32_t now)
{
    assert(motor_test_update(test, now, true, false, true, false) == MOTOR_TEST_NONE);
    return motor_test_update(test, now + 40U, true, false, true, false);
}

static void test_explicit_press_and_held_boot(void)
{
    motor_test_t test;
    motor_test_init(&test, 0U);
    for (uint32_t now = 0U; now < 5000U; now += 20U) {
        assert(motor_test_update(&test, now, true, false, true, false) == MOTOR_TEST_NONE);
    }
    release_button(&test, 5000U);
    assert(press_button(&test, 5060U) == MOTOR_TEST_START);
    assert(test.phase == MOTOR_TEST_ACTIVE && test.step == 0U);
    for (uint32_t now = 5120U; now < 5500U; now += 20U) {
        assert(motor_test_update(&test, now, true, false, true, true) == MOTOR_TEST_NONE);
    }
    assert(motor_test_update(&test, 5500U, true, false, true, false) == MOTOR_TEST_STOP);
    assert(test.phase == MOTOR_TEST_COOLDOWN && test.step == 1U);
    assert(motor_test_update(&test, 6500U, true, false, true, false) == MOTOR_TEST_NONE);
    assert(test.phase == MOTOR_TEST_IDLE);
    assert(motor_test_update(&test, 6600U, true, false, true, false) == MOTOR_TEST_NONE);
}

static void test_bounce_stop_and_faults(void)
{
    motor_test_t test;
    motor_test_init(&test, 0U);
    release_button(&test, 0U);
    assert(motor_test_update(&test, 60U, true, false, true, false) == MOTOR_TEST_NONE);
    assert(motor_test_update(&test, 70U, false, false, true, false) == MOTOR_TEST_NONE);
    assert(motor_test_update(&test, 80U, true, false, true, false) == MOTOR_TEST_NONE);
    assert(motor_test_update(&test, 100U, true, true, true, false) == MOTOR_TEST_STOP);
    assert(test.phase == MOTOR_TEST_STOPPED && test.stop_reason == MOTOR_STOP_BUTTON);
    assert(motor_test_update(&test, 1000U, true, false, true, false) == MOTOR_TEST_NONE);
    motor_test_init(&test, 0U);
    assert(motor_test_update(&test, 0U, false, false, false, false) == MOTOR_TEST_STOP);
    assert(test.stop_reason == MOTOR_STOP_INPUT_FAULT);
    assert(motor_test_update(&test, 1000U, true, false, true, false) == MOTOR_TEST_NONE);
}

static void test_four_steps_and_timer_completion(void)
{
    motor_test_t test;
    uint32_t now = 0U;
    motor_test_init(&test, now);
    for (unsigned int step = 0U; step < 4U; ++step) {
        release_button(&test, now);
        assert(press_button(&test, now + 60U) == MOTOR_TEST_START);
        assert(test.step == step);
        /* Driver timer can stop independently before this task runs again. */
        assert(motor_test_update(&test, now + 500U, true, false, true, false) == MOTOR_TEST_STOP);
        assert(test.step == step + 1U);
        assert(motor_test_update(&test, now + 1500U, true, false, true, false) == MOTOR_TEST_NONE);
        now += 1600U;
    }
    assert(test.phase == MOTOR_TEST_DONE);
    release_button(&test, now);
    assert(press_button(&test, now + 60U) == MOTOR_TEST_NONE);
}

static void test_wrap_and_active_fault(void)
{
    motor_test_t test;
    uint32_t base = UINT32_MAX - 200U;
    motor_test_init(&test, base);
    release_button(&test, base);
    assert(press_button(&test, base + 60U) == MOTOR_TEST_START);
    assert(motor_test_update(&test, base + 499U, true, false, true, true) == MOTOR_TEST_NONE);
    assert(motor_test_update(&test, base + 500U, true, false, true, false) == MOTOR_TEST_STOP);
    motor_test_init(&test, 0U);
    release_button(&test, 0U);
    assert(press_button(&test, 60U) == MOTOR_TEST_START);
    assert(motor_test_update(&test, 120U, true, false, false, true) == MOTOR_TEST_STOP);
    assert(test.phase == MOTOR_TEST_STOPPED);
    motor_test_abort(&test, MOTOR_STOP_DRIVER);
    assert(test.stop_reason == MOTOR_STOP_DRIVER);
}

static void test_fallback_does_not_preempt_hardware_deadline(void)
{
    motor_test_t test;
    motor_test_init(&test, 0U);
    release_button(&test, 0U);
    assert(press_button(&test, 60U) == MOTOR_TEST_START); /* Starts at t=100. */
    assert(motor_test_update(&test, 500U, true, false, true, true) == MOTOR_TEST_NONE);
    assert(motor_test_update(&test, 599U, true, false, true, true) == MOTOR_TEST_NONE);
    assert(motor_test_update(&test, 600U, true, false, true, true) == MOTOR_TEST_STOP);
    assert(test.phase == MOTOR_TEST_STOPPED && test.stop_reason == MOTOR_STOP_DRIVER);
}

/* Catches a count-quality warning incorrectly aborting open-loop motion. */
static void test_capture_warning_allows_bounded_pulse(void)
{
    encoder_capture_t capture = {0};
    motor_pulse_status_t motor = {.ready = true};
    motor_test_t test;
    bool is_ready;

    motor_test_init(&test, 0U);
    release_button(&test, 0U);
    assert(press_button(&test, 60U) == MOTOR_TEST_START);
    capture.left.count = 1856;
    capture.irq_count = 1857U;
    capture.left_overrun = 1U;
    is_ready = motor_test_inputs_ready(&capture, &motor, true, 0, 0);
    assert(motor_test_update(&test, 260U, false, false, is_ready, true) ==
           MOTOR_TEST_NONE);
    assert(test.phase == MOTOR_TEST_ACTIVE);
    capture.right.invalid_transitions = 1U;
    capture.left.invalid_transitions = 1U;
    capture.right_overrun = 1U;
    is_ready = motor_test_inputs_ready(&capture, &motor, true, 0, 0);
    assert(is_ready);
    assert(motor_test_update(&test, 500U, false, false, is_ready, false) ==
           MOTOR_TEST_STOP);
    assert(test.phase == MOTOR_TEST_COOLDOWN);
    assert(test.stop_reason == MOTOR_STOP_NONE);
    assert(motor_test_update(&test, 1500U, false, false, is_ready, false) ==
           MOTOR_TEST_NONE);
    assert(test.phase == MOTOR_TEST_IDLE && test.step == 1U);
    assert(press_button(&test, 1560U) == MOTOR_TEST_START);
    assert(motor_test_update(&test, 1620U, false, true, is_ready, true) ==
           MOTOR_TEST_STOP);
    assert(test.phase == MOTOR_TEST_STOPPED);
    assert(test.stop_reason == MOTOR_STOP_BUTTON);
    release_button(&test, 1660U);
    assert(press_button(&test, 1720U) == MOTOR_TEST_NONE);
}

/* Catches warning tolerance accidentally disabling actual fault guards. */
static void test_diagnostic_readiness_preserves_faults(void)
{
    encoder_capture_t capture = {0};
    motor_pulse_status_t motor = {.ready = true};

    assert(motor_test_inputs_ready(&capture, &motor, true, 0, 0));
    capture.error = -50;
    assert(!motor_test_inputs_ready(&capture, &motor, true, 0, 0));
    capture.error = 0;
    assert(!motor_test_inputs_ready(NULL, &motor, true, 0, 0));
    assert(!motor_test_inputs_ready(&capture, NULL, true, 0, 0));
    assert(!motor_test_inputs_ready(&capture, &motor, false, 0, 0));
    assert(!motor_test_inputs_ready(&capture, &motor, true, -5, 0));
    assert(!motor_test_inputs_ready(&capture, &motor, true, 0, -5));
    motor.ready = false;
    assert(!motor_test_inputs_ready(&capture, &motor, true, 0, 0));
    motor.ready = true;
    motor.error = -5;
    assert(!motor_test_inputs_ready(&capture, &motor, true, 0, 0));
    motor.error = 0;
    capture.left.count_saturated = true;
    assert(!motor_test_inputs_ready(&capture, &motor, true, 0, 0));
    capture.left.count_saturated = false;
    capture.right.count_saturated = true;
    assert(!motor_test_inputs_ready(&capture, &motor, true, 0, 0));
}

int main(void)
{
    test_explicit_press_and_held_boot();
    test_bounce_stop_and_faults();
    test_four_steps_and_timer_completion();
    test_wrap_and_active_fault();
    test_fallback_does_not_preempt_hardware_deadline();
    test_capture_warning_allows_bounded_pulse();
    test_diagnostic_readiness_preserves_faults();
    puts("PASS: motor controller explicit-start, debounce/hold, STOP/fault, cooldown, sequence and wrap");
    return 0;
}
