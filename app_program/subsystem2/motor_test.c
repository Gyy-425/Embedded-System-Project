#include "motor_test.h"
#include <stddef.h>

bool motor_test_inputs_ready(const encoder_capture_t *p_capture,
                             const motor_pulse_status_t *p_motor,
                             bool is_capture_ready, int32_t buttons_error,
                             int32_t motor_init_error)
{
    if ((p_capture == NULL) || (p_motor == NULL))
    {
        return false;
    }
    /* This bounded open-loop test never steers from encoder counts.
     * Keep capture-quality warnings observable, but do not latch STOP on
     * them. A feedback controller needs a separate quality policy. */
    return is_capture_ready && (p_capture->error == 0) &&
           p_motor->ready && (p_motor->error == 0) &&
           (buttons_error == 0) && (motor_init_error == 0) &&
           !p_capture->left.count_saturated &&
           !p_capture->right.count_saturated;
}

void motor_test_init(motor_test_t *test, uint32_t now)
{
    *test = (motor_test_t){0};
    test->button_changed_ms = now;
    /* Treat boot as held until a stable release is observed. */
    test->raw_next = true;
    test->stable_next = true;
}

void motor_test_abort(motor_test_t *test, uint8_t reason)
{
    test->phase = MOTOR_TEST_STOPPED;
    test->stop_reason = reason;
}

uint8_t motor_test_update(motor_test_t *test, uint32_t now, bool next,
                          bool stop, bool healthy, bool pulse_active)
{
    bool pressed = false;
    if (test->phase == MOTOR_TEST_STOPPED)
    {
        return MOTOR_TEST_NONE;
    }
    if (stop || !healthy)
    {
        motor_test_abort(test, stop ? MOTOR_STOP_BUTTON : MOTOR_STOP_INPUT_FAULT);
        return MOTOR_TEST_STOP;
    }
    if (next != test->raw_next)
    {
        test->raw_next = next;
        test->button_changed_ms = now;
    }
    if ((uint32_t)(now - test->button_changed_ms) >= MOTOR_NEXT_DEBOUNCE_MS)
    {
        if (!test->raw_next)
        {
            test->stable_next = false;
            test->released = true;
        }
        else if (!test->stable_next)
        {
            test->stable_next = true;
            pressed = test->released;
            test->released = false;
        }
    }
    if (test->phase == MOTOR_TEST_ACTIVE)
    {
        if (pulse_active && (uint32_t)(now - test->phase_started_ms) >= MOTOR_SOFT_TIMEOUT_MS)
        {
            motor_test_abort(test, MOTOR_STOP_DRIVER);
            return MOTOR_TEST_STOP;
        }
        if (!pulse_active)
        {
            test->phase = MOTOR_TEST_COOLDOWN;
            test->phase_started_ms = now;
            ++test->step;
            return MOTOR_TEST_STOP;
        }
    }
    else if (test->phase == MOTOR_TEST_COOLDOWN)
    {
        if ((uint32_t)(now - test->phase_started_ms) >= MOTOR_COOLDOWN_MS)
        {
            test->phase = (test->step == 4U) ? MOTOR_TEST_DONE : MOTOR_TEST_IDLE;
        }
    }
    else if ((test->phase == MOTOR_TEST_IDLE) && pressed && (test->step < 4U))
    {
        test->phase = MOTOR_TEST_ACTIVE;
        test->phase_started_ms = now;
        return MOTOR_TEST_START;
    }
    return MOTOR_TEST_NONE;
}
