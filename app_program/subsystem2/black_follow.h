/* Bounded stripe/corridor policy; geometry is selected at build time. */
#ifndef AAI05_BLACK_FOLLOW_H
#define AAI05_BLACK_FOLLOW_H
#include <stdbool.h>
#include <stdint.h>
#include "../../config/follow_config.h"

#define FOLLOW_IDLE 0U
#define FOLLOW_RUNNING 1U
#define FOLLOW_STOPPED 2U
#define FOLLOW_NONE 0U
#define FOLLOW_BUTTON 1U
#define FOLLOW_IR 2U
#define FOLLOW_RGB 3U
#define FOLLOW_HARDWARE 4U
#define FOLLOW_AMBIGUOUS 5U
#define FOLLOW_COMPLETE 6U
#define FOLLOW_DRIVER 7U
#define FOLLOW_JOIN_TIMEOUT 8U
#define FOLLOW_CORNER_NOT_SEEN 9U
#define FOLLOW_CORNER_TIMEOUT 10U
#define FOLLOW_LEASE_REFRESH 11U /* Diagnostic label for the command TTL. */
#define FOLLOW_CORNER_SEEK 0U
#define FOLLOW_CORNER_TURN 1U
#define FOLLOW_CORNER_EXIT 2U
#if FOLLOW_TWO_TURN
#define FOLLOW_BEND_SECOND_TURN 3U
#define FOLLOW_BEND_EXIT 4U
#endif
#define FOLLOW_RUN_MS 1000U
#define FOLLOW_IR_FRESH_MS 100U
#if FOLLOW_RED_SECOND_PIVOT
#if FOLLOW_RED_SECOND_SLOW
#define FOLLOW_RED_SECOND_PIVOT_MS 180U
#define FOLLOW_RED_SECOND_PIVOT_LIMIT_MS 200U
#else
#define FOLLOW_RED_SECOND_PIVOT_MS 160U
#define FOLLOW_RED_SECOND_PIVOT_LIMIT_MS 180U
#endif
#endif

typedef struct
{
    uint32_t now_ms;
    uint32_t ir_ms;
    uint16_t left_raw;
    uint16_t right_raw;
    bool has_ir;
    bool ir_valid;
    bool rgb_target_matched; /* Adapter supplies a fresh valid MATCH accepted by the mode. */
    bool rgb_join_active; /* Bounded permission to steer through a qualifying join. */
    bool rgb_join_timed_out;
    bool rgb_healthy; /* Valid acquisition, including an out-of-profile surface. */
    uint32_t rgb_sequence;
    uint8_t rgb_colour;
    bool blue_surface; /* Selects blue IR normalization in multicolour mode. */
#if FOLLOW_BLUE_RED_HANDOFF
    bool blue_red_blend;
#endif
#if FOLLOW_BLUE_EXIT_LEAD
    bool blue_exit_fade;
#endif
#if FOLLOW_BEND_CUE_REFINEMENT
    bool blue_inner_fade;
#endif
#if FOLLOW_BLUE_DIAGONAL_EXIT
    bool blue_exit_on_black;
#endif
#if FOLLOW_BLUE_EXIT_AFTER_RECOVERY
    bool blue_recovered_fade;
#endif
#if FOLLOW_RED_EXIT_LEAD
    bool red_surface;
    bool red_exit_fade;
#endif
#if FOLLOW_RED_BEND_AFTER_HANDOFF
    bool red_arrived_after_bend;
    bool red_handoff_mix;
#endif
#if FOLLOW_RED_SECOND_EXIT_REFINEMENT
    bool red_reacquired_after_join;
#endif
#if FOLLOW_RED_BETWEEN_BENDS
    uint16_t red_rgb_clear;
#endif
#if FOLLOW_RED_DIAGONAL_REACQUIRE
    bool red_diagonal_mix;
#endif
#if FOLLOW_BLUE_RED_CONFIRM_GRACE
    bool red_approach_grace;
#endif
#if FOLLOW_RED_RETURN_GRACE
    bool red_return_grace;
#endif
#if FOLLOW_HOME_TRACK
    bool yellow_surface;
#endif
#if FOLLOW_LOOP_RECOVERY_DIRECTION != 0
    bool bend_loss_rgb;
    bool rgb_same_lane;
#endif
#if FOLLOW_YELLOW_EXIT_CUE
    bool yellow_exit_cue;
#endif
#if FOLLOW_YELLOW_LEAD_CUE
    bool yellow_lead_cue;
#endif
    bool hardware_ready;
    bool start_pressed;
    bool stop_pressed;
} black_follow_input_t;

#if FOLLOW_STOP_TRACE
typedef struct
{
    uint32_t captured_ms;
    uint32_t rgb_ms;
    uint32_t join_age_ms;
    uint16_t left_raw;
    uint16_t right_raw;
    uint8_t rgb_result;
    bool join_active;
    bool valid;
} follow_stop_trace_t;
#endif

