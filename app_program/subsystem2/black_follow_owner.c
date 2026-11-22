#include "black_follow_owner.h"
#if AAI05_FOLLOW_APP
#include "motion_chassis.h"
#include "../subsystem1/rgb_diag.h"
#include "../subsystem1/rgb_classifier.h"
#include "../../config/follow_config.h"
#if AAI05_FOLLOW_MQTT
#include "follow_remote.h"
#endif

static void owner_stop(black_follow_owner_t *p_owner, uint8_t reason)
{
    black_follow_abort(&p_owner->policy, reason);
    motion_control_stop(&p_owner->motion, reason == FOLLOW_BUTTON ?
                           MOTION_REASON_BUTTON : MOTION_REASON_REQUEST);
    motor_pulse_stop();
}

#if FOLLOW_MOTOR_HALF_PCT
static int32_t apply_fractional_drive(black_follow_status_t *p_status,
                                      motion_output_t *p_output, int32_t left_limit_half_pct,
                                      uint32_t deadline_us)
{
    /* The chassis mixer is on a 0..50 scale; defer rounding to the PWM path. */
    int8_t left = (int8_t)(p_output->left_pct * left_limit_half_pct / 50);
    int8_t right = (int8_t)(p_output->right_pct * FOLLOW_DUTY_LIMIT_HALF_PCT / 50);
    int32_t result = motor_drive_set_until_half_pct(left, right, deadline_us);
    if (result == 0)
    {
        p_status->left_half_pct = left;
        p_status->right_half_pct = right;
        p_output->left_pct = (int8_t)(left / 2);
        p_output->right_pct = (int8_t)(right / 2);
    }
    return result;
}
#endif

#if FOLLOW_MULTICOLOUR_CORRIDOR
#if FOLLOW_RED_BEND_AFTER_HANDOFF
static bool red_mix_reading(const rgb_snapshot_t *p_rgb, rgb_classification_t colour,
                            uint32_t red_fraction_min)
{
    /* Red-leaning no-match frames have nearly equal green and blue. */
    uint32_t sum = (uint32_t)p_rgb->red + p_rgb->green + p_rgb->blue;
    uint32_t red = (uint32_t)p_rgb->red * RGB_FRACTION_MAX;
    return rgb_classification_reason(colour) == RGB_CLASS_NO_MATCH && sum != 0U &&
           p_rgb->clear >= 600U && p_rgb->clear <= 670U &&
           red >= red_fraction_min * sum && red < 1574U * sum &&
           p_rgb->green >= p_rgb->blue && p_rgb->green <= p_rgb->blue + 8U;
}
#endif
#if FOLLOW_RED_EXIT_LEAD
static bool red_exit_reading(const rgb_snapshot_t *p_rgb)
{
    /* The red bend and both handoff runs lost red saturation before leaving tape. */
    const uint32_t red_fraction_max = FOLLOW_BLUE_RED_BEND_CUES ? 1626U :
        (FOLLOW_RED_EXIT_EARLY ? 1618U : 1612U);
    const uint16_t clear_max = FOLLOW_BLUE_RED_BEND_CUES ? 670U : 650U;
    uint32_t sum = (uint32_t)p_rgb->red + p_rgb->green + p_rgb->blue;
    return p_rgb->clear >= 590U && p_rgb->clear <= clear_max && sum != 0U &&
           (uint32_t)p_rgb->red * RGB_FRACTION_MAX <= red_fraction_max * sum;
}
#endif
#if FOLLOW_BLUE_EXIT_LEAD
static bool blue_exit_reading(const rgb_snapshot_t *p_rgb, uint16_t clear_max,
                              uint32_t red_fraction_min, uint32_t blue_fraction_max)
{
    /* Both bend traces lost blue saturation one frame before UNKNOWN. */
    uint32_t sum = (uint32_t)p_rgb->red + p_rgb->green + p_rgb->blue;
    return p_rgb->clear >= 520U && p_rgb->clear <= clear_max && sum != 0U &&
           (uint32_t)p_rgb->red * RGB_FRACTION_MAX >= red_fraction_min * sum &&
           (uint32_t)p_rgb->blue * RGB_FRACTION_MAX <= blue_fraction_max * sum;
}
#endif
#if FOLLOW_BLUE_RED_HANDOFF
static bool blue_red_blend_reading(const rgb_snapshot_t *p_rgb)
{
    /* Window fitted to the two hand-guided blue-to-red joins on the home track. */
    uint32_t sum = (uint32_t)p_rgb->red + p_rgb->green + p_rgb->blue;
    uint32_t red = (uint32_t)p_rgb->red * RGB_FRACTION_MAX;
    uint32_t green = (uint32_t)p_rgb->green * RGB_FRACTION_MAX;

    return p_rgb->clear >= 570U && p_rgb->clear <= 700U &&
           sum != 0U && p_rgb->blue >= p_rgb->green &&
           red >= 1260U * sum && red <= 1580U * sum &&
           green >= 1240U * sum && green <= 1371U * sum;
}
#endif
#if FOLLOW_YELLOW_EXIT_CUE
static bool yellow_approach_reading(const rgb_snapshot_t *p_rgb, rgb_classification_t colour)
{
    uint32_t sum = (uint32_t)p_rgb->red + p_rgb->green + p_rgb->blue;
    if (sum == 0U)
    {
        return false;
    }
    uint32_t red = (uint32_t)p_rgb->red * RGB_FRACTION_MAX;
    uint32_t blue = (uint32_t)p_rgb->blue * RGB_FRACTION_MAX;
#if FOLLOW_YELLOW_LEAD_CUE
    /* Three approaches showed this yellow match immediately before a loss. */
    if (colour == RGB_CLASS_YELLOW)
    {
        return (p_rgb->clear >= 740U) && (p_rgb->clear <= 790U) &&
               (red >= 1525U * sum) && (red <= 1550U * sum) &&
               (blue >= 1140U * sum) && (blue <= 1177U * sum);
    }
    const uint32_t red_max = 1520U;
    const uint32_t blue_min = 1175U;
#else
    const uint32_t red_max = 1505U;
    const uint32_t blue_min = 1190U;
#endif
    /* Measured yellow-to-cardboard readings before BACKGROUND. The recorded
     * yellow-to-blue straight join lay outside these bounds. */
    if (rgb_classification_reason(colour) != RGB_CLASS_NO_MATCH)
    {
        return false;
    }
    return (p_rgb->clear >= 700U) && (p_rgb->clear <= 765U) &&
           (red >= 1470U * sum) && (red <= red_max * sum) &&
           (blue >= blue_min * sum) && (blue <= 1220U * sum);
}
#endif

