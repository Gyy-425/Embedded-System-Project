#ifndef AAI05_BLACK_FOLLOW_OWNER_H
#define AAI05_BLACK_FOLLOW_OWNER_H
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include "../app.h"
#pragma GCC diagnostic pop
#include "black_follow.h"
#include "motion_control.h"
#include "motor_pulse.h"
#include "../subsystem1/rgb_sensor.h"

typedef struct
{
    uint32_t started_ms;
    uint32_t candidate_sequence;
    uint8_t lane;
    uint8_t candidate;
    uint8_t completed;
    bool active;
#if FOLLOW_BLUE_RED_CONFIRM_GRACE
    bool red_approach;
#endif
#if FOLLOW_RED_RETURN_GRACE
    bool late_red_return;
#endif
#if FOLLOW_BLUE_RED_LATE_CONFIRM
    bool late_blue_red;
#endif
#if FOLLOW_BLUE_RED_JOIN_EXTEND
    bool blue_red_extend;
#endif
} follow_join_t;

/* Entire context is private to Motion owner, never read by Report directly. */
typedef struct
{
    black_follow_t policy;
    motion_control_t motion;
    ir_snapshot_t ir;
    rgb_snapshot_t rgb;
    follow_join_t join;
    uint32_t ir_busy;
    uint32_t rgb_busy;
#if FOLLOW_BLUE_EXIT_AFTER_RECOVERY
    uint32_t blue_recovered_ms;
    bool blue_recovered;
#endif
#if FOLLOW_CLIPPED_LOOP
    uint32_t last_lease_cutoff_count;
#endif
    uint16_t sequence;
    bool has_ir;
    bool has_rgb;
} black_follow_owner_t;

void black_follow_owner_init(black_follow_owner_t *p_owner);
void black_follow_owner_step(black_follow_owner_t *p_owner, uint32_t buttons,
                              bool hardware_ready,
                              const motor_pulse_status_t *p_motor,
                              black_follow_status_t *p_follow,
                              motion_status_t *p_motion);
#endif
