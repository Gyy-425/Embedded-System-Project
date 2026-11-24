#include "black_follow.h"

static uint16_t normalize(uint16_t raw, uint16_t baseline, uint16_t span)
{
    if (raw <= baseline)
    {
        return 0U;
    }
    uint32_t value = ((uint32_t)raw - baseline) * 1000U / span;
    return (uint16_t)(value > 1000U ? 1000U : value);
}

void black_follow_init(black_follow_t *p_state, uint32_t now_ms)
{
    *p_state = (black_follow_t){.button_changed_ms=now_ms,
                              .raw_pressed=true, .stable_pressed=true};
}

void black_follow_abort(black_follow_t *p_state, uint8_t reason)
{
    p_state->status.phase = FOLLOW_STOPPED;
    p_state->status.reason = reason;
    p_state->status.turn_pct = 0;
}

static bool fresh_press(black_follow_t *p_state,
                         const black_follow_input_t *p_input)
{
    bool pressed = false;
    if (p_input->start_pressed != p_state->raw_pressed)
    {
        p_state->raw_pressed = p_input->start_pressed;
        p_state->button_changed_ms = p_input->now_ms;
    }
    if ((uint32_t)(p_input->now_ms - p_state->button_changed_ms) >= 30U)
    {
        if (!p_state->raw_pressed)
        {
            p_state->stable_pressed = false;
            p_state->release_seen = true;
        }
        else if (!p_state->stable_pressed)
        {
            p_state->stable_pressed = true;
            pressed = p_state->release_seen;
            p_state->release_seen = false;
        }
    }
    return pressed;
}

static uint32_t run_limit_ms(void)
{
#if FOLLOW_CLIPPED_LOOP
    return FOLLOW_CLIPPED_RUN_MS;
#elif FOLLOW_CORNER_DIRECTION != 0
    return FOLLOW_CORNER_RUN_MS;
#else
    return FOLLOW_RUN_MS;
#endif
}

uint32_t black_follow_remaining_ms(const black_follow_t *p_state, uint32_t now_ms,
                                   uint8_t *p_reason)
{
    uint32_t elapsed = now_ms - p_state->run_started_ms;
    uint32_t remaining = elapsed < run_limit_ms() ? run_limit_ms() - elapsed : 0U;
    *p_reason = FOLLOW_COMPLETE;
#if FOLLOW_TWO_TURN
    const uint32_t stage_limits[] = {2500U, 420U, 1500U, 420U, 350U};
    const uint8_t stage_reasons[] = {
        FOLLOW_CORNER_NOT_SEEN, FOLLOW_CORNER_TIMEOUT, FOLLOW_CORNER_NOT_SEEN,
        FOLLOW_CORNER_TIMEOUT, FOLLOW_COMPLETE
    };
#elif FOLLOW_CORNER_DIRECTION != 0
    const uint32_t stage_limits[] = {
        FOLLOW_CORNER_APPROACH_MS, FOLLOW_CORNER_MAX_TURN_MS, FOLLOW_CORNER_EXIT_MS
    };
    const uint8_t stage_reasons[] = {
        FOLLOW_CORNER_NOT_SEEN, FOLLOW_CORNER_TIMEOUT, FOLLOW_COMPLETE
    };
#endif
#if FOLLOW_TWO_TURN || FOLLOW_CORNER_DIRECTION != 0
#if AAI05_FOLLOW_MQTT
    if (p_state->straight_mode)
    {
        return remaining;
    }
#endif
    uint8_t phase = p_state->status.corner_phase;
    uint32_t stage_elapsed = now_ms - p_state->corner_stage_started_ms;
    uint32_t stage_left = stage_elapsed < stage_limits[phase] ?
                          stage_limits[phase] - stage_elapsed : 0U;
    if (stage_left < remaining)
    {
        *p_reason = stage_reasons[phase];
        remaining = stage_left;
    }
#endif
    return remaining;
}

#if FOLLOW_CORNER_DIRECTION != 0
static void update_corner(black_follow_t *p_state,
                          const black_follow_input_t *p_input, bool both_high)
{
    if (p_state->status.corner_phase == FOLLOW_CORNER_SEEK)
    {
        if (!both_high)
        {
            p_state->corner_signal_seen = false;
        }
        else if (!p_state->corner_signal_seen)
        {
            p_state->corner_signal_seen = true;
            p_state->corner_ir_since_ms = p_input->ir_ms;
        }
        else if ((p_input->ir_ms - p_state->corner_ir_since_ms) >= FOLLOW_CORNER_CUE_MS)
        {
            p_state->status.corner_phase = FOLLOW_CORNER_TURN;
            p_state->corner_stage_started_ms = p_input->now_ms;
            p_state->corner_signal_seen = false;
        }
    }
    else if (p_state->status.corner_phase == FOLLOW_CORNER_TURN)
    {
        uint32_t elapsed = p_input->now_ms - p_state->corner_stage_started_ms;
        bool lane_seen = (elapsed >= FOLLOW_CORNER_MIN_TURN_MS) &&
                         p_input->rgb_target_matched &&
                         (p_input->left_raw < 450U) && (p_input->right_raw < 450U);
        if (!lane_seen)
        {
            p_state->corner_signal_seen = false;
        }
        else if (!p_state->corner_signal_seen ||
                 (p_state->corner_rgb_colour != p_input->rgb_colour))
        {
            p_state->corner_signal_seen = true;
            p_state->corner_ir_since_ms = p_input->ir_ms;
            p_state->corner_rgb_sequence = p_input->rgb_sequence;
            p_state->corner_rgb_colour = p_input->rgb_colour;
        }
        else if ((p_input->ir_ms - p_state->corner_ir_since_ms >= FOLLOW_CORNER_LANE_STABLE_MS) &&
                 (p_state->corner_rgb_sequence != p_input->rgb_sequence))
        {
            p_state->status.corner_phase = FOLLOW_CORNER_EXIT;
            p_state->corner_stage_started_ms = p_input->now_ms;
        }
    }
}
#endif

#if FOLLOW_TWO_TURN
static bool bend_turning(uint8_t phase)
{
    return (phase == FOLLOW_CORNER_TURN) || (phase == FOLLOW_BEND_SECOND_TURN);
}

static bool bend_centred(const black_follow_t *p_state)
{
    return (p_state->status.left_norm <= 120U) && (p_state->status.right_norm <= 120U);
}

static void update_two_turn_bend(black_follow_t *p_state, const black_follow_input_t *p_input)
{
    uint8_t phase = p_state->status.corner_phase;
    if (phase == FOLLOW_BEND_EXIT)
    {
        return;
    }
    bool turning = bend_turning(phase);
    bool on_lane = p_input->rgb_target_matched && !p_input->rgb_join_active;
    /* On the clockwise approach, the left sensor is on the outside. */
    bool outside_edge = (p_state->status.left_norm == 1000U) &&
                        (p_state->status.right_norm <= 150U);
    bool turn_old_enough = (p_input->now_ms - p_state->corner_stage_started_ms) >= 100U;
    bool cue = on_lane && (turning ? (turn_old_enough && bend_centred(p_state)) : outside_edge);
    if (!cue)
    {
        p_state->corner_signal_seen = false;
        return;
    }
    if (!p_state->corner_signal_seen ||
        (turning && p_state->corner_rgb_colour != p_input->rgb_colour))
    {
        p_state->corner_signal_seen = true;
        p_state->corner_ir_since_ms = p_input->ir_ms;
        p_state->corner_rgb_sequence = p_input->rgb_sequence;
        p_state->corner_rgb_colour = p_input->rgb_colour;
        return;
    }
    if ((p_input->ir_ms - p_state->corner_ir_since_ms >= 40U) &&
        (!turning || p_state->corner_rgb_sequence != p_input->rgb_sequence))
    {
        /* SEEK -> TURN_1 -> DIAGONAL -> TURN_2 -> EXIT. */
        p_state->status.corner_phase = (uint8_t)(phase + 1U);
        p_state->corner_stage_started_ms = p_input->now_ms;
        p_state->corner_signal_seen = false;
    }
}
#endif

