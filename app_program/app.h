#ifndef AAI05_APP_H
#define AAI05_APP_H

#include <stdint.h>
#include <stdbool.h>
#include <tk/tkernel.h>
#include <tm/tmonitor.h>
#include <bsp/libbsp.h>
#include "subsystem2/encoder_capture.h"
#include "subsystem2/motor_test.h"
#include "subsystem2/motion_control.h"

#ifndef AAI05_COMMS_APP
#define AAI05_COMMS_APP 0
#endif
#ifndef AAI05_MOTION_APP
#define AAI05_MOTION_APP 0
#endif
#ifndef AAI05_REMOTE_APP
#define AAI05_REMOTE_APP 0
#endif
#ifndef AAI05_RGB_DIAG
#define AAI05_RGB_DIAG 0
#endif
#ifndef AAI05_FOLLOW_APP
#define AAI05_FOLLOW_APP 0
#endif
#ifndef AAI05_FOLLOW_MQTT
#define AAI05_FOLLOW_MQTT 0
#endif
#if AAI05_FOLLOW_MQTT
#include "subsystem2/follow_route.h"
#endif
#if AAI05_FOLLOW_APP
#include "../config/follow_config.h"
#endif
#define AAI05_RGB_ACTIVE (AAI05_RGB_DIAG || AAI05_FOLLOW_APP)
#include "subsystem2/black_follow.h"
#if AAI05_RGB_ACTIVE && (CNF_TIMER_PERIOD != 10)
#error RGB diagnostic delay margins require the reviewed 10ms kernel tick
#endif
#if AAI05_RGB_DIAG && (!MOTOR_TEST_PROBE || AAI05_MOTION_APP || \
    AAI05_REMOTE_APP || AAI05_COMMS_APP || AAI05_FOLLOW_APP || TM_NET_LWIP || TM_MQTT)
#error RGB diagnostic requires no-drive and no network application
#endif
#if AAI05_FOLLOW_APP && (AAI05_RGB_DIAG || AAI05_COMMS_APP || \
    !TK_SUPPORT_SMP || !AAI05_ENCODER_PIO)
#error FOLLOW requires an SMP/PIO profile with exclusive sensor ownership
#endif
#if AAI05_FOLLOW_APP && !AAI05_FOLLOW_MQTT && (AAI05_MOTION_APP || \
    AAI05_REMOTE_APP || TM_NET_LWIP || TM_MQTT)
#error Local FOLLOW requires an isolated profile
#endif
#if AAI05_FOLLOW_MQTT && (!AAI05_FOLLOW_APP || !AAI05_REMOTE_APP || \
    !FOLLOW_MULTICOLOUR_CORRIDOR)
#error MQTT FOLLOW requires the multicolour follower and remote transport
#endif

#if AAI05_MOTION_APP && (MOTOR_TEST_PROBE || AAI05_COMMS_APP)
#error Local Motion profile requires motor output and no network application
#endif
#if AAI05_REMOTE_APP && (!AAI05_MOTION_APP || !TM_NET_LWIP || !TM_MQTT)
#error Remote Motion requires the Motion, lwIP and MQTT profiles
#endif
#if AAI05_MOTION_APP && !AAI05_REMOTE_APP && TM_NET_LWIP
#error Local Motion cannot be combined with networking
#endif

#if AAI05_COMMS_APP && (!MOTOR_TEST_PROBE || !TM_NET_LWIP || !TM_MQTT)
#error Communication application requires no-drive, lwIP and MQTT profiles
#endif

#define TASK_STACK_SIZE 2048
#define APP_PRI_MOTION  12
#define APP_PRI_IR      14
#define APP_PRI_RGB     15
#define APP_PRI_REPORT  16
#define APP_IR_LEFT_PIN 26U
#define APP_IR_RIGHT_PIN 27U
#define APP_MOTOR_FIRST_PIN 8U
#define APP_MOTOR_LAST_PIN 11U

typedef struct {
    uint32_t sequence;
    uint32_t sampled_ms;
    uint16_t left_raw;
    uint16_t right_raw;
    ER error;
    bool valid;
} ir_snapshot_t;

typedef struct {
    uint32_t sequence;
    uint32_t sampled_ms;
    int32_t left_count;
    int32_t right_count;
    uint32_t left_invalid;
    uint32_t right_invalid;
    uint32_t irq_count;
    uint32_t left_overrun;
    uint32_t right_overrun;
    ER capture_error;
    uint8_t left_ab;
    uint8_t right_ab;
    uint8_t pio_invalid_flags;
    uint8_t pio_block;
    bool count_saturated;
    bool capture_ready;
    motor_pulse_status_t motor;
    uint8_t motor_phase;
    uint8_t motor_step;
    uint8_t motor_stop_reason;
    motion_status_t motion;
#if AAI05_FOLLOW_APP
    black_follow_status_t follow;
#endif
#if AAI05_FOLLOW_MQTT
    follow_route_report_t route;
    uint16_t route_epoch;
#endif
} motion_snapshot_t;

IMPORT ID app_start_task(FP entry, PRI priority, UW processor, const char *name);
IMPORT uint32_t app_now_ms(void);
IMPORT ER subsystem1_init(void);
IMPORT ER subsystem1_start(void);
IMPORT ER subsystem1_get_snapshot(ir_snapshot_t *snapshot);
IMPORT ER subsystem2_init(void);
IMPORT ER subsystem2_start(void);
IMPORT ER subsystem2_get_snapshot(motion_snapshot_t *snapshot);
IMPORT ER subsystem3_init(void);
IMPORT ER subsystem3_start(void);
#if APP_TEST
IMPORT void subsystem2_test(void);
#endif
#endif