#if FOLLOW_BEND_TRACE
typedef struct
{
    uint32_t captured_ms;
    uint32_t ir_ms;
    uint32_t rgb_ms;
    uint16_t left_raw;
    uint16_t right_raw;
    uint16_t red;
    uint16_t green;
    uint16_t blue;
    uint16_t clear;
    uint8_t rgb_result;
} follow_bend_trace_t;
#endif

typedef struct
{
    uint16_t left_norm;
    uint16_t right_norm;
    uint16_t elapsed_ms;
    int8_t turn_pct;
    uint8_t phase;
    uint8_t reason;
    bool ready;
    bool join_active;
    uint8_t join_count;
    uint8_t corner_phase;
#if FOLLOW_STOP_TRACE
    follow_stop_trace_t stop_trace;
#endif
#if FOLLOW_BEND_TRACE
    follow_bend_trace_t bend_trace;
#endif
#if FOLLOW_BLUE_EXIT_LEAD
    bool blue_exit_lead_used;
#endif
#if FOLLOW_BLUE_EXIT_AFTER_RECOVERY
    bool blue_recovered_exit_used;
#endif
#if FOLLOW_RED_EXIT_LEAD
    bool red_exit_lead_used;
#endif
#if FOLLOW_RED_SECOND_PIVOT
    bool red_second_pivot_used;
#endif
#if FOLLOW_RED_SECOND_SLOW
    bool red_second_slow_used;
#endif
#if FOLLOW_RED_FIRST_BEND_BOOST
    bool red_first_pivot_used;
#endif
#if FOLLOW_RED_HANDOFF_ALIGN
    bool red_alignment_used;
#endif
#if FOLLOW_RED_EXIT_ALIGN
    bool red_exit_alignment_used;
#endif
#if FOLLOW_RED_BETWEEN_BENDS
    bool red_clear_pivot_used;
#endif
#if FOLLOW_RED_INSIDE_RELEASE
    bool red_second_release_used;
#endif
#if FOLLOW_RED_EXIT_TURN
    bool red_exit_turn_used;
#endif
#if FOLLOW_RED_BEND_AFTER_HANDOFF
    bool red_handoff_armed;
#endif
#if FOLLOW_BLUE_RED_CONFIRM_GRACE
    bool red_approach_grace_used;
#endif
#if FOLLOW_RED_RETURN_GRACE
    bool red_return_grace_used;
#endif
#if FOLLOW_BLUE_RED_HANDOFF
    bool blue_red_join_used;
#endif
#if FOLLOW_BLUE_SECOND_BOOST
    bool blue_second_used;
#endif
#if FOLLOW_BLUE_OUTER_EDGE
    bool blue_outer_used;
#endif
#if FOLLOW_BLUE_OUTER_RECOVERY
    bool blue_outer_recovery_used;
#endif
#if FOLLOW_BLUE_BEND_SLOW
    bool blue_bend_slow_used;
#endif
#if FOLLOW_BLUE_CONFIRM_GRACE
    bool blue_late_confirmed;
#endif
#if FOLLOW_YELLOW_LEAD_CUE
    bool yellow_lead_used;
#endif
#if FOLLOW_BLUE_REVERSAL_DAMPING
    bool blue_reversal_damped;
#endif
#if FOLLOW_MOTOR_HALF_PCT
    int8_t left_half_pct;
    int8_t right_half_pct;
#endif
#if FOLLOW_CLIPPED_LOOP
    uint8_t lease_limit;
    uint8_t last_lease_ms;
    bool lease_alarm;
#endif
#if FOLLOW_RED_OUTSIDE_RECOVERY
    bool red_outside_used;
#endif
} black_follow_status_t;

