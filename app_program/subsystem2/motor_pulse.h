#ifndef AAI05_MOTOR_PULSE_H
#define AAI05_MOTOR_PULSE_H
#include <stdint.h>
#include <stdbool.h>

/* Diagnostic profile: exercise PWM/alarm setup while forcing both inputs high. */
#ifndef MOTOR_TEST_PROBE
#define MOTOR_TEST_PROBE 0
#endif
#if (MOTOR_TEST_PROBE != 0) && (MOTOR_TEST_PROBE != 1)
#error MOTOR_TEST_PROBE must be 0 or 1
#endif

#define MOTOR_PULSE_MAX_DUTY 50U
#define MOTOR_PULSE_MS 400U
#define MOTOR_PULSE_US (MOTOR_PULSE_MS * 1000U)
#define MOTOR_PWM_PERIOD 12500U
#define MOTOR_NEXT_PIN 20U
#define MOTOR_STOP_PIN 21U

typedef struct {
    uint32_t cutoff_count;
    uint32_t elapsed_us;
    int32_t error;
    uint8_t wheel;
    uint8_t direction;
    uint8_t duty;
    bool active;
    bool ready;
} motor_pulse_status_t;

/* Startup-only, before the priority-1 initial task yields to USB/radio tasks. */
int32_t motor_timebase_init(void);
int32_t motor_pulse_init(void);
int32_t motor_pulse_start(uint8_t wheel, uint8_t direction, uint8_t duty, uint32_t duration_ms);
void motor_pulse_stop(void);
void motor_pulse_read(motor_pulse_status_t *status);
#endif