static bool join_reading(rgb_classification_t colour, const rgb_snapshot_t *p_rgb)
{
    /* Supplied join readings span clear594..1026 at the calibrated exposure.
     * A match here permits only a timed IR-guided handoff. */
    uint8_t reason = rgb_classification_reason(colour);
    uint16_t min_clear = 560U;
#if FOLLOW_CLIPPED_LOOP && AAI05_RGB_HOME_PROFILE
    /* Home approaches to blue supplied NO_MATCH at clear526..551. */
    if (reason == RGB_CLASS_NO_MATCH)
    {
        min_clear = 510U;
    }
#endif
    if ((p_rgb->clear < min_clear) || (p_rgb->clear > 1080U) ||
        ((p_rgb->red | p_rgb->green | p_rgb->blue) == 0U))
    {
        return false;
    }
    return (colour == RGB_CLASS_BLACK) || (colour == RGB_CLASS_BACKGROUND) ||
           (reason == RGB_CLASS_NO_MATCH) || (reason == RGB_CLASS_AMBIGUOUS);
}

#if FOLLOW_BLUE_RED_CONFIRM_GRACE
static bool red_approach_reading(const rgb_snapshot_t *p_rgb, rgb_classification_t colour)
{
    /* Powered red approaches separated R while G and B stayed close. */
    const uint32_t blue_over_green = FOLLOW_BLUE_RED_JOIN_REFINEMENT ? 1U : 0U;
    return rgb_classification_reason(colour) == RGB_CLASS_NO_MATCH &&
           p_rgb->clear >= 610U && p_rgb->clear <= 700U &&
           p_rgb->red >= p_rgb->green + 15U && p_rgb->blue <= p_rgb->green + blue_over_green &&
           p_rgb->green <= p_rgb->blue + 8U;
}

static bool red_approach_in_progress(const follow_join_t *p_join, const rgb_snapshot_t *p_rgb,
                                     rgb_classification_t colour)
{
    return p_join->red_approach &&
           (colour == RGB_CLASS_RED || red_approach_reading(p_rgb, colour));
}
#endif

#if FOLLOW_RED_RETURN_GRACE
static const uint32_t red_return_grace_ms = 120U;
#endif

#if FOLLOW_BLUE_RED_LATE_CONFIRM
static const uint32_t blue_red_arm_window_ms = 120U;
/* Leave time for the owner to process the second 100 ms RGB frame. */
static const uint32_t blue_red_confirm_grace_ms = FOLLOW_BLUE_RED_LATE_APPROACH ? 160U : 120U;
#endif

#if FOLLOW_BLUE_RED_BLEND_GRACE
static bool near_red_blend(const rgb_snapshot_t *p_rgb, rgb_classification_t colour)
{
    /* The measured mixed frames are red-dominant with nearly equal green
     * and blue. They only qualify for confirmation time. */
#if FOLLOW_BLUE_RED_BLEND_MARGIN
    uint32_t sum = (uint32_t)p_rgb->red + p_rgb->green + p_rgb->blue;
    const uint32_t red_min = FOLLOW_BLUE_RED_LATE_APPROACH ? 1475U : 1528U;
    return rgb_classification_reason(colour) == RGB_CLASS_NO_MATCH &&
           p_rgb->clear >= 630U && p_rgb->clear <= 675U && sum != 0U &&
           (uint32_t)p_rgb->red * RGB_FRACTION_MAX >= red_min * sum &&
           p_rgb->blue >= p_rgb->green && p_rgb->blue <= p_rgb->green + 6U;
#else
    return rgb_classification_reason(colour) == RGB_CLASS_NO_MATCH &&
           p_rgb->clear >= 640U && p_rgb->clear <= 665U &&
           p_rgb->red >= p_rgb->green + 55U &&
           p_rgb->blue >= p_rgb->green + 2U && p_rgb->blue <= p_rgb->green + 4U;
#endif
}
#endif

#if FOLLOW_BLUE_RED_JOIN_EXTEND
static bool blue_red_extend_latch(const black_follow_owner_t *p_owner, bool was_joining,
                                  uint8_t previous_lane, bool blue_exit_seen)
{
    /* v0.83 fix: v0.82 relied only on the 650 ms blue-exit cue window, which
     * had expired before the test2 join began. The exit pivot flag is reset
     * at every start and is set once per lap, so it marks the blue->red join. */
    return was_joining && (previous_lane == RGB_CLASS_BLUE) &&
        (p_owner->join.blue_red_extend || blue_exit_seen ||
         p_owner->policy.status.blue_exit_lead_used);
}
#endif