#if FOLLOW_BLUE_STEER_RAMP
static int32_t ramp_blue_edge_turn(black_follow_t *p_state, const black_follow_input_t *p_input,
                                   bool blue_surface, int32_t turn)
{
    if (!blue_surface || !p_input->rgb_target_matched || p_input->rgb_join_active)
    {
        p_state->blue_edge_direction = 0;
#if FOLLOW_BLUE_REVERSAL_DAMPING
        p_state->blue_last_edge_direction = 0;
#endif
        return turn;
    }
    if ((turn >= -FOLLOW_BLUE_RAMP_START) && (turn <= FOLLOW_BLUE_RAMP_START))
    {
        p_state->blue_edge_direction = 0;
        return turn;
    }

    /* Build up the stronger correction while fresh IR samples continue
     * to indicate the same edge. */
    int8_t direction = turn > 0 ? 1 : -1;
    if (direction != p_state->blue_edge_direction)
    {
#if FOLLOW_BLUE_REVERSAL_DAMPING
        /* A centred read releases the steering immediately. Remember the last
         * strong edge briefly, so crossing the centre cannot hide a reversal. */
        bool recent = (p_state->blue_last_edge_direction != 0) &&
                      (p_input->ir_ms - p_state->blue_last_edge_ms <= 400U);
        if (!recent || (direction != p_state->blue_last_edge_direction))
        {
            p_state->blue_reversal_ramp = recent;
        }
        if (p_state->blue_reversal_ramp)
        {
            p_state->status.blue_reversal_damped = true;
        }
#endif
        p_state->blue_edge_direction = direction;
        p_state->blue_edge_since_ms = p_input->ir_ms;
    }
#if FOLLOW_BLUE_REVERSAL_DAMPING
    p_state->blue_last_edge_direction = direction;
    p_state->blue_last_edge_ms = p_input->ir_ms;
    const uint32_t ramp_ms = p_state->blue_reversal_ramp ? 400U : 120U;
#else
    const uint32_t ramp_ms = 120U;
#endif
    uint32_t elapsed = p_input->ir_ms - p_state->blue_edge_since_ms;
    if (elapsed >= ramp_ms)
    {
        return turn;
    }
    uint32_t boost = elapsed * (FOLLOW_TURN_PEAK_PCT - FOLLOW_BLUE_RAMP_START) / ramp_ms;
    int32_t limit = FOLLOW_BLUE_RAMP_START + (int32_t)boost;
    return (turn * direction > limit) ? direction * limit : turn;
}
#endif

#if FOLLOW_BLUE_OUTER_EDGE
static int32_t blue_outer_edge_turn(black_follow_t *p_state, const black_follow_input_t *p_input,
                                    bool blue_surface, int32_t turn)
{
    /* At a clockwise bend, this edge is outside. Confirm it over fresh IR
     * samples while blue still matches and the inside sensor reads clear. */
    bool matched_blue = blue_surface && p_input->rgb_target_matched && !p_input->rgb_join_active;
    bool outside_edge = matched_blue && p_input->left_raw >= 550U && p_state->status.right_norm == 0U;
#if FOLLOW_BEND_CUE_REFINEMENT
    if (!matched_blue)
    {
        p_state->blue_outside_sample_seen = false;
    }
    else if (outside_edge)
    {
        p_state->blue_outside_sample_seen = true;
        p_state->blue_outside_sample_ms = p_input->ir_ms;
    }
#endif
    if (!outside_edge)
    {
        p_state->blue_outer_seen = false;
    }
    else if (!p_state->blue_outer_seen)
    {
        p_state->blue_outer_seen = true;
        p_state->blue_outer_since_ms = p_input->ir_ms;
    }
    else if (p_input->ir_ms - p_state->blue_outer_since_ms >= 40U)
    {
#if FOLLOW_BLUE_BEND_SLOW
        p_state->blue_outer_confirmed = true;
        p_state->blue_outer_confirmed_ms = p_input->ir_ms;
#endif
        if (turn < 9)
        {
            p_state->status.blue_outer_used = true;
            turn = 9;
        }
    }
    return turn;
}
#endif

#if FOLLOW_BLUE_EXIT_LEAD
static bool blue_exit_lead(black_follow_t *p_state, const black_follow_input_t *p_input,
                           bool blue_surface)
{
    const uint16_t left_max = FOLLOW_BLUE_RED_BEND_CUES ? 650U : 80U;
    bool edge_ready = p_state->status.left_norm <= left_max;
    uint32_t hold_ms = 150U;
#if FOLLOW_BLUE_EXIT_TURN == 22
    /* Also start while the outside edge is still visible if it has been
     * sustained and RGB already shows the measured fading-blue cue. */
    edge_ready = edge_ready || (p_state->blue_outer_seen &&
        p_input->ir_ms - p_state->blue_outer_since_ms >= 120U);
    hold_ms = 360U;
#endif
#if FOLLOW_BLUE_DIAGONAL_EXIT
    /* Keep a started exit turn through the black border while outside IR
     * remains clear. The owner's join timeout still applies. */
    if (p_input->blue_exit_on_black && p_input->rgb_join_active &&
        p_state->status.left_norm <= 80U)
    {
        hold_ms = 450U;
    }
#endif
    bool fading = p_input->blue_exit_fade;
    bool outside_border_cue = fading && p_state->blue_outer_confirmed && edge_ready;
#if FOLLOW_BEND_CUE_REFINEMENT
    /* The second blue bend can take the sensors quickly from the outside
     * edge to the inside edge while RGB still matches fading blue. */
    bool crossed_inside = p_input->blue_inner_fade && p_state->blue_outside_sample_seen &&
        p_input->ir_ms - p_state->blue_outside_sample_ms <= 180U &&
        p_state->status.left_norm <= 80U && p_input->right_raw >= 850U;
    outside_border_cue = outside_border_cue || crossed_inside;
    fading = fading || (p_input->blue_inner_fade && (crossed_inside || p_state->blue_exit_lead_seen));
#endif
    bool recovered_cue = false;
#if FOLLOW_BLUE_EXIT_AFTER_RECOVERY
    fading = fading || p_input->blue_recovered_fade;
    const uint16_t recovered_left_max = FOLLOW_BLUE_RED_BEND_TRIAL ? 160U : 80U;
    recovered_cue = p_input->blue_recovered_fade && p_state->status.left_norm <= recovered_left_max;
#endif
    bool bend_ready = outside_border_cue || recovered_cue;
#if FOLLOW_BLUE_EXIT_EDGE_CONFIRM
    /* A fading blue frame and a left edge can occur before the diagonal.
     * Wait for the left edge to clear and the right IR to rise. */
    bool crossed_to_right = p_state->blue_outside_sample_seen &&
        p_input->ir_ms - p_state->blue_outside_sample_ms <= 180U &&
        p_state->status.left_norm <= 80U && p_state->status.right_norm >= 240U;
    fading = fading || (p_input->blue_inner_fade && crossed_to_right);
    bend_ready = crossed_to_right && fading && p_input->rgb_target_matched;
#endif
    if (!blue_surface || p_input->blue_red_blend ||
        (p_input->rgb_target_matched && !fading))
    {
        p_state->blue_exit_lead_seen = false;
    }
    else if (!p_state->blue_exit_lead_seen && !p_input->rgb_join_active &&
             bend_ready)
    {
        p_state->blue_exit_lead_seen = true;
        p_state->blue_exit_lead_ms = p_input->now_ms;
#if FOLLOW_BLUE_EXIT_AFTER_RECOVERY
        if (recovered_cue && !outside_border_cue)
        {
            p_state->status.blue_recovered_exit_used = true;
        }
#endif
    }
    return p_state->blue_exit_lead_seen && p_input->now_ms - p_state->blue_exit_lead_ms < hold_ms;
}
#endif

#if FOLLOW_RED_EXIT_LEAD
static const uint32_t red_exit_hold_ms = FOLLOW_RED_EXIT_EARLY ? 360U : 260U;

