#ifndef AAI05_MOTION_CONTROL_H
#define AAI05_MOTION_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

#define MOTION_COMMAND_TTL_MS 100U
#define MOTION_MAX_DUTY 50

#define MOTION_READY 0U
#define MOTION_RUNNING 1U
#define MOTION_STOPPED 2U
#define MOTION_FAULT 3U

#define MOTION_REASON_NONE 0U
#define MOTION_REASON_REQUEST 1U
#define MOTION_REASON_BUTTON 2U
#define MOTION_REASON_EXPIRED 3U
#define MOTION_REASON_HARDWARE 4U

#define MOTION_ACCEPTED 0U
#define MOTION_DUPLICATE 1U
#define MOTION_BAD_COMMAND 2U
#define MOTION_STALE 3U
#define MOTION_CONFLICT 4U
#define MOTION_LATCHED 5U
#define MOTION_UNAVAILABLE 6U
typedef uint8_t motion_result_t;

/* Typed in-process data, never a wire-format/native-structure MQTT payload. */
typedef struct
{
    uint32_t issued_ms;
    uint16_t sequence;
    int8_t left_pct;
    int8_t right_pct;
} motion_command_t;

typedef struct
{
    motion_command_t command;
    uint8_t phase;
    uint8_t reason;
    bool has_command;
    bool has_sequence;
} motion_control_t;

typedef struct
{
    int8_t left_pct;
    int8_t right_pct;
    uint8_t lease_ms;
} motion_output_t;

typedef struct
{
    uint32_t issued_ms;
    uint16_t sequence;
    int8_t left_pct;
    int8_t right_pct;
    uint16_t epoch;
    uint8_t phase;
    uint8_t reason;
    uint8_t lease_ms;
    bool has_sequence;
} motion_status_t;

/* Caller owns synchronization; step and hardware application share its lock. */
void motion_control_init(motion_control_t *p_control);
motion_result_t motion_control_submit(motion_control_t *p_control,
                                      const motion_command_t *p_command,
                                      uint32_t now_ms);
void motion_control_stop(motion_control_t *p_control, uint8_t reason);
void motion_control_step(motion_control_t *p_control, uint32_t now_ms,
                          bool is_stop_pressed, bool is_hardware_ready,
                          bool is_driver_active, motion_output_t *p_output);
#endif