static uint32_t join_window_ms(const follow_join_t *p_join, const rgb_snapshot_t *p_rgb,
                               rgb_classification_t colour)
{
#if FOLLOW_RED_RETURN_GRACE
    if (p_join->late_red_return && colour == RGB_CLASS_RED)
    {
        return FOLLOW_JOIN_MAX_MS + red_return_grace_ms;
    }
#endif
#if FOLLOW_BLUE_RED_CONFIRM_GRACE
    if (red_approach_in_progress(p_join, p_rgb, colour))
    {
        return FOLLOW_JOIN_MAX_MS + 420U;
    }
#else
    (void)p_rgb;
#endif
#if FOLLOW_BLUE_RED_LATE_CONFIRM
    bool red_pending = colour == RGB_CLASS_RED && p_join->candidate == RGB_CLASS_RED;
#if FOLLOW_BLUE_RED_BLEND_GRACE
    red_pending = colour == RGB_CLASS_RED || near_red_blend(p_rgb, colour);
#endif
    if (p_join->late_blue_red && red_pending)
    {
        return FOLLOW_JOIN_MAX_MS + blue_red_confirm_grace_ms;
    }
#endif
#if FOLLOW_BLUE_RED_JOIN_EXTEND
    /* v0.82: test5 timed out at 490 ms on 321/258/260 (red tape seen with
     * the border) and read RED one frame later. After the blue exit, keep
     * the join open while the reading is clearly red-leading. Cardboard is
     * excluded by class; IR ambiguity and freshness guards still apply. */
    /* v0.87 fix: colour is a packed class+reason code. UNKNOWN/NO_MATCH is
     * 0x17, never equal to the bare class 7, so v0.82..v0.86 only extended
     * RED frames. Compare the class. */
    if (p_join->blue_red_extend &&
        ((rgb_classification_class(colour) == RGB_CLASS_UNKNOWN) ||
         (colour == RGB_CLASS_RED)) &&
        (p_rgb->red >= p_rgb->green + 40U) && (p_rgb->red >= p_rgb->blue + 40U))
    {
        return FOLLOW_JOIN_MAX_MS + 250U;
    }
#endif
#if FOLLOW_RED_CORNER_REFINE
    /* v0.86: v0.85 test1 read RED again at 3.48 s and timed out at 3.54 s
     * before the second red frame. A red-lane join that sees RED gets one
     * more frame of time; any other reading keeps the normal deadline. */
    if ((p_join->lane == RGB_CLASS_RED) && (colour == RGB_CLASS_RED))
    {
        return FOLLOW_JOIN_MAX_MS + 150U;
    }
#endif
#if FOLLOW_BLUE_CONFIRM_GRACE
    /* A first blue match before the deadline may wait for the next RGB
     * frame. A loss or a different colour ends the extra time immediately. */
    if (p_join->lane == RGB_CLASS_BLUE && p_join->candidate == RGB_CLASS_BLUE &&
        colour == RGB_CLASS_BLUE)
    {
        return FOLLOW_JOIN_MAX_MS + 120U;
    }
#else
    (void)p_join;
    (void)colour;
#endif
    return FOLLOW_JOIN_MAX_MS;
}

static uint8_t update_join(follow_join_t *p_join, const rgb_snapshot_t *p_rgb,
                           uint32_t now, rgb_classification_t colour, bool lane_match)
{
    if (p_join->active && (now - p_join->started_ms >= join_window_ms(p_join, p_rgb, colour)))
    {
        return FOLLOW_JOIN_TIMEOUT;
    }
    if (lane_match)
    {
        if (!p_join->active)
        {
            p_join->lane = colour;
        }
        else if (p_join->candidate != colour)
        {
            p_join->candidate = colour;
            p_join->candidate_sequence = p_rgb->sequence;
        }
        else if (p_join->candidate_sequence != p_rgb->sequence)
        {
            p_join->lane = colour;
            p_join->active = false;
        }
        return FOLLOW_NONE;
    }
    if (!join_reading(colour, p_rgb))
    {
        return FOLLOW_RGB;
    }
    if (!p_join->active)
    {
        p_join->started_ms = p_rgb->sampled_ms;
        p_join->active = true;
#if FOLLOW_BLUE_RED_CONFIRM_GRACE
        p_join->red_approach = false;
#endif
#if FOLLOW_BLUE_RED_JOIN_EXTEND
        p_join->blue_red_extend = false;
#endif
    }
    p_join->candidate = RGB_CLASS_UNKNOWN;
    return FOLLOW_NONE;
}
#endif

void black_follow_owner_init(black_follow_owner_t *p_owner)
{
    *p_owner = (black_follow_owner_t){0};
    black_follow_init(&p_owner->policy, app_now_ms());
    motion_control_init(&p_owner->motion);
}

