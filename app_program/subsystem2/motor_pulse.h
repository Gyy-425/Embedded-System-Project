#ifndef AAI05_MOTOR_PULSE_H
#define AAI05_MOTOR_PULSE_H
#include <stdint.h>
#include <stdbool.h>

/* Diagnostic profile: exercise PWM/alarm setup with both inputs held high. */
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
#define MOTOR_DRIVE_MAX_LEASE_MS 100U

typedef struct
{
    uint32_t cutoff_count;
    uint32_t elapsed_us;
    int32_t error;
    uint8_t wheel;
    uint8_t direction;
    uint8_t duty;
    bool active;
    bool ready;
} motor_pulse_status_t;

/* Startup-only, before the initial task yields to USB/radio tasks. */
int32_t motor_timebase_init(void);
int32_t motor_pulse_init(void);
/* Running: direct PWM on selected input, other LOW; STOP/boot:11 coast. */
int32_t motor_pulse_start(uint8_t wheel, uint8_t direction, uint8_t duty,
                         uint32_t duration_ms);
void motor_pulse_stop(void);
void motor_pulse_read(motor_pulse_status_t *p_status);
/* Motion-owner only. Signed electrical duty, zero=coast, lease1..100ms.
 * Same-duty updates renew the alarm without interrupting PWM. */
int32_t motor_drive_set(int8_t left_pct, int8_t right_pct, uint32_t lease_ms);
/* Follower only: read initialized hardware time, then submit an absolute
 * deadline at most100000us ahead. Checked inside the motor lock at actual
 * start/renew time; expired deadlines coast/latch rather than start late. */
uint32_t motor_drive_now_us(void);
int32_t motor_drive_set_until(int8_t left_pct, int8_t right_pct,
                               uint32_t deadline_us);
/* Same deadline contract. Each signed unit is half a percentage point. */
int32_t motor_drive_set_until_half_pct(int8_t left_half_pct, int8_t right_half_pct,
                                       uint32_t deadline_us);
/* Owner only, explicit rearm after a verified stop/coast; never starts output. */
int32_t motor_drive_rearm(void);
#endif