#if FOLLOW_RED_HANDOFF_ALIGN
static bool red_handoff_aligning(const black_follow_t *p_state,
                                 const black_follow_input_t *p_input)
{
    /* Move forward onto red before allowing the first pivot. A brief
     * red-like mix can continue the alignment within the normal join lease. */
    bool near_red = p_input->rgb_join_active ? p_input->red_handoff_mix : p_input->rgb_target_matched;
    return p_input->red_surface && p_state->red_second_armed && p_state->red_second_from_handoff &&
        p_input->now_ms - p_state->red_recovered_ms < 180U &&
        p_state->status.right_norm <= 80U && near_red;
}
#endif

#if FOLLOW_RED_EXIT_ALIGN
static const uint32_t red_exit_alignment_ms = 240U;

static bool red_second_exit_aligning(const black_follow_t *p_state,
                                     const black_follow_input_t *p_input)
{
    /* Keep a shallow arc briefly after the second pivot's usual turn window.
     * Give ordinary steering priority when either IR sensor sees a strong edge. */
    uint32_t pivot_age_ms = p_input->now_ms - p_state->red_second_pivot_ms;
    bool within_alignment = pivot_age_ms >= red_exit_hold_ms &&
        pivot_age_ms < red_exit_hold_ms + red_exit_alignment_ms;
#if FOLLOW_RED_INSIDE_RELEASE
    if (p_state->status.red_second_release_used)
    {
        within_alignment = p_input->now_ms - p_state->red_second_release_ms < red_exit_alignment_ms;
    }
#endif
    bool near_red = p_input->rgb_join_active ? p_input->red_handoff_mix : p_input->rgb_target_matched;
    return p_input->red_surface && p_state->status.red_alignment_used &&
        p_state->status.red_second_pivot_used && near_red &&
        within_alignment &&
        p_state->status.left_norm <= 320U && p_state->status.right_norm <= 180U;
}
#endif

#if FOLLOW_RED_EXIT_TURN
static const uint32_t red_exit_arc_hold_ms = 280U;
#if FOLLOW_RED_CORNER_REFINE
bool black_follow_red_final_hold_ok(uint16_t left_norm, bool right_clear,
                                    bool confirmed_red, uint32_t turn_age_ms)
{
    /* v0.86: v0.85 test2 ended the final arc at 300 ms because the LEFT IR
     * reached the outer border, then rode that border for 600 ms at turn 14.
     * An outer-border reading now keeps the arc for up to 150 ms more. */
    uint32_t limit_ms = red_exit_arc_hold_ms + (FOLLOW_RED_FINAL_ARC_EXTEND ? 100U : 0U);
    if (left_norm > 320U)
    {
        limit_ms += 150U;
    }
    return confirmed_red && right_clear && (turn_age_ms < limit_ms);
}
#endif

static bool red_late_exit_turn(black_follow_t *p_state, const black_follow_input_t *p_input)
{
    bool right_clear = p_state->status.right_norm <= 80U;
    bool outside_edge = FOLLOW_RED_EXIT_TURN_STRONG && right_clear && p_state->status.left_norm >= 600U;
    bool exit_ready = p_state->status.red_exit_alignment_used || outside_edge ||
        (FOLLOW_RED_EXIT_TURN_STRONG && p_state->status.red_exit_turn_used);
    bool left_usable = p_state->status.left_norm <= 320U || (FOLLOW_RED_EXIT_TURN_STRONG && right_clear);
    if (!p_input->red_surface || !p_state->status.red_second_pivot_used || !exit_ready ||
        p_state->status.red_second_release_used || !left_usable ||
        p_state->status.right_norm > 240U)
    {
        return false;
    }

    /* The last two runs saw the outside edge soon after the second pivot.
     * With clear inside IR this can cue the arc before alignment finishes. */
    const uint32_t normal_exit_ms = red_exit_hold_ms + red_exit_alignment_ms;
    const uint32_t turn_earliest_ms = FOLLOW_RED_EXIT_TURN_STRONG ? red_exit_hold_ms + 20U :
        normal_exit_ms - (FOLLOW_RED_EXIT_TURN_EARLY ? 100U : 0U);
    uint32_t pivot_age_ms = p_input->now_ms - p_state->red_second_pivot_ms;
    bool confirmed_red = p_input->rgb_target_matched && !p_input->rgb_join_active;
    if (!p_state->status.red_exit_turn_used && confirmed_red && p_input->red_exit_fade &&
        p_input->red_rgb_clear <= 645U && right_clear &&
        pivot_age_ms >= turn_earliest_ms && pivot_age_ms < normal_exit_ms + 120U)
    {
        p_state->red_exit_turn_started_ms = p_input->now_ms;
        p_state->status.red_exit_turn_used = true;
    }

    bool near_red = confirmed_red || (p_input->rgb_join_active && p_input->red_handoff_mix);
    uint32_t turn_age_ms = p_input->now_ms - p_state->red_exit_turn_started_ms;
    /* The extension yields as soon as red or either clear IR reading is lost. */
    const uint32_t arc_limit_ms = red_exit_arc_hold_ms + (FOLLOW_RED_FINAL_ARC_EXTEND ? 100U : 0U);
#if FOLLOW_RED_CORNER_REFINE
    (void)arc_limit_ms;
    bool hold_on_red = black_follow_red_final_hold_ok(p_state->status.left_norm, right_clear,
                                                      confirmed_red, turn_age_ms);
#else
    bool hold_on_red = FOLLOW_RED_EXIT_TURN_HOLD && confirmed_red && right_clear &&
        p_state->status.left_norm <= 320U && turn_age_ms < arc_limit_ms;
#endif
    return p_state->status.red_exit_turn_used && near_red &&
        (turn_age_ms < 150U || hold_on_red);
}
#endif

#if FOLLOW_RED_BETWEEN_BENDS
static bool red_between_bends_fade(black_follow_t *p_state, const black_follow_input_t *p_input,
                                  uint32_t first_turn_age)
{
    if (!p_input->red_surface || (!FOLLOW_RED_DIAGONAL_REACQUIRE && !p_state->red_first_active) ||
        !p_state->status.red_alignment_used || !p_state->status.red_first_pivot_used ||
        p_state->status.red_second_pivot_used)
    {
        return false;
    }

    /* Centre-lane red was brighter on the diagonal, even on runs whose
     * RGB ratios still met the older fade cue. Measure the drop from that peak. */
    const uint16_t min_clear = FOLLOW_RED_INSIDE_RELEASE ? 654U : 655U;
    if (p_input->rgb_target_matched && !p_input->rgb_join_active &&
        first_turn_age >= FOLLOW_RED_SECOND_PIVOT_MS && first_turn_age <= 400U &&
        p_state->status.left_norm <= 80U && p_state->status.right_norm <= 80U &&
        p_input->red_rgb_clear >= min_clear && p_input->red_rgb_clear > p_state->red_between_bends_peak_clear)
    {
        p_state->red_between_bends_peak_clear = p_input->red_rgb_clear;
    }

    bool fading = p_input->red_exit_fade || (p_input->rgb_join_active && p_input->red_handoff_mix);
#if FOLLOW_RED_DIAGONAL_REACQUIRE
    /* Solid red clears the first turn command. Retain the diagonal cue,
     * but require a recent inside-edge reading before using it again. */
    if (first_turn_age >= red_exit_hold_ms && first_turn_age <= 750U &&
        p_state->status.left_norm <= 80U && p_state->status.right_norm >= 600U)
    {
        p_state->red_diagonal_inside_seen = true;
        p_state->red_diagonal_inside_ms = p_input->ir_ms;
    }
    if (!p_state->red_first_active)
    {
        if (!p_state->red_diagonal_inside_seen || p_input->ir_ms - p_state->red_diagonal_inside_ms > 120U)
        {
            return false;
        }
        fading = fading || (p_input->rgb_join_active && p_input->red_diagonal_mix);
    }
#endif
    return fading && first_turn_age >= red_exit_hold_ms && first_turn_age <= 750U &&
        p_input->red_rgb_clear + 12U <= p_state->red_between_bends_peak_clear &&
        p_state->status.left_norm <= 320U && p_state->status.right_norm <= 80U;
}
#endif