void black_follow_owner_step(black_follow_owner_t *p_owner, uint32_t buttons,
                              bool hardware_ready,
                              const motor_pulse_status_t *p_motor,
                              black_follow_status_t *p_follow,
                              motion_status_t *p_motion)
{
    ir_snapshot_t ir;
    rgb_snapshot_t rgb;
    if (subsystem1_get_snapshot(&ir) == 0)
    {
        p_owner->ir = ir;
        p_owner->has_ir = true;
    }
    else if (p_owner->ir_busy != UINT32_MAX)
    {
        ++p_owner->ir_busy;
    }
    if (rgb_diag_snapshot(&rgb))
    {
        p_owner->rgb = rgb;
        p_owner->has_rgb = true;
    }
    else if (p_owner->rgb_busy != UINT32_MAX)
    {
        ++p_owner->rgb_busy;
    }
#if AAI05_FOLLOW_MQTT
    follow_remote_permission_t remote = follow_remote_take();
#endif
    uint32_t now = app_now_ms(); /* After copies, never before newer samples. */
#if AAI05_FOLLOW_MQTT
    bool lease_valid = follow_remote_remaining(remote, now) != 0U;
    if (!lease_valid && p_owner->policy.status.phase == FOLLOW_RUNNING &&
        (buttons & (1U << 21U)) != 0U)
    {
        owner_stop(p_owner, remote.armed ? FOLLOW_REMOTE_TIMEOUT : FOLLOW_REMOTE_STOP);
    }
#endif
    rgb_classification_t colour = rgb_classify(rgb_default_calibration(),
        &p_owner->rgb, now, RGB_GAIN, RGB_INTEGRATION_US);
    uint8_t reason = rgb_classification_reason(colour);
    bool rgb_healthy = p_owner->has_rgb && ((reason == RGB_CLASS_MATCH) ||
        (reason == RGB_CLASS_NO_MATCH) || (reason == RGB_CLASS_AMBIGUOUS));
#if FOLLOW_CORNER_DIRECTION != 0
    bool was_pivoting = (p_owner->policy.status.phase == FOLLOW_RUNNING) &&
                        (p_owner->policy.status.corner_phase == FOLLOW_CORNER_TURN);
#endif
    bool blue_surface = (colour == RGB_CLASS_BLUE);
    bool join_active = false;
    bool join_timed_out = false;
#if FOLLOW_MULTICOLOUR_CORRIDOR
    bool colour_allowed = (colour == RGB_CLASS_RED) ||
                          (colour == RGB_CLASS_YELLOW) ||
                          (colour == RGB_CLASS_BLUE);
    bool join_completed = false;
    bool track_join = p_owner->policy.status.phase == FOLLOW_RUNNING;
#if FOLLOW_RED_BEND_AFTER_HANDOFF
    bool red_arrived_after_bend = false;
#endif
#if FOLLOW_RED_SECOND_EXIT_REFINEMENT
    bool red_reacquired_after_join = false;
#endif
#if FOLLOW_CORNER_DIRECTION != 0
    track_join = track_join && !was_pivoting;
#endif
    if (track_join)
    {
        bool was_joining = p_owner->join.active;
        uint8_t previous_lane = p_owner->join.lane;
#if FOLLOW_BLUE_RED_CONFIRM_GRACE
        bool blue_exit_seen = p_owner->policy.blue_exit_lead_seen;
#if FOLLOW_BLUE_RED_JOIN_REFINEMENT
        /* A blend can end steering before the red approach sample arrives. */
        blue_exit_seen = blue_exit_seen || (p_owner->policy.status.blue_exit_lead_used &&
            now - p_owner->policy.blue_exit_lead_ms < 650U);
#endif
#if FOLLOW_BLUE_EXIT_EDGE_CONFIRM
        /* Colour confirmation still gets its bounded grace when the sensor
         * approaches red after a blue edge that did not require a pivot. */
        blue_exit_seen = blue_exit_seen || (p_owner->policy.blue_outer_confirmed_ms != 0U &&
            now - p_owner->policy.blue_outer_confirmed_ms < 650U);
#endif
        if (was_joining && previous_lane == RGB_CLASS_BLUE && blue_exit_seen &&
            now - p_owner->join.started_ms < FOLLOW_JOIN_MAX_MS && p_owner->has_ir &&
            p_owner->ir.valid && p_owner->ir.error == 0 &&
            now - p_owner->ir.sampled_ms <= FOLLOW_IR_FRESH_MS &&
            p_owner->ir.left_raw <= 339U && p_owner->ir.right_raw <= 313U &&
            red_approach_reading(&p_owner->rgb, colour))
        {
            p_owner->join.red_approach = true;
        }
#if FOLLOW_BLUE_RED_LATE_CONFIRM
        /* The sensor may report red one frame before the join expires. Keep
         * the normal two-frame confirmation and give that candidate one more frame. */
        uint32_t blue_red_join_age = now - p_owner->join.started_ms;
        bool first_late_red = blue_exit_seen &&
            blue_red_join_age >= FOLLOW_JOIN_MAX_MS - blue_red_arm_window_ms &&
            blue_red_join_age < FOLLOW_JOIN_MAX_MS;
        bool red_pending = colour == RGB_CLASS_RED;
#if FOLLOW_BLUE_RED_BLEND_GRACE
        bool mixed = near_red_blend(&p_owner->rgb, colour);
        bool ir_clear = p_owner->has_ir && p_owner->ir.valid && p_owner->ir.error == 0 &&
            now - p_owner->ir.sampled_ms <= FOLLOW_IR_FRESH_MS &&
            p_owner->ir.left_raw <= 339U && p_owner->ir.right_raw <= 313U;
        first_late_red = first_late_red && (red_pending || (mixed && ir_clear));
        red_pending = red_pending || mixed;
#endif
        p_owner->join.late_blue_red = was_joining && previous_lane == RGB_CLASS_BLUE &&
            red_pending && (p_owner->join.late_blue_red || first_late_red);
#endif
#if FOLLOW_BLUE_RED_JOIN_EXTEND
        /* Latched for this join once the blue exit cue or pivot was seen. */
        p_owner->join.blue_red_extend =
            blue_red_extend_latch(p_owner, was_joining, previous_lane, blue_exit_seen);
#endif
#endif
#if FOLLOW_RED_RETURN_GRACE
        /* Keep the first returning red frame long enough for one more acquisition.
         * Any loss of red or an edge outside the measured range ends the grace. */
        uint32_t join_age = now - p_owner->join.started_ms;
        bool near_deadline = join_age >= FOLLOW_JOIN_MAX_MS - red_return_grace_ms &&
            join_age < FOLLOW_JOIN_MAX_MS;
        p_owner->join.late_red_return = (p_owner->join.late_red_return || near_deadline) &&
            was_joining && previous_lane == RGB_CLASS_RED && colour == RGB_CLASS_RED &&
            p_owner->policy.status.red_second_release_used &&
            p_owner->policy.status.red_exit_alignment_used && p_owner->has_ir &&
            p_owner->ir.valid && p_owner->ir.error == 0 &&
            now - p_owner->ir.sampled_ms <= FOLLOW_IR_FRESH_MS &&
            p_owner->ir.left_raw <= 370U && p_owner->ir.right_raw <= 280U;
#endif
        uint8_t join_reason = update_join(&p_owner->join, &p_owner->rgb, now,
                                         colour, p_owner->has_rgb && colour_allowed);
        join_active = (join_reason == FOLLOW_NONE) && p_owner->join.active;
        join_timed_out = (join_reason == FOLLOW_JOIN_TIMEOUT);
        join_completed = was_joining && !p_owner->join.active &&
                         (previous_lane != p_owner->join.lane);
#if FOLLOW_RED_BEND_AFTER_HANDOFF
        bool approached_red_bend = p_owner->join.red_approach;
#if FOLLOW_BLUE_RED_LATE_CONFIRM
        approached_red_bend = approached_red_bend || p_owner->join.late_blue_red;
#endif
        red_arrived_after_bend = join_completed && previous_lane == RGB_CLASS_BLUE &&
            colour == RGB_CLASS_RED && approached_red_bend &&
            now - p_owner->join.started_ms >= FOLLOW_JOIN_MAX_MS;
#endif
#if FOLLOW_RED_SECOND_EXIT_REFINEMENT
        red_reacquired_after_join = was_joining && join_reason == FOLLOW_NONE &&
            !p_owner->join.active && previous_lane == RGB_CLASS_RED && colour == RGB_CLASS_RED;
#endif
        blue_surface = (p_owner->join.lane == RGB_CLASS_BLUE);
#if FOLLOW_BLUE_EXIT_AFTER_RECOVERY
        if (was_joining && !p_owner->join.active && blue_surface &&
            previous_lane == RGB_CLASS_BLUE && p_owner->policy.bend_recovering)
        {
            p_owner->blue_recovered = true;
            p_owner->blue_recovered_ms = now;
        }
        else if (p_owner->join.active || !blue_surface || colour != RGB_CLASS_BLUE)
        {
            p_owner->blue_recovered = false;
        }
#endif
#if FOLLOW_BLUE_CONFIRM_GRACE
        if (was_joining && !p_owner->join.active && blue_surface &&
            previous_lane == RGB_CLASS_BLUE && now - p_owner->join.started_ms >= FOLLOW_JOIN_MAX_MS)
        {
            p_owner->policy.status.blue_late_confirmed = true;
        }
#endif
    }
#elif FOLLOW_BLUE_CORRIDOR
    bool colour_allowed = (colour == RGB_CLASS_BLUE);
#elif FOLLOW_RED_CORRIDOR
    bool colour_allowed = (colour == RGB_CLASS_RED);
#elif FOLLOW_YELLOW_CORRIDOR
    bool colour_allowed = (colour == RGB_CLASS_YELLOW);
#else
    bool colour_allowed = (colour == RGB_CLASS_BLACK);
#endif
#if FOLLOW_YELLOW_EXIT_CUE
    bool yellow_cue = (p_owner->join.lane == RGB_CLASS_YELLOW) &&
                      yellow_approach_reading(&p_owner->rgb, colour);
#endif
#if FOLLOW_BLUE_EXIT_LEAD
    const uint16_t blue_exit_clear_max = FOLLOW_BEND_CUE_REFINEMENT ? 596U :
        (FOLLOW_BLUE_RED_BEND_CUES ? 590U : (FOLLOW_BLUE_RED_BEND_TRIAL ? 554U : 550U));
#endif
#if FOLLOW_BLUE_EXIT_AFTER_RECOVERY
    const uint16_t recovered_blue_clear_max = FOLLOW_BLUE_RED_BEND_CUES ? 590U :
        (FOLLOW_BLUE_RED_BEND_TRIAL ? 564U : 560U);
#endif
    black_follow_input_t input = {
        .now_ms=now, .ir_ms=p_owner->ir.sampled_ms,
        .left_raw=p_owner->ir.left_raw, .right_raw=p_owner->ir.right_raw,
        .has_ir=p_owner->has_ir,
        .ir_valid=p_owner->ir.valid && (p_owner->ir.error == 0),
        .rgb_target_matched=p_owner->has_rgb && colour_allowed,
        .rgb_join_active=join_active,
        .rgb_join_timed_out=join_timed_out,
        .rgb_healthy=rgb_healthy,
        .rgb_sequence=p_owner->rgb.sequence,
        .rgb_colour=rgb_classification_class(colour),
        .blue_surface=blue_surface,
#if FOLLOW_BLUE_RED_HANDOFF
        .blue_red_blend=blue_surface && (reason == RGB_CLASS_NO_MATCH) &&
            blue_red_blend_reading(&p_owner->rgb),
#endif
#if FOLLOW_BLUE_EXIT_LEAD
        .blue_exit_fade=colour == RGB_CLASS_BLUE &&
            blue_exit_reading(&p_owner->rgb, blue_exit_clear_max, 1244U, 1465U),
#endif
#if FOLLOW_BEND_CUE_REFINEMENT
        .blue_inner_fade=colour == RGB_CLASS_BLUE &&
            blue_exit_reading(&p_owner->rgb, FOLLOW_BLUE_DIAGONAL_EXIT ? 560U : 550U,
                              FOLLOW_BLUE_DIAGONAL_EXIT ? 1235U : 1238U,
                              FOLLOW_BLUE_DIAGONAL_EXIT ? 1472U : 1465U),
#endif
#if FOLLOW_BLUE_DIAGONAL_EXIT
        .blue_exit_on_black=blue_surface && colour == RGB_CLASS_BLACK,
#endif
#if FOLLOW_BLUE_EXIT_AFTER_RECOVERY
        .blue_recovered_fade=colour == RGB_CLASS_BLUE && p_owner->blue_recovered &&
            now - p_owner->blue_recovered_ms >= 40U && now - p_owner->blue_recovered_ms < 350U &&
            blue_exit_reading(&p_owner->rgb, recovered_blue_clear_max, 1244U, 1465U),
#endif
#if FOLLOW_RED_EXIT_LEAD
        .red_surface=(track_join ? p_owner->join.lane : colour) == RGB_CLASS_RED,
        .red_exit_fade=colour == RGB_CLASS_RED && red_exit_reading(&p_owner->rgb),
#endif
#if FOLLOW_RED_BEND_AFTER_HANDOFF
        .red_arrived_after_bend=red_arrived_after_bend,
        .red_handoff_mix=red_mix_reading(&p_owner->rgb, colour,
                                         FOLLOW_BLUE_RED_JOIN_REFINEMENT ? 1528U : 1540U),
#endif
#if FOLLOW_RED_DIAGONAL_REACQUIRE
        .red_diagonal_mix=red_mix_reading(&p_owner->rgb, colour, 1510U),
#endif
#if FOLLOW_RED_SECOND_EXIT_REFINEMENT
        .red_reacquired_after_join=red_reacquired_after_join,
#endif
#if FOLLOW_RED_BETWEEN_BENDS
        .red_rgb_clear=p_owner->rgb.clear,
#endif
#if FOLLOW_BLUE_RED_CONFIRM_GRACE
        .red_approach_grace=join_active && now - p_owner->join.started_ms >= FOLLOW_JOIN_MAX_MS &&
            red_approach_in_progress(&p_owner->join, &p_owner->rgb, colour),
#endif
#if FOLLOW_RED_RETURN_GRACE
        .red_return_grace=join_active && p_owner->join.late_red_return,
#endif
#if FOLLOW_HOME_TRACK
        .yellow_surface=(track_join ? p_owner->join.lane : colour) == RGB_CLASS_YELLOW,
#endif
#if FOLLOW_LOOP_RECOVERY_DIRECTION != 0
        .bend_loss_rgb=(colour == RGB_CLASS_BACKGROUND) ||
            (blue_surface && (rgb_classification_class(colour) == RGB_CLASS_UNKNOWN)),
        .rgb_same_lane=colour == p_owner->join.lane,
#endif
#if FOLLOW_YELLOW_EXIT_CUE
        .yellow_exit_cue=yellow_cue && (reason == RGB_CLASS_NO_MATCH),
#endif
#if FOLLOW_YELLOW_LEAD_CUE
        .yellow_lead_cue=yellow_cue && (colour == RGB_CLASS_YELLOW),
#endif
        .hardware_ready=hardware_ready,
#if AAI05_FOLLOW_MQTT
        .remote_start=lease_valid && remote.start,
        .straight_mode=remote.straight,
#else
        .start_pressed=(buttons & (1U << 20U)) == 0U,
#endif
        .stop_pressed=(buttons & (1U << 21U)) == 0U
    };
#if FOLLOW_MOTOR_HALF_PCT
    p_owner->policy.status.left_half_pct = 0;
    p_owner->policy.status.right_half_pct = 0;
#endif
    black_follow_output_t request = black_follow_step(&p_owner->policy, &input);
    motion_output_t output = {0};
    if (request.start)
    {
#if FOLLOW_STOP_TRACE
        p_owner->policy.status.stop_trace = (follow_stop_trace_t){0};
#endif
#if FOLLOW_BLUE_RED_HANDOFF
        p_owner->policy.status.blue_red_join_used = false;
#endif
#if FOLLOW_BLUE_EXIT_AFTER_RECOVERY
        p_owner->blue_recovered = false;
#endif
#if FOLLOW_BLUE_CONFIRM_GRACE
        p_owner->policy.status.blue_late_confirmed = false;
#endif
#if FOLLOW_CLIPPED_LOOP
        p_owner->policy.status.lease_limit = FOLLOW_NONE;
        p_owner->policy.status.last_lease_ms = 0U;
#endif
        if (motor_drive_rearm() != 0)
        {
            owner_stop(p_owner, FOLLOW_DRIVER);
            request.drive = false;
        }
        else
        {
            motion_control_init(&p_owner->motion);
#if FOLLOW_MULTICOLOUR_CORRIDOR
            p_owner->join = (follow_join_t){.lane=colour};
#endif
        }
    }
    if (request.drive)
    {
#if FOLLOW_CORNER_DIRECTION != 0
        if (request.pivot)
        {
            /* Rotation has its own deadline and colour reacquisition. */
            join_active = false;
            join_completed = false;
            p_owner->join.active = false;
        }
        else if (was_pivoting)
        {
            p_owner->join.lane = colour;
        }
#endif
        motion_chassis_command_t chassis = {.issued_ms=now,
            .sequence=++p_owner->sequence, .forward_pct=request.forward_pct,
            .turn_pct=request.turn_pct};
        motion_command_t command;
        motion_result_t converted;
#if FOLLOW_BLUE_EXIT_PIVOT
        if (request.blue_exit_pivot)
        {
            /* In half-percent units after scaling: 25% forward, 5% reverse. */
            command = (motion_command_t){.issued_ms=now, .sequence=chassis.sequence,
                                         .left_pct=-50, .right_pct=-10};
            converted = MOTION_ACCEPTED;
        }
        else
#endif
#if FOLLOW_CORNER_DIRECTION != 0
        if (request.pivot)
        {
            /* Logical forward is raw L negative / R positive. */
            int8_t duty = (int8_t)(-FOLLOW_CORNER_DIRECTION * FOLLOW_DUTY_LIMIT_PCT);
            command = (motion_command_t){.issued_ms=now, .sequence=chassis.sequence,
                                          .left_pct=duty, .right_pct=duty};
            converted = MOTION_ACCEPTED;
        }
        else
#elif FOLLOW_RED_SECOND_PIVOT
        if (request.pivot)
        {
            /* Raw negative is forward on the left and reverse on the right. */
            int8_t left = FOLLOW_RED_SECOND_SLOW ? -45 : -50;
            int8_t right = FOLLOW_RED_SECOND_SLOW ? -25 : -20;
#if FOLLOW_RED_FIRST_BEND_BOOST
            if (p_owner->policy.red_first_active)
            {
                left = -50;
                right = FOLLOW_RED_FIRST_BEND_TIGHTER ? -40 : -30;
            }
#if FOLLOW_RED_SECOND_EXIT_REFINEMENT
            else if (p_owner->policy.status.red_first_pivot_used)
            {
                right = -35;
            }
#endif
#endif
            command = (motion_command_t){.issued_ms=now, .sequence=chassis.sequence,
                                         .left_pct=left, .right_pct=right};
            converted = MOTION_ACCEPTED;
        }
        else
#endif
        {
            converted = motion_chassis_convert(&chassis, &command);
#if !FOLLOW_MOTOR_HALF_PCT
            if (converted == MOTION_ACCEPTED)
            {
                /* Common scaling keeps the steering ratio. */
                command.left_pct = (int8_t)((int32_t)command.left_pct * FOLLOW_DUTY_LIMIT_PCT / 50);
                command.right_pct = (int8_t)((int32_t)command.right_pct * FOLLOW_DUTY_LIMIT_PCT / 50);
            }
#endif
        }
        if ((converted != MOTION_ACCEPTED) ||
            (motion_control_submit(&p_owner->motion, &command, app_now_ms()) !=
             MOTION_ACCEPTED))
        {
            owner_stop(p_owner, FOLLOW_DRIVER);
        }
        else
        {
            /* Anchor hardware BEFORE reading kernel time. The10ms margin
             * conservatively covers timestamp quantization; later preemption
             * cannot move the absolute deadline checked inside the driver. */
            uint32_t hardware_anchor_us = motor_drive_now_us();
            uint32_t apply_now = app_now_ms();
            motion_control_step(&p_owner->motion, apply_now, input.stop_pressed,
                                  hardware_ready, p_motor->active, &output);
            uint8_t deadline_reason;
            uint32_t remaining = black_follow_remaining_ms(&p_owner->policy, apply_now,
                                                            &deadline_reason);
            uint32_t ir_age = apply_now - p_owner->ir.sampled_ms;
            uint32_t rgb_age = apply_now - p_owner->rgb.sampled_ms;
            uint32_t rgb_fresh_ms = rgb_default_calibration()->max_age_ms;
            if (remaining == 0U)
            {
                owner_stop(p_owner, deadline_reason);
                output = (motion_output_t){0};
            }
            else if (output.lease_ms == 0U)
            {
                owner_stop(p_owner, FOLLOW_DRIVER);
            }
            else if ((ir_age >= FOLLOW_IR_FRESH_MS) || (rgb_age >= rgb_fresh_ms))
            {
                owner_stop(p_owner, ir_age >= FOLLOW_IR_FRESH_MS ? FOLLOW_IR : FOLLOW_RGB);
                output = (motion_output_t){0};
            }
            else
            {
                uint32_t ir_remaining = FOLLOW_IR_FRESH_MS - ir_age;
                uint32_t rgb_remaining = rgb_fresh_ms - rgb_age;
                if (remaining > ir_remaining)
                {remaining = ir_remaining; deadline_reason = FOLLOW_IR;}
                if (remaining > rgb_remaining)
                {remaining = rgb_remaining; deadline_reason = FOLLOW_RGB;}
#if FOLLOW_MULTICOLOUR_CORRIDOR
                if (join_active)
                {
                    uint32_t join_age = apply_now - p_owner->join.started_ms;
                    uint32_t join_limit = join_window_ms(&p_owner->join, &p_owner->rgb, colour);
                    uint32_t join_remaining = join_age < join_limit ? join_limit - join_age : 0U;
                    if (remaining > join_remaining)
                    {remaining = join_remaining; deadline_reason = FOLLOW_JOIN_TIMEOUT;}
                }
#endif
                if (remaining > output.lease_ms)
                {remaining = output.lease_ms; deadline_reason = FOLLOW_DRIVER;}
#if AAI05_FOLLOW_MQTT
                uint32_t remote_remaining = follow_remote_remaining(remote, apply_now);
                if (remaining > remote_remaining)
                {remaining = remote_remaining; deadline_reason = FOLLOW_REMOTE_TIMEOUT;}
#endif
#if FOLLOW_RED_SECOND_PIVOT
                bool timed_red_pivot = request.pivot;
#if FOLLOW_RED_OUTSIDE_RECOVERY
                /* The outside spin is re-decided every cycle and bounded by the
                 * policy (edge present / 350 ms) and the 20 ms motor lease. */
                timed_red_pivot = timed_red_pivot && !request.red_outside_spin;
#endif
                if (timed_red_pivot)
                {
                    uint32_t pivot_age = apply_now - p_owner->policy.red_exit_lead_ms;
                    uint32_t pivot_remaining = pivot_age < FOLLOW_RED_SECOND_PIVOT_LIMIT_MS ?
                        FOLLOW_RED_SECOND_PIVOT_LIMIT_MS - pivot_age : 0U;
                    if (remaining > pivot_remaining)
                    {remaining = pivot_remaining; deadline_reason = FOLLOW_DRIVER;}
                }
#endif
#if FOLLOW_BLUE_EXIT_PIVOT
                if (request.blue_exit_pivot)
                {
                    uint32_t pivot_age = apply_now - p_owner->policy.blue_exit_lead_ms;
                    uint32_t pivot_remaining = pivot_age < FOLLOW_BLUE_EXIT_PIVOT_LIMIT_MS ?
                        FOLLOW_BLUE_EXIT_PIVOT_LIMIT_MS - pivot_age : 0U;
                    if (remaining > pivot_remaining)
                    {remaining = pivot_remaining; deadline_reason = FOLLOW_DRIVER;}
                }
#endif
#if FOLLOW_BLUE_EXIT_LEFT_BOOST
                bool boost_left = request.blue_exit_pivot;
#if FOLLOW_RED_FIRST_LEFT_BOOST
                boost_left = boost_left || (request.pivot && p_owner->policy.red_first_active);
#endif
#if FOLLOW_RED_SECOND_ARC_BOOST
                boost_left = boost_left || request.red_second_exit_arc;
#endif
#if FOLLOW_RED_FINAL_LEFT_BOOST
                boost_left = boost_left || request.red_final_arc;
#endif
                int32_t left_limit_half_pct = boost_left ? 60 : FOLLOW_DUTY_LIMIT_HALF_PCT;
#if FOLLOW_BLUE_SECOND_LEFT_BOOST
                bool use_35_percent_left = request.blue_second_left_boost;
#if FOLLOW_RED_FINAL_ARC_EXTEND
                use_35_percent_left = use_35_percent_left || request.red_final_arc_extended;
#endif
                if (use_35_percent_left)
                {
                    left_limit_half_pct = 70;
                }
#if FOLLOW_RED_FINAL_ARC_STRONG
                /* v0.81: test3 finished the final red arc short of the lane
                 * heading. Raise only the left (outside) wheel: 35% for the
                 * main arc, 40% for the extension. Right wheel unchanged. */
                if (request.red_final_arc)
                {
                    left_limit_half_pct = request.red_final_arc_extended ? 80 : 70;
                }
#endif
#endif
#elif FOLLOW_MOTOR_HALF_PCT
                const int32_t left_limit_half_pct = FOLLOW_DUTY_LIMIT_HALF_PCT;
#endif
                if (remaining <= 10U)
                {
                    owner_stop(p_owner, deadline_reason);
                    output = (motion_output_t){0};
                }
#if FOLLOW_MOTOR_HALF_PCT
                else if (apply_fractional_drive(&p_owner->policy.status, &output, left_limit_half_pct,
                    hardware_anchor_us + (remaining - 10U) * 1000U) != 0)
#else
                else if (motor_drive_set_until(output.left_pct, output.right_pct,
                    hardware_anchor_us + (remaining - 10U) * 1000U) != 0)
#endif
                {
                    owner_stop(p_owner, FOLLOW_DRIVER);
                    output = (motion_output_t){0};
                }
                else
                {
                    output.lease_ms = (uint8_t)(remaining - 10U);
#if FOLLOW_CLIPPED_LOOP
                    /* Keep the last successfully armed deadline after STOP.
                     * A changed hardware counter separately confirms an IRQ cutoff. */
                    p_owner->policy.status.lease_limit = deadline_reason == FOLLOW_DRIVER ?
                        FOLLOW_LEASE_REFRESH : deadline_reason;
                    p_owner->policy.status.last_lease_ms = output.lease_ms;
                    p_owner->last_lease_cutoff_count = p_motor->cutoff_count;
#endif
                }
            }
        }
    }
    else
    {
        motion_control_stop(&p_owner->motion,
            input.stop_pressed ? MOTION_REASON_BUTTON : MOTION_REASON_REQUEST);
        motor_pulse_stop();
    }
#if FOLLOW_MULTICOLOUR_CORRIDOR
    if (join_completed && (p_owner->policy.status.phase == FOLLOW_RUNNING))
    {
        ++p_owner->join.completed;
    }
    p_owner->policy.status.join_active = join_active &&
                                        (p_owner->policy.status.phase == FOLLOW_RUNNING);
    p_owner->policy.status.join_count = p_owner->join.completed;
#endif
#if FOLLOW_CLIPPED_LOOP
    p_owner->policy.status.lease_alarm = (p_owner->policy.status.last_lease_ms > 0U) &&
        (p_motor->cutoff_count != p_owner->last_lease_cutoff_count);
#endif
#if FOLLOW_STOP_TRACE
    if ((track_join || request.start) && p_owner->policy.status.phase == FOLLOW_STOPPED)
    {
        bool was_joining = track_join && p_owner->join.active;
        p_owner->policy.status.stop_trace = (follow_stop_trace_t){
            .captured_ms=now,
            .rgb_ms=p_owner->rgb.sampled_ms,
            .join_age_ms=was_joining ? now - p_owner->join.started_ms : 0U,
            .left_raw=input.left_raw,
            .right_raw=input.right_raw,
            .rgb_result=colour,
            .join_active=was_joining,
            .valid=true
        };
    }
#endif
#if FOLLOW_BEND_TRACE
    if (p_owner->policy.status.phase == FOLLOW_RUNNING)
    {
        p_owner->policy.status.bend_trace = (follow_bend_trace_t){
            .captured_ms=now, .ir_ms=input.ir_ms, .rgb_ms=p_owner->rgb.sampled_ms,
            .left_raw=input.left_raw, .right_raw=input.right_raw,
            .red=p_owner->rgb.red, .green=p_owner->rgb.green,
            .blue=p_owner->rgb.blue, .clear=p_owner->rgb.clear,
            .rgb_result=colour
        };
    }
#endif
    *p_follow = p_owner->policy.status;
    *p_motion = (motion_status_t){.issued_ms=p_owner->motion.command.issued_ms,
        .sequence=p_owner->motion.command.sequence, .left_pct=output.left_pct,
        .right_pct=output.right_pct, .lease_ms=output.lease_ms,
        .phase=p_owner->motion.phase, .reason=p_owner->motion.reason,
        .has_sequence=p_owner->motion.has_sequence};
#if AAI05_FOLLOW_MQTT
    p_motion->epoch = remote.epoch;
    if (remote.armed && !remote.fed)
    {
        /* Acknowledge readiness before the network owner issues the first feed. */
        p_motion->phase = p_follow->ready ? MOTION_READY : MOTION_FAULT;
    }
    else if (p_follow->phase != FOLLOW_RUNNING)
    {
        p_motion->phase = MOTION_STOPPED;
    }
#endif
}
#endif
