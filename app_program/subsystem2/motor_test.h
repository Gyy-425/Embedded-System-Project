#ifndef AAI05_MOTOR_TEST_H
#define AAI05_MOTOR_TEST_H
#include "motor_pulse.h"

enum {MOTOR_TEST_IDLE, MOTOR_TEST_ACTIVE, MOTOR_TEST_COOLDOWN, MOTOR_TEST_DONE, MOTOR_TEST_STOPPED};
enum {MOTOR_TEST_NONE, MOTOR_TEST_START, MOTOR_TEST_STOP};
enum {MOTOR_STOP_NONE, MOTOR_STOP_BUTTON, MOTOR_STOP_INPUT_FAULT, MOTOR_STOP_DRIVER};
#define MOTOR_NEXT_DEBOUNCE_MS 40U
#define MOTOR_COOLDOWN_MS 1000U
#define MOTOR_SOFT_TIMEOUT_MS (MOTOR_PULSE_MS + 100U)

typedef struct {
    uint32_t button_changed_ms;
    uint32_t phase_started_ms;
    uint8_t phase;
    uint8_t step;
    uint8_t stop_reason;
    bool raw_next;
    bool stable_next;
    bool released;
} motor_test_t;

void motor_test_init(motor_test_t *test, uint32_t now);
uint8_t motor_test_update(motor_test_t *test, uint32_t now, bool next,
                          bool stop, bool healthy, bool pulse_active);
void motor_test_abort(motor_test_t *test, uint8_t reason);
#endif