#if FOLLOW_RED_INSIDE_RELEASE
bool black_follow_red_inside_release_due(const black_follow_t *p_state,
                                         const black_follow_input_t *p_input)
{
    /* A strong inside edge during this pivot calls for normal steering.
     * v0.83: v0.82 test1 kept pivoting onto the inside cardboard because
     * red_clear_pivot had not been set; the IR edge alone now releases it. */
    uint32_t second_turn_age = p_input->now_ms - p_state->red_second_pivot_ms;
    bool clear_pivot_ok = FOLLOW_RED_PIVOT_INSIDE_RELEASE || p_state->status.red_clear_pivot_used;
    return p_input->red_surface && p_state->red_second_active && clear_pivot_ok &&
        p_state->status.red_second_pivot_used && !p_state->status.red_second_release_used &&
        second_turn_age >= 60U && second_turn_age < red_exit_hold_ms &&
        p_state->status.left_norm <= 80U && p_input->right_raw >= 600U;
}
#endif

#if FOLLOW_RED_OUTSIDE_RECOVERY
static const uint32_t red_outside_pair_ms = 250U;
static const uint32_t red_outside_hold_ms = 350U;

bool black_follow_red_outside_update(black_follow_t *p_state,
                                     const black_follow_input_t *p_input, bool pivot_active)
{
    /* v0.84: in both v0.83 runs the LEFT IR crossed the outer border and,
     * 100-200 ms later, the RIGHT IR crossed the same outer border. Normal
     * steering read that as the inner border and turned left, off the lap.
     * Between the first red pivot and the final arc, a right edge that
     * follows a left edge this closely means the car is leaving outside:
     * spin right until red returns, the left IR sees the border, or the
     * bounded hold ends. */
#if FOLLOW_RED_SPIN_HOLD
    /* v0.87: same corner window as the right-edge rule, so a spin started
     * in the first 500 ms of the final arc is not dropped at once. */
    bool final_arc_done = p_state->status.red_exit_turn_used &&
        (p_input->now_ms - p_state->red_exit_turn_started_ms >= 500U);
    bool in_section = p_input->red_surface && p_state->status.red_first_pivot_used &&
        !final_arc_done;
#else
    bool in_section = p_input->red_surface && p_state->status.red_first_pivot_used &&
        !p_state->status.red_exit_turn_used;
#endif
    if (!in_section)
    {
        p_state->red_outside_active = false;
        p_state->red_left_edge_seen = false;
        return false;
    }
    uint32_t now = p_input->now_ms;
    if (p_state->red_outside_active)
    {
        bool red_back = p_input->rgb_target_matched && !p_input->rgb_join_active;
#if FOLLOW_RED_SPIN_HOLD
        /* v0.86 test2/test3 spun for one cycle: the colour frame that was
         * still RED when the spin began released it. Only a newer frame may. */
        red_back = p_input->rgb_target_matched &&
            (p_input->rgb_sequence != p_state->red_outside_seq);
#endif
        if (red_back || (p_state->status.left_norm >= 500U) ||
            (now - p_state->red_outside_since_ms >= red_outside_hold_ms))
        {
            p_state->red_outside_active = false;
            p_state->red_left_edge_seen = false;
            return false;
        }
        return true;
    }
    if (p_state->status.left_norm >= 500U)
    {
        p_state->red_left_edge_ms = now;
        p_state->red_left_edge_seen = true;
        return false;
    }
    if (!pivot_active && p_state->red_left_edge_seen &&
        (now - p_state->red_left_edge_ms <= red_outside_pair_ms) &&
        (p_state->status.left_norm <= 80U) && (p_state->status.right_norm >= 500U))
    {
        p_state->red_outside_active = true;
        p_state->red_outside_since_ms = now;
        p_state->status.red_outside_used = true;
#if FOLLOW_RED_SPIN_HOLD
        p_state->red_outside_seq = p_input->rgb_sequence;
#endif
        return true;
    }
    return false;
}
#if FOLLOW_RED_SPIN_HOLD
void black_follow_red_spin_start(black_follow_t *p_state, const black_follow_input_t *p_input)
{
    p_state->red_outside_active = true;
    p_state->red_outside_since_ms = p_input->now_ms;
    p_state->red_outside_seq = p_input->rgb_sequence;
    p_state->status.red_outside_used = true;
}
#endif
#endif

#if FOLLOW_RED_RIGHT_EDGE_OUTSIDE
bool black_follow_red_right_edge_outside(const black_follow_t *p_state,
                                         const black_follow_input_t *p_input)
{
    /* v0.85: all four v0.84 runs left the red corner the same way. The
     * right IR rose (403..1098) during or just before a commanded right
     * turn, steering answered with a LEFT turn, and the car finished past
     * the outer border. Once the car overshoots, that border lies to its
     * right. From the blue/red handoff until 500 ms into the final exit
     * arc, a right edge with the left clear spins right instead. */
    bool final_arc_done = p_state->status.red_exit_turn_used &&
        (p_input->now_ms - p_state->red_exit_turn_started_ms >= 500U);
#if FOLLOW_RED_CORNER_REFINE
    /* v0.86: v0.85 test4 fired on the bottom straight right after the
     * handoff, where the right border really is the inner one. */
    bool in_corner = p_state->status.red_first_pivot_used;
#else
    bool in_corner = p_state->status.red_alignment_used;
#endif
    return p_input->red_surface && in_corner && !final_arc_done &&
        (p_state->status.right_norm >= 400U) && (p_state->status.left_norm <= 80U);
}
#endif