typedef struct
{
    black_follow_status_t status;
    uint32_t run_started_ms;
    uint32_t button_changed_ms;
    uint32_t corner_stage_started_ms;
    uint32_t corner_ir_since_ms;
    uint32_t corner_rgb_sequence;
#if FOLLOW_YELLOW_LEAD_CUE
    uint32_t yellow_lead_since_ms;
    bool yellow_lead_seen;
#endif
#if FOLLOW_BLUE_STEER_RAMP
    uint32_t blue_edge_since_ms;
    int8_t blue_edge_direction;
#endif
#if FOLLOW_BLUE_SECOND_BOOST
    uint32_t blue_reacquired_ms;
    bool blue_second_armed;
#endif
#if FOLLOW_BLUE_OUTER_EDGE
    uint32_t blue_outer_since_ms;
    bool blue_outer_seen;
#endif
#if FOLLOW_BEND_CUE_REFINEMENT
    uint32_t blue_outside_sample_ms;
    bool blue_outside_sample_seen;
#endif
#if FOLLOW_BLUE_BEND_SLOW
    uint32_t blue_outer_confirmed_ms;
    bool blue_outer_confirmed;
#endif
#if FOLLOW_BLUE_EXIT_LEAD
    uint32_t blue_exit_lead_ms;
    bool blue_exit_lead_seen;
#endif
#if FOLLOW_RED_EXIT_LEAD
    uint32_t red_outer_since_ms;
    uint32_t red_outer_sample_ms;
    uint32_t red_exit_lead_ms;
#if FOLLOW_RED_EXIT_ALIGN
    uint32_t red_second_pivot_ms;
#endif
#if FOLLOW_RED_INSIDE_RELEASE
    uint32_t red_second_release_ms;
#endif
#if FOLLOW_RED_EXIT_TURN
    uint32_t red_exit_turn_started_ms;
#endif
#if FOLLOW_RED_BETWEEN_BENDS
    uint16_t red_between_bends_peak_clear;
#endif
#if FOLLOW_RED_DIAGONAL_REACQUIRE
    uint32_t red_first_pivot_ms;
    uint32_t red_diagonal_inside_ms;
    bool red_diagonal_inside_seen;
#endif
    bool red_outer_seen;
    bool red_outer_confirmed;
    bool red_exit_lead_seen;
#endif
#if FOLLOW_RED_SECOND_PIVOT
    uint32_t red_recovered_ms;
    bool red_second_armed;
    bool red_second_active;
#endif
#if FOLLOW_RED_BEND_AFTER_HANDOFF
    uint32_t red_handoff_sequence;
    bool red_second_from_handoff;
#endif
#if FOLLOW_RED_FIRST_BEND_BOOST
    bool red_first_active;
#endif
#if FOLLOW_BLUE_REVERSAL_DAMPING
    uint32_t blue_last_edge_ms;
    int8_t blue_last_edge_direction;
    bool blue_reversal_ramp;
#endif
    uint8_t corner_rgb_colour;
    bool corner_signal_seen;
#if FOLLOW_LOOP_RECOVERY_DIRECTION != 0
    bool bend_recovering;
#endif
#if FOLLOW_RED_SPIN_HOLD
    uint32_t red_outside_seq;
#endif
#if FOLLOW_RED_OUTSIDE_RECOVERY
    uint32_t red_left_edge_ms;
    uint32_t red_outside_since_ms;
    bool red_left_edge_seen;
    bool red_outside_active;
#endif
    bool raw_pressed;
    bool stable_pressed;
    bool release_seen;
} black_follow_t;

typedef struct
{
    int8_t forward_pct;
    int8_t turn_pct;
    bool start; /* Explicit fresh press; adapter must rearm successfully first. */
    bool drive;
    bool pivot; /* Signed wheel duties are applied by the local Motion owner. */
#if FOLLOW_BLUE_EXIT_PIVOT
    bool blue_exit_pivot;
#endif
#if FOLLOW_RED_SECOND_ARC_BOOST
    bool red_second_exit_arc;
#endif
#if FOLLOW_RED_OUTSIDE_RECOVERY
    bool red_outside_spin; /* Own bound: not the red pivot window from red_exit_lead_ms. */
#endif
#if FOLLOW_BLUE_SECOND_LEFT_BOOST
    bool blue_second_left_boost;
#endif
#if FOLLOW_RED_FINAL_LEFT_BOOST
    bool red_final_arc;
#endif
#if FOLLOW_RED_FINAL_ARC_EXTEND
    bool red_final_arc_extended;
#endif
} black_follow_output_t;

#if FOLLOW_BLUE_EXIT_PIVOT
#define FOLLOW_BLUE_EXIT_PIVOT_MS 100U
#define FOLLOW_BLUE_EXIT_PIVOT_LIMIT_MS 160U
#endif

void black_follow_init(black_follow_t *p_state, uint32_t now_ms);
black_follow_output_t black_follow_step(black_follow_t *p_state,
                                        const black_follow_input_t *p_input);
void black_follow_abort(black_follow_t *p_state, uint8_t reason);
#if FOLLOW_RED_INSIDE_RELEASE
bool black_follow_red_inside_release_due(const black_follow_t *p_state,
                                         const black_follow_input_t *p_input);
#endif
#if FOLLOW_RED_OUTSIDE_RECOVERY
bool black_follow_red_outside_update(black_follow_t *p_state,
                                     const black_follow_input_t *p_input, bool pivot_active);
#endif
#if FOLLOW_RED_RIGHT_EDGE_OUTSIDE
bool black_follow_red_right_edge_outside(const black_follow_t *p_state,
                                         const black_follow_input_t *p_input);
#endif
#if FOLLOW_RED_CORNER_REFINE
bool black_follow_red_final_hold_ok(uint16_t left_norm, bool right_clear,
                                    bool confirmed_red, uint32_t turn_age_ms);
#endif
#if FOLLOW_RED_SPIN_HOLD
void black_follow_red_spin_start(black_follow_t *p_state, const black_follow_input_t *p_input);
#endif
uint32_t black_follow_remaining_ms(const black_follow_t *p_state, uint32_t now_ms,
                                   uint8_t *p_reason);
const char *black_follow_phase_name(uint8_t phase);
const char *black_follow_reason_name(uint8_t reason);
const char *black_follow_corner_name(uint8_t phase);
#endif