static bool red_exit_lead(black_follow_t *p_state, const black_follow_input_t *p_input)
{
#if FOLLOW_RED_INSIDE_RELEASE
    /* Remember the release time so the forward alignment stays bounded. */
    if (black_follow_red_inside_release_due(p_state, p_input))
    {
        p_state->red_second_active = false;
        p_state->red_exit_lead_seen = false;
        p_state->red_second_release_ms = p_input->now_ms;
        p_state->status.red_second_release_used = true;
    }
#endif
    if (!p_input->red_surface || p_input->rgb_join_active || !p_input->rgb_target_matched)
    {
        p_state->red_outer_seen = false;
        p_state->red_outer_confirmed = false;
    }
    else if (p_state->status.left_norm == 1000U && p_state->status.right_norm <= 80U)
    {
        if (!p_state->red_outer_seen)
        {
            p_state->red_outer_seen = true;
            p_state->red_outer_since_ms = p_input->ir_ms;
        }
        else if (p_input->ir_ms - p_state->red_outer_since_ms >= 40U)
        {
            p_state->red_outer_confirmed = true;
            p_state->red_outer_sample_ms = p_input->ir_ms;
        }
    }
    else
    {
        p_state->red_outer_seen = false;
    }

    bool outside_red_fade = p_input->red_exit_fade && p_state->red_outer_confirmed &&
        p_input->now_ms - p_state->red_outer_sample_ms < 350U;
    bool second_ready = false;
    bool fading = p_input->red_exit_fade;
#if FOLLOW_RED_SECOND_PIVOT
    if (!p_input->red_surface)
    {
        p_state->red_second_armed = false;
    }
    uint32_t first_turn_age = p_input->now_ms - p_state->red_exit_lead_ms;
    bool red_recovered = p_input->red_surface && p_input->rgb_target_matched &&
        !p_input->red_exit_fade && p_state->red_exit_lead_seen &&
        p_state->status.red_exit_lead_used && first_turn_age >= 80U && first_turn_age <= 400U;
#if FOLLOW_RED_SECOND_EXIT_REFINEMENT
    /* Two fresh red matches can close a brief loss even if red is still
     * faded. Accept this only just after the first pivot's turn window. */
    bool reacquired_after_first = p_input->red_reacquired_after_join &&
        p_state->red_first_active && first_turn_age >= red_exit_hold_ms && first_turn_age <= 750U;
    red_recovered = red_recovered || reacquired_after_first;
#endif
    if (red_recovered && !p_state->status.red_second_pivot_used)
    {
#if FOLLOW_RED_SECOND_EXIT_REFINEMENT
        if (reacquired_after_first)
        {
            p_state->red_exit_lead_seen = false;
            p_state->red_first_active = false;
            p_state->red_second_active = false;
        }
#endif
        p_state->red_recovered_ms = p_input->now_ms;
        p_state->red_second_armed = true;
#if FOLLOW_RED_BEND_AFTER_HANDOFF
        p_state->red_second_from_handoff = false;
#endif
    }
    bool ready_for_next_sample = true;
    bool mix_after_handoff = false;
#if FOLLOW_RED_BEND_AFTER_HANDOFF
    bool handoff_turn_available = !p_state->status.red_second_pivot_used;
#if FOLLOW_RED_FIRST_BEND_BOOST
    handoff_turn_available = handoff_turn_available && !p_state->status.red_first_pivot_used;
#endif
    if (p_input->red_arrived_after_bend && handoff_turn_available)
    {
        p_state->red_recovered_ms = p_input->now_ms;
        p_state->red_handoff_sequence = p_input->rgb_sequence;
        p_state->red_second_armed = true;
        p_state->red_second_from_handoff = true;
        p_state->status.red_handoff_armed = true;
    }
    ready_for_next_sample = !p_state->red_second_from_handoff ||
        p_input->rgb_sequence != p_state->red_handoff_sequence;
    mix_after_handoff = p_state->red_second_armed && p_state->red_second_from_handoff &&
        ready_for_next_sample && p_input->red_handoff_mix;
#endif
    bool outside_edge_after_handoff = false;
#if FOLLOW_RED_FIRST_BEND_BOOST
    /* Sustained outside IR confirms the turn even if the fading red frame
     * is still the one that closed the handoff. Require clear inside IR. */
    outside_edge_after_handoff = p_state->red_second_from_handoff && outside_red_fade;
#endif
    uint16_t second_left_max = 80U;
#if FOLLOW_BEND_CUE_REFINEMENT
    /* After the first pivot and solid red, a fresh fade can arrive slightly
     * left of centre on the diagonal. Keep the inside sensor clear. */
    if (p_state->status.red_first_pivot_used && p_input->red_exit_fade)
    {
        second_left_max = 320U;
    }
#endif
    second_ready = p_state->red_second_armed &&
        (ready_for_next_sample || outside_edge_after_handoff) &&
        (!p_input->rgb_join_active || mix_after_handoff) &&
        p_input->now_ms - p_state->red_recovered_ms < 400U &&
        (p_state->status.left_norm <= second_left_max || outside_edge_after_handoff) &&
        p_state->status.right_norm <= 80U;
    fading = fading || (second_ready && mix_after_handoff);
#if FOLLOW_RED_BETWEEN_BENDS
#if FOLLOW_RED_DIAGONAL_REACQUIRE
    const uint32_t diagonal_age = p_input->now_ms - p_state->red_first_pivot_ms;
#else
    const uint32_t diagonal_age = first_turn_age;
#endif
    if (red_between_bends_fade(p_state, p_input, diagonal_age))
    {
        p_state->red_exit_lead_seen = false;
        p_state->red_second_from_handoff = false;
        p_state->status.red_clear_pivot_used = true;
        second_ready = true;
        fading = true;
    }
#endif
#endif
    bool turn_ready = (second_ready && fading) || outside_red_fade;
#if FOLLOW_RED_HANDOFF_ALIGN
    turn_ready = turn_ready && !red_handoff_aligning(p_state, p_input);
#endif
    if (!p_input->red_surface || (p_input->rgb_target_matched && !p_input->red_exit_fade))
    {
        p_state->red_exit_lead_seen = false;
#if FOLLOW_RED_SECOND_PIVOT
        p_state->red_second_active = false;
#endif
#if FOLLOW_RED_FIRST_BEND_BOOST
        p_state->red_first_active = false;
#endif
    }
    else if (!p_state->red_exit_lead_seen && turn_ready)
    {
        p_state->red_exit_lead_seen = true;
        p_state->red_exit_lead_ms = p_input->now_ms;
#if FOLLOW_RED_SECOND_PIVOT
        p_state->red_second_active = second_ready;
        p_state->red_second_armed = false;
#endif
#if FOLLOW_RED_FIRST_BEND_BOOST
        p_state->red_first_active = second_ready && p_state->red_second_from_handoff;
#if FOLLOW_RED_FIRST_BEND_TIGHTER
        /* The first red cue can also occur when starting directly on red.
         * Confirm it with a matched fade and clear inside IR. */
        if (!p_state->status.red_handoff_armed && !p_state->status.red_exit_lead_used &&
            outside_red_fade && p_input->rgb_target_matched && !p_input->rgb_join_active &&
            p_state->status.right_norm <= 80U)
        {
            p_state->red_first_active = true;
        }
#endif
#endif
    }
    return p_state->red_exit_lead_seen && p_input->now_ms - p_state->red_exit_lead_ms < red_exit_hold_ms;
}
#endif

black_follow_output_t black_follow_step(black_follow_t *p_state,
                                        const black_follow_input_t *p_input)
{
    black_follow_output_t output = {0};
    bool pressed = fresh_press(p_state, p_input);
    bool straight_mode = false;
#if AAI05_FOLLOW_MQTT
    pressed = p_input->remote_start;
    straight_mode = p_input->straight_mode;
#endif
    bool ir_ok = p_input->has_ir && p_input->ir_valid &&
        (p_input->left_raw <= 4095U) && (p_input->right_raw <= 4095U) &&
        ((uint32_t)(p_input->now_ms - p_input->ir_ms) <= FOLLOW_IR_FRESH_MS);
#if FOLLOW_BORDERED_CORRIDOR
    /* Each black border reaches full scale at 500. Use interior readings
     * measured for the current tape to zero the two sensors. */
    bool blue_surface = FOLLOW_BLUE_CORRIDOR ||
                        (FOLLOW_MULTICOLOUR_CORRIDOR && p_input->blue_surface);
#if FOLLOW_HOME_TRACK
    uint16_t left_centre = blue_surface ? 339U : 255U;
    uint16_t right_centre = blue_surface ? 313U : 236U;
    if (p_input->yellow_surface)
    {
        right_centre = 278U;
    }
#else
    uint16_t left_centre = blue_surface ? 331U : 255U;
    uint16_t right_centre = blue_surface ? 315U : 236U;
#endif
    p_state->status.left_norm = ir_ok ?
        normalize(p_input->left_raw, left_centre, (uint16_t)(500U - left_centre)) : 0U;
    p_state->status.right_norm = ir_ok ?
        normalize(p_input->right_raw, right_centre, (uint16_t)(500U - right_centre)) : 0U;
    uint16_t ambiguity_threshold = 500U;
#if AAI05_FOLLOW_MQTT
    if (blue_surface && p_input->rgb_target_matched && !p_input->rgb_join_active)
    {
        /* Blue can reach the 600s on small offsets. Keep the original
         * normalization for bend cues and use raw IR for the stop guard. */
        ambiguity_threshold = 700U;
    }
#endif
    bool ambiguous = ir_ok && (p_input->left_raw >= ambiguity_threshold) &&
                     (p_input->right_raw >= ambiguity_threshold);
#else
    p_state->status.left_norm = ir_ok ? normalize(p_input->left_raw, 268U, 1100U) : 0U;
    p_state->status.right_norm = ir_ok ? normalize(p_input->right_raw, 304U, 1140U) : 0U;
    bool ambiguous = (p_state->status.left_norm >= 600U) &&
                     (p_state->status.right_norm >= 600U);
#endif
    bool corner_turning = false;
    bool allow_corner_cue = false;
#if FOLLOW_CORNER_DIRECTION != 0
    corner_turning = !straight_mode && p_state->status.corner_phase == FOLLOW_CORNER_TURN;
    allow_corner_cue = !straight_mode && ((p_state->status.phase != FOLLOW_RUNNING) ||
                       (p_state->status.corner_phase != FOLLOW_CORNER_EXIT));
#endif
    bool ambiguous_stop = ambiguous && !allow_corner_cue;
    p_state->status.ready = ir_ok && p_input->rgb_target_matched &&
                           p_input->hardware_ready && !ambiguous_stop &&
                           !p_input->stop_pressed;
#if FOLLOW_TWO_TURN
    if (!straight_mode && p_state->status.phase != FOLLOW_RUNNING)
    {
        p_state->status.ready = p_state->status.ready && bend_centred(p_state);
    }
#endif
    if (p_input->stop_pressed)
    {
        black_follow_abort(p_state, FOLLOW_BUTTON);
        return output;
    }
    if (p_state->status.phase == FOLLOW_RUNNING)
    {
        uint32_t elapsed = p_input->now_ms - p_state->run_started_ms;
        p_state->status.elapsed_ms = (uint16_t)(elapsed > run_limit_ms() ?
                                               run_limit_ms() : elapsed);
        bool rgb_ok = corner_turning ? p_input->rgb_healthy :
                      (p_input->rgb_target_matched || p_input->rgb_join_active);
        uint8_t deadline_reason;
        uint32_t remaining = black_follow_remaining_ms(p_state, p_input->now_ms, &deadline_reason);
        uint8_t reason = !p_input->hardware_ready ? FOLLOW_HARDWARE :
                         !ir_ok ? FOLLOW_IR :
                         p_input->rgb_join_timed_out ? FOLLOW_JOIN_TIMEOUT :
                         !rgb_ok ? FOLLOW_RGB :
                         ambiguous_stop ? FOLLOW_AMBIGUOUS :
                         remaining == 0U ? deadline_reason : FOLLOW_NONE;
        if (reason != FOLLOW_NONE)
        {
            black_follow_abort(p_state, reason);
            return output;
        }
    }
    else if (pressed && p_state->status.ready)
    {
        p_state->status.phase = FOLLOW_RUNNING;
        p_state->status.reason = FOLLOW_NONE;
        p_state->status.elapsed_ms = 0U;
        p_state->run_started_ms = p_input->now_ms;
#if AAI05_FOLLOW_MQTT
        p_state->straight_mode = straight_mode;
#endif
#if FOLLOW_BLUE_STEER_RAMP
        p_state->blue_edge_direction = 0;
#endif
#if FOLLOW_BLUE_SECOND_BOOST
        p_state->blue_second_armed = false;
        p_state->status.blue_second_used = false;
#endif
#if FOLLOW_BLUE_OUTER_EDGE
        p_state->blue_outer_seen = false;
        p_state->status.blue_outer_used = false;
#endif
#if FOLLOW_BEND_CUE_REFINEMENT
        p_state->blue_outside_sample_seen = false;
#endif
#if FOLLOW_BLUE_OUTER_RECOVERY
        p_state->status.blue_outer_recovery_used = false;
#endif
#if FOLLOW_BLUE_BEND_SLOW
        p_state->blue_outer_confirmed = false;
#if FOLLOW_BLUE_EXIT_EDGE_CONFIRM
        p_state->blue_outer_confirmed_ms = 0U;
#endif
        p_state->status.blue_bend_slow_used = false;
#endif
#if FOLLOW_BLUE_EXIT_LEAD
        p_state->blue_exit_lead_seen = false;
        p_state->status.blue_exit_lead_used = false;
#endif
#if FOLLOW_RED_OUTSIDE_RECOVERY
        p_state->red_outside_active = false;
        p_state->red_left_edge_seen = false;
        p_state->status.red_outside_used = false;
#endif
#if FOLLOW_BLUE_EXIT_AFTER_RECOVERY
        p_state->status.blue_recovered_exit_used = false;
#endif
#if FOLLOW_RED_EXIT_LEAD
        p_state->red_outer_seen = false;
        p_state->red_outer_confirmed = false;
        p_state->red_exit_lead_seen = false;
        p_state->status.red_exit_lead_used = false;
#endif
#if FOLLOW_RED_SECOND_PIVOT
        p_state->red_second_armed = false;
        p_state->red_second_active = false;
        p_state->status.red_second_pivot_used = false;
#endif
#if FOLLOW_RED_SECOND_SLOW
        p_state->status.red_second_slow_used = false;
#endif
#if FOLLOW_RED_BEND_AFTER_HANDOFF
        p_state->red_second_from_handoff = false;
        p_state->status.red_handoff_armed = false;
#if FOLLOW_RED_FIRST_BEND_BOOST
        p_state->red_first_active = false;
        p_state->status.red_first_pivot_used = false;
#endif
#endif
#if FOLLOW_RED_HANDOFF_ALIGN
        p_state->status.red_alignment_used = false;
#endif
#if FOLLOW_RED_EXIT_ALIGN
        p_state->status.red_exit_alignment_used = false;
#endif
#if FOLLOW_RED_BETWEEN_BENDS
        p_state->red_between_bends_peak_clear = 0;
        p_state->status.red_clear_pivot_used = false;
#endif
#if FOLLOW_RED_INSIDE_RELEASE
        p_state->status.red_second_release_used = false;
#endif
#if FOLLOW_RED_EXIT_TURN
        p_state->status.red_exit_turn_used = false;
#endif
#if FOLLOW_BLUE_RED_CONFIRM_GRACE
        p_state->status.red_approach_grace_used = false;
#endif
#if FOLLOW_RED_RETURN_GRACE
        p_state->status.red_return_grace_used = false;
#endif
#if FOLLOW_BLUE_REVERSAL_DAMPING
        p_state->blue_last_edge_direction = 0;
        p_state->status.blue_reversal_damped = false;
#endif
#if FOLLOW_YELLOW_LEAD_CUE
        p_state->yellow_lead_seen = false;
        p_state->status.yellow_lead_used = false;
#endif
#if FOLLOW_LOOP_RECOVERY_DIRECTION != 0
        p_state->bend_recovering = false;
#endif
#if FOLLOW_TWO_TURN || FOLLOW_CORNER_DIRECTION != 0
        p_state->status.corner_phase = FOLLOW_CORNER_SEEK;
        p_state->corner_stage_started_ms = p_input->now_ms;
        p_state->corner_signal_seen = false;
#endif
        output.start = true;
    }
    if (p_state->status.phase == FOLLOW_RUNNING)
    {
#if FOLLOW_TWO_TURN
        if (!straight_mode)
        {
            update_two_turn_bend(p_state, p_input);
        }
        if (!straight_mode && bend_turning(p_state->status.corner_phase))
        {
            p_state->blue_edge_direction = 0;
            p_state->status.turn_pct = 18;
            output.drive = true;
            output.forward_pct = 35;
            output.turn_pct = p_state->status.turn_pct;
            return output;
        }
#elif FOLLOW_CORNER_DIRECTION != 0
        if (!straight_mode)
        {
            update_corner(p_state, p_input, ambiguous);
        }
        if (!straight_mode && p_state->status.corner_phase == FOLLOW_CORNER_TURN)
        {
            p_state->status.turn_pct = 0;
            output.drive = true;
            output.pivot = true;
            return output;
        }
#endif
#if FOLLOW_BORDERED_CORRIDOR
        int32_t error = (int32_t)p_state->status.left_norm - p_state->status.right_norm;
#else
        int32_t error = (int32_t)p_state->status.right_norm - p_state->status.left_norm;
#endif
        int32_t steering_error = error;
#if FOLLOW_BLUE_STEER_WIDE_RANGE
        if (blue_surface && p_input->rgb_target_matched && !p_input->rgb_join_active)
        {
            /* Reserve full blue steering for 700. Bend cues still use the
             * separate normalization that reaches its maximum at 500. */
            const uint16_t blue_steer_high = 700U;
            uint16_t left_steer = normalize(p_input->left_raw, left_centre,
                                            (uint16_t)(blue_steer_high - left_centre));
            uint16_t right_steer = normalize(p_input->right_raw, right_centre,
                                             (uint16_t)(blue_steer_high - right_centre));
            steering_error = (int32_t)left_steer - right_steer;
        }
#endif
        int32_t turn_limit = FOLLOW_TURN_LIMIT_PCT;
#if FOLLOW_CLIPPED_LOOP
        /* Keep gentle centring, then increase differential as one border
         * dominates. The chassis remains forward-only through the bends. */
        int32_t magnitude = steering_error < 0 ? -steering_error : steering_error;
        if (!straight_mode && magnitude > 600)
        {
            turn_limit += (magnitude - 600) *
                          (FOLLOW_TURN_PEAK_PCT - FOLLOW_TURN_LIMIT_PCT) / 400;
        }
#endif
        const int32_t steering_deadband = 80;
        int32_t turn = ((steering_error >= -steering_deadband) &&
                        (steering_error <= steering_deadband)) ? 0 :
                       steering_error * turn_limit / 1000;
        output.drive = true;
        output.forward_pct = (allow_corner_cue && ambiguous) ? 25 : 50;
        if (straight_mode)
        {
            output.turn_pct = (int8_t)turn;
            p_state->status.turn_pct = output.turn_pct;
            return output;
        }
#if FOLLOW_EARLY_BEND
        /* The left border is outside on a clockwise lap. Reduce forward
         * drive as this edge dominates, while the colour is still confirmed. */
        const int32_t outside_slow_threshold = 400;
        const int32_t max_forward_reduction = 20;
        if (p_input->rgb_target_matched && !p_input->rgb_join_active &&
            (error > outside_slow_threshold))
        {
            output.forward_pct = (int8_t)(output.forward_pct -
                (error - outside_slow_threshold) * max_forward_reduction /
                (1000 - outside_slow_threshold));
        }
#endif
#if FOLLOW_LOOP_RECOVERY_DIRECTION != 0 && !FOLLOW_TWO_TURN
#if FOLLOW_BLUE_SECOND_BOOST
        if (!blue_surface || p_input->rgb_join_active)
        {
            p_state->blue_second_armed = false;
        }
        else if (p_state->bend_recovering && p_input->rgb_target_matched)
        {
            /* Two blue matches closed a rightward recovery. For the next
             * half-second, a strong outside edge may be the second bend. */
            p_state->blue_reacquired_ms = p_input->now_ms;
            p_state->blue_second_armed = true;
        }
#endif
        if (!p_input->rgb_join_active)
        {
            p_state->bend_recovering = false;
        }
        else if ((p_state->status.left_norm <= steering_deadband) &&
                 (p_state->status.right_norm <= steering_deadband))
        {
#if FOLLOW_BLUE_RED_HANDOFF
            if (p_input->blue_red_blend)
            {
                p_state->bend_recovering = false;
                p_state->status.blue_red_join_used = true;
                turn = 0;
#if FOLLOW_BLUE_DIAGONAL_EXIT
                if (p_state->status.blue_exit_lead_used &&
                    p_input->now_ms - p_state->blue_exit_lead_ms < 650U)
                {
                    output.forward_pct = 30;
                    turn = 10;
                }
#endif
            }
            else
#endif
            if (!p_input->rgb_target_matched &&
                (p_input->bend_loss_rgb || p_state->bend_recovering))
            {
                p_state->bend_recovering = true;
                /* Drive the outside wheel at the cap with the inside wheel off. */
                output.forward_pct = 25;
                turn = FOLLOW_LOOP_RECOVERY_DIRECTION * output.forward_pct;
            }
#if FOLLOW_YELLOW_EXIT_CUE
            else if (p_input->yellow_exit_cue)
            {
                /* The earlier cue calls for an arc while both edges are weak.
                 * A later BACKGROUND sample can still request tight recovery. */
                output.forward_pct = 35;
                turn = FOLLOW_LOOP_RECOVERY_DIRECTION * FOLLOW_TURN_PEAK_PCT;
            }
#endif
            else if (p_state->bend_recovering && p_input->rgb_same_lane)
            {
                /* A first reading of the same tape may only be a glimpse.
                 * Keep a gentle turn until the owner confirms the handoff. */
                output.forward_pct = 30;
                turn = FOLLOW_LOOP_RECOVERY_DIRECTION * FOLLOW_TURN_LIMIT_PCT;
            }
        }
#if FOLLOW_BLUE_OUTER_RECOVERY
        else if (blue_surface && p_state->bend_recovering &&
                 !p_input->rgb_target_matched && !p_input->blue_red_blend &&
                 p_input->left_raw >= 700U && p_state->status.right_norm == 0U)
        {
            /* The outside edge is back, but blue has not returned. Keep
             * the left wheel at its cap and let the right roll slowly. */
            output.forward_pct = 35;
            turn = 18;
            p_state->status.blue_outer_recovery_used = true;
        }
#endif
#elif FOLLOW_BLUE_BEND_RIGHT
        /* The trial is clockwise at the all-blue bend. Use the existing
         * join deadline when the colour is lost and both edge cues are weak. */
        if (blue_surface && p_input->rgb_join_active && !p_input->rgb_target_matched &&
            (p_state->status.left_norm <= steering_deadband) &&
            (p_state->status.right_norm <= steering_deadband))
        {
            /* The chassis mixer stops the inner wheel and drives the outer wheel. */
            output.forward_pct = 25;
            turn = output.forward_pct;
        }
#endif
#if FOLLOW_YELLOW_LEAD_CUE
        if (!p_input->yellow_lead_cue)
        {
            p_state->yellow_lead_seen = false;
        }
        else if (!p_input->rgb_join_active &&
                 (p_state->status.left_norm <= steering_deadband) &&
                 (p_state->status.right_norm <= steering_deadband))
        {
            if (!p_state->yellow_lead_seen)
            {
                p_state->yellow_lead_since_ms = p_input->now_ms;
                p_state->yellow_lead_seen = true;
            }
            if (p_input->now_ms - p_state->yellow_lead_since_ms < 180U)
            {
                output.forward_pct = 35;
                turn = FOLLOW_LOOP_RECOVERY_DIRECTION * 10;
                p_state->status.yellow_lead_used = true;
            }
        }
#endif
#if FOLLOW_BLUE_STEER_RAMP
        turn = ramp_blue_edge_turn(p_state, p_input, blue_surface, turn);
#endif
#if FOLLOW_BLUE_OUTER_EDGE
        turn = blue_outer_edge_turn(p_state, p_input, blue_surface, turn);
#endif
#if FOLLOW_BLUE_BEND_SLOW
        const uint32_t blue_slow_ms = 300U;
        /* The late blue fade can follow the last outside edge by about 400 ms. */
        const uint32_t blue_edge_memory_ms = FOLLOW_BLUE_RED_JOIN_REFINEMENT ? 450U : blue_slow_ms;
        const uint32_t blue_edge_age = p_input->now_ms - p_state->blue_outer_confirmed_ms;
        if (!blue_surface || !p_input->rgb_target_matched || p_input->rgb_join_active ||
            blue_edge_age >= blue_edge_memory_ms)
        {
            p_state->blue_outer_confirmed = false;
        }
        if (p_state->blue_outer_confirmed && blue_edge_age < blue_slow_ms &&
            output.forward_pct > 40)
        {
            /* Limit straight duty to 22% briefly after the outside edge. */
            output.forward_pct = 40;
            p_state->status.blue_bend_slow_used = true;
        }
#endif
#if FOLLOW_BLUE_SECOND_BOOST
        if (p_state->blue_second_armed &&
            p_input->now_ms - p_state->blue_reacquired_ms < 500U &&
            p_input->left_raw >= 700U && p_input->right_raw <= right_centre &&
            turn < 12)
        {
            turn = 12;
            p_state->status.blue_second_used = true;
        }
#endif
#if FOLLOW_BLUE_EXIT_LEAD
        const int8_t blue_exit_turn = FOLLOW_BLUE_RED_BEND_CUES ? 25 : FOLLOW_BLUE_EXIT_TURN;
        if (blue_exit_lead(p_state, p_input, blue_surface) && turn < blue_exit_turn)
        {
            output.forward_pct = FOLLOW_BLUE_RED_BEND_CUES ? 25 : 35;
            turn = blue_exit_turn;
            p_state->status.blue_exit_lead_used = true;
#if FOLLOW_BLUE_EXIT_PIVOT
            /* Add yaw at the qualified bend while RGB still matches blue. */
            output.blue_exit_pivot = p_input->rgb_target_matched && !p_input->rgb_join_active &&
                p_input->now_ms - p_state->blue_exit_lead_ms < FOLLOW_BLUE_EXIT_PIVOT_MS;
#if FOLLOW_BLUE_SECOND_LEFT_BOOST
            /* The second bend crosses from the left edge to the right while blue fades. */
            output.blue_second_left_boost = output.blue_exit_pivot && p_input->blue_inner_fade &&
                p_state->status.left_norm <= 80U && p_state->status.right_norm >= 240U &&
                p_state->blue_outside_sample_seen && p_input->ir_ms - p_state->blue_outside_sample_ms <= 180U;
#endif
#endif
        }
#endif
#if FOLLOW_BLUE_RED_CONFIRM_GRACE
        if (p_input->red_approach_grace)
        {
            p_state->status.red_approach_grace_used = true;
            if (p_state->status.left_norm <= steering_deadband &&
                p_state->status.right_norm <= steering_deadband)
            {
                output.forward_pct = 30;
                turn = p_input->rgb_target_matched ? 0 : 10;
            }
        }
#endif
#if FOLLOW_RED_EXIT_LEAD
        const int8_t red_exit_turn = FOLLOW_BLUE_RED_BEND_TRIAL ? 25 : 22;
        bool red_leading = red_exit_lead(p_state, p_input);
#if FOLLOW_RED_SECOND_SLOW
        if (p_input->red_surface && p_input->rgb_target_matched && !p_input->rgb_join_active &&
            p_state->red_second_armed && p_input->now_ms - p_state->red_recovered_ms < 400U &&
            output.forward_pct > 40)
        {
            output.forward_pct = 40;
            p_state->status.red_second_slow_used = true;
        }
#endif
#if FOLLOW_RED_HANDOFF_ALIGN
        if (red_handoff_aligning(p_state, p_input))
        {
            output.forward_pct = 35;
            turn = 5;
            p_state->status.red_alignment_used = true;
        }
#endif
        if (red_leading && turn < red_exit_turn)
        {
            output.forward_pct = FOLLOW_BLUE_RED_BEND_TRIAL ? 25 : 35;
            turn = red_exit_turn;
            p_state->status.red_exit_lead_used = true;
        }
#if FOLLOW_RED_SECOND_PIVOT
        bool red_pivot_active = p_state->red_second_active;
#if FOLLOW_RED_FIRST_BEND_BOOST
        red_pivot_active = red_pivot_active || p_state->red_first_active;
#endif
        if (red_leading && red_pivot_active &&
            p_input->now_ms - p_state->red_exit_lead_ms < FOLLOW_RED_SECOND_PIVOT_MS)
        {
            output.pivot = true;
#if FOLLOW_RED_FIRST_BEND_BOOST
            if (p_state->red_first_active)
            {
#if FOLLOW_RED_DIAGONAL_REACQUIRE
                if (!p_state->status.red_first_pivot_used)
                {
                    p_state->red_first_pivot_ms = p_state->red_exit_lead_ms;
                    p_state->red_diagonal_inside_seen = false;
                }
#endif
                p_state->status.red_first_pivot_used = true;
            }
            else
#endif
            {
#if FOLLOW_RED_EXIT_ALIGN
                if (!p_state->status.red_second_pivot_used)
                {
                    p_state->red_second_pivot_ms = p_state->red_exit_lead_ms;
                }
#endif
                p_state->status.red_second_pivot_used = true;
            }
#if FOLLOW_RED_SECOND_SLOW
            p_state->status.red_second_slow_used = true;
#endif
        }
#if FOLLOW_RED_SECOND_SLOW
        else if (red_leading && red_pivot_active)
        {
            output.forward_pct = 20;
            turn = 20;
#if FOLLOW_RED_SECOND_ARC_BOOST
            /* Strengthen the bounded forward arc after the second pivot. */
            if (p_state->red_second_active && p_state->status.red_second_pivot_used &&
                !p_state->red_first_active && p_state->status.right_norm <= 240U)
            {
                output.red_second_exit_arc = true;
                output.forward_pct = 25;
                turn = 25;
            }
#endif
        }
#endif
#endif
#if FOLLOW_RED_EXIT_ALIGN
        bool exit_aligning = !red_leading && red_second_exit_aligning(p_state, p_input);
#if FOLLOW_RED_EXIT_TURN
        if (!red_leading && (FOLLOW_RED_EXIT_TURN_EARLY || !exit_aligning) &&
            red_late_exit_turn(p_state, p_input))
        {
            output.forward_pct = FOLLOW_RED_EXIT_TURN_TIGHTER ? 30 : 35;
            turn = FOLLOW_RED_EXIT_TURN_STRONG ? 20 : (FOLLOW_RED_EXIT_TURN_TIGHTER ? 15 : 10);
#if FOLLOW_RED_FINAL_LEFT_BOOST
            output.red_final_arc = true;
#endif
#if FOLLOW_RED_FINAL_ARC_EXTEND
            output.red_final_arc_extended =
                p_input->now_ms - p_state->red_exit_turn_started_ms >= red_exit_arc_hold_ms;
#endif
        }
        else
#endif
        if (exit_aligning)
        {
            output.forward_pct = 35;
            turn = 5;
            p_state->status.red_exit_alignment_used = true;
        }
#endif
#endif
#if FOLLOW_RED_RETURN_GRACE
        if (p_input->red_return_grace)
        {
            output.forward_pct = 35;
            turn = 5;
            p_state->status.red_return_grace_used = true;
#if FOLLOW_RED_SECOND_ARC_BOOST
            output.red_second_exit_arc = false;
#endif
#if FOLLOW_RED_FINAL_LEFT_BOOST
            output.red_final_arc = false;
#endif
#if FOLLOW_RED_FINAL_ARC_EXTEND
            output.red_final_arc_extended = false;
#endif
        }
#endif
#if FOLLOW_RED_OUTSIDE_RECOVERY
        bool red_spin_right = black_follow_red_outside_update(p_state, p_input, output.pivot);
#if FOLLOW_RED_RIGHT_EDGE_OUTSIDE
        if (!red_spin_right && black_follow_red_right_edge_outside(p_state, p_input))
        {
            red_spin_right = true;
#if FOLLOW_RED_SPIN_HOLD
            black_follow_red_spin_start(p_state, p_input);
#endif
        }
#endif
        if (red_spin_right)
        {
            /* Same right spin as the second red pivot (owner duties). */
            output.pivot = true;
            output.red_outside_spin = true;
            output.forward_pct = 25;
            turn = 25;
            output.blue_exit_pivot = false;
            output.blue_second_left_boost = false;
            output.red_second_exit_arc = false;
            output.red_final_arc = false;
            output.red_final_arc_extended = false;
        }
#endif
        p_state->status.turn_pct = (int8_t)turn;
        output.turn_pct = (int8_t)turn;
    }
    return output;
}

const char *black_follow_phase_name(uint8_t phase)
{
    static const char *const names[3] = {"IDLE", "RUNNING", "STOPPED"};
    return phase < 3U ? names[phase] : "INVALID";
}

const char *black_follow_reason_name(uint8_t reason)
{
    static const char *const names[] = {
        "NONE", "BUTTON", "IR", "RGB", "HARDWARE", "AMBIGUOUS", "COMPLETE", "DRIVER",
        "JOIN_TIMEOUT", "CORNER_NOT_SEEN", "CORNER_TIMEOUT", "REFRESH",
        "MQTT_STOP", "MQTT_TIMEOUT"
    };
    return reason < sizeof(names) / sizeof(names[0]) ? names[reason] : "INVALID";
}

const char *black_follow_corner_name(uint8_t phase)
{
#if FOLLOW_TWO_TURN
    static const char *const names[] = {"SEEK", "TURN_1", "DIAGONAL", "TURN_2", "EXIT"};
#else
    static const char *const names[] = {"SEEK", "TURN", "EXIT"};
#endif
    return phase < sizeof(names) / sizeof(names[0]) ? names[phase] : "INVALID";
}
