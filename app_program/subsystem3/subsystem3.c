#include "subsystem3.h"
#include "../subsystem1/subsystem1.h"
#if AAI05_RGB_ACTIVE
#include "../subsystem1/rgb_diag.h"
#endif
#if FOLLOW_STOP_TRACE
#include "../subsystem1/rgb_classifier.h"
#endif
#if TM_WIFI_CYW43
#include "cyw43_utk.h"
#endif
#if AAI05_FOLLOW_MQTT && AAI05_REMOTE_APP
#include "drive_service.h"
#endif

#if !AAI05_FOLLOW_APP
static const char *motor_phase_name(uint8_t phase)
{
#if AAI05_MOTION_APP
    static const char *const names[] = {"READY", "RUNNING", "STOPPED", "FAULT"};
    return phase < 4U ? names[phase] : "INVALID";
#else
    static const char *const names[] = {"IDLE", "PULSE", "COOLDOWN", "DONE", "STOPPED"};
    return phase < 5U ? names[phase] : "INVALID";
#endif
}
#endif

#if FOLLOW_BEND_TRACE
static void report_bend(const motion_snapshot_t *p_motion)
{
    const black_follow_status_t *p_follow = &p_motion->follow;
    const follow_bend_trace_t *p_trace = &p_follow->bend_trace;
    tm_printf((UB *)"BEND[t=%u elapsed=%u ir_t=%u ir=%u/%u norm=%u/%u "
              "rgb_t=%u rgb=%s/%s rgbc=%u/%u/%u/%u turn=%d duty_half=%d/%d "
              "join=%u "
#if FOLLOW_BLUE_EXIT_LEAD
              "blue_exit=%u "
#endif
#if FOLLOW_BLUE_RED_CONFIRM_GRACE
              "red_grace=%u "
#endif
#if FOLLOW_BLUE_EXIT_AFTER_RECOVERY
              "blue_after_recovery=%u "
#endif
#if FOLLOW_RED_EXIT_LEAD
              "red_exit=%u "
#endif
#if FOLLOW_RED_SECOND_PIVOT
              "red_pivot=%u "
#endif
#if FOLLOW_RED_SECOND_SLOW
              "red_slow=%u "
#endif
#if FOLLOW_RED_RETURN_GRACE
              "red_return_grace=%u "
#endif
#if FOLLOW_RED_EXIT_TURN
              "red_exit_turn=%u "
#endif
#if FOLLOW_RED_INSIDE_RELEASE
              "red_release=%u "
#endif
#if FOLLOW_RED_BETWEEN_BENDS
              "red_clear_pivot=%u "
#endif
#if FOLLOW_RED_EXIT_ALIGN
              "red_exit_align=%u "
#endif
#if FOLLOW_RED_HANDOFF_ALIGN
              "red_align=%u "
#endif
#if FOLLOW_RED_FIRST_BEND_BOOST
              "red_first=%u "
#endif
#if FOLLOW_RED_BEND_AFTER_HANDOFF
              "red_handoff=%u "
#endif
              "enc=%d/%d]\n",
              (UINT)p_trace->captured_ms, (UINT)p_follow->elapsed_ms, (UINT)p_trace->ir_ms,
              (UINT)p_trace->left_raw, (UINT)p_trace->right_raw,
              (UINT)p_follow->left_norm, (UINT)p_follow->right_norm, (UINT)p_trace->rgb_ms,
              rgb_class_name(rgb_classification_class(p_trace->rgb_result)),
              rgb_reason_name(rgb_classification_reason(p_trace->rgb_result)),
              (UINT)p_trace->red, (UINT)p_trace->green, (UINT)p_trace->blue, (UINT)p_trace->clear,
              (INT)p_follow->turn_pct, (INT)p_follow->left_half_pct, (INT)p_follow->right_half_pct,
              (UINT)p_follow->join_active,
#if FOLLOW_BLUE_EXIT_LEAD
              (UINT)p_follow->blue_exit_lead_used,
#endif
#if FOLLOW_BLUE_RED_CONFIRM_GRACE
              (UINT)p_follow->red_approach_grace_used,
#endif
#if FOLLOW_BLUE_EXIT_AFTER_RECOVERY
              (UINT)p_follow->blue_recovered_exit_used,
#endif
#if FOLLOW_RED_EXIT_LEAD
              (UINT)p_follow->red_exit_lead_used,
#endif
#if FOLLOW_RED_SECOND_PIVOT
              (UINT)p_follow->red_second_pivot_used,
#endif
#if FOLLOW_RED_SECOND_SLOW
              (UINT)p_follow->red_second_slow_used,
#endif
#if FOLLOW_RED_RETURN_GRACE
              (UINT)p_follow->red_return_grace_used,
#endif
#if FOLLOW_RED_EXIT_TURN
              (UINT)p_follow->red_exit_turn_used,
#endif
#if FOLLOW_RED_INSIDE_RELEASE
              (UINT)p_follow->red_second_release_used,
#endif
#if FOLLOW_RED_BETWEEN_BENDS
              (UINT)p_follow->red_clear_pivot_used,
#endif
#if FOLLOW_RED_EXIT_ALIGN
              (UINT)p_follow->red_exit_alignment_used,
#endif
#if FOLLOW_RED_HANDOFF_ALIGN
              (UINT)p_follow->red_alignment_used,
#endif
#if FOLLOW_RED_FIRST_BEND_BOOST
              (UINT)p_follow->red_first_pivot_used,
#endif
#if FOLLOW_RED_BEND_AFTER_HANDOFF
              (UINT)p_follow->red_handoff_armed,
#endif
              (INT)p_motion->left_count, (INT)p_motion->right_count);
}
#endif

static void report_task(INT start_code, void *extra)
{
    ir_snapshot_t ir;
    motion_snapshot_t motion;
    uint32_t now;
#if !AAI05_FOLLOW_APP
    uint32_t ir_age;
    uint32_t motion_age;
#endif
    ER ir_error;
    ER motion_error;
#if !AAI05_FOLLOW_APP
    UINT radio_ready = 0U;
#endif
    (void)start_code;
    (void)extra;
    for (;;)
    {
        motion_error = subsystem2_get_snapshot(&motion);
#if FOLLOW_BEND_TRACE
        if (motion_error == E_OK && motion.follow.phase == FOLLOW_RUNNING)
        {
            report_bend(&motion);
            (void)tk_dly_tsk(40);
            continue;
        }
#endif
        ir_error = subsystem1_get_snapshot(&ir);
#if AAI05_FOLLOW_APP
        now = app_now_ms();
        if (ir_error == E_OK)
        {
            tm_printf((UB *)"t=%u IR[L=%u R=%u valid=%u err=%d age=%u]\n",
                (UINT)now, (UINT)ir.left_raw, (UINT)ir.right_raw,
                (UINT)(ir.valid && (ir.error == E_OK) &&
                       ((uint32_t)(now-ir.sampled_ms) <= FOLLOW_IR_FRESH_MS)),
                (INT)ir.error, (UINT)(now-ir.sampled_ms));
        }
        else
        {
            tm_printf((UB *)"IR[read=0 err=%d]\n", (INT)ir_error);
        }
        if (motion_error == E_OK)
        {
#if FOLLOW_STOP_TRACE
            const follow_stop_trace_t *p_stop = &motion.follow.stop_trace;
#endif
            tm_printf((UB *)"FOLLOW[state=%s reason=%s ready=%u norm=%u/%u "
                "limit=%u" FOLLOW_DUTY_DISPLAY_SUFFIX
                " turn_max=%u turn=%d elapsed=%u L=%d R=%d lease=%u age=%u "
#if FOLLOW_MOTOR_HALF_PCT
                "L_half_pct=%d R_half_pct=%d "
#endif
#if FOLLOW_YELLOW_LEAD_CUE
                "yellow_lead=%u "
#endif
#if FOLLOW_BLUE_REVERSAL_DAMPING
                "blue_damp=%u "
#endif
#if FOLLOW_MULTICOLOUR_CORRIDOR
                "join=%u joins=%u "
#endif
#if FOLLOW_BLUE_RED_HANDOFF
                "blue_red_join=%u "
#endif
#if FOLLOW_BLUE_SECOND_BOOST
                "blue_second=%u "
#endif
#if FOLLOW_BLUE_OUTER_EDGE
                "blue_outer=%u "
#endif
#if FOLLOW_BLUE_OUTER_RECOVERY
                "blue_recovery=%u "
#endif
#if FOLLOW_BLUE_BEND_SLOW
                "blue_slow=%u "
#endif
#if FOLLOW_BLUE_CONFIRM_GRACE
                "blue_confirm=%u "
#endif
#if FOLLOW_BLUE_EXIT_LEAD
                "blue_exit=%u "
#endif
#if FOLLOW_BLUE_RED_CONFIRM_GRACE
                "red_grace=%u "
#endif
#if FOLLOW_BLUE_EXIT_AFTER_RECOVERY
                "blue_after_recovery=%u "
#endif
#if FOLLOW_RED_EXIT_LEAD
                "red_exit=%u "
#endif
#if FOLLOW_RED_SECOND_PIVOT
                "red_pivot=%u "
#endif
#if FOLLOW_RED_SECOND_SLOW
                "red_slow=%u "
#endif
#if FOLLOW_RED_RETURN_GRACE
                "red_return_grace=%u "
#endif
#if FOLLOW_RED_EXIT_TURN
                "red_exit_turn=%u "
#endif
#if FOLLOW_RED_INSIDE_RELEASE
                "red_release=%u "
#endif
#if FOLLOW_RED_BETWEEN_BENDS
                "red_clear_pivot=%u "
#endif
#if FOLLOW_RED_EXIT_ALIGN
                "red_exit_align=%u "
#endif
#if FOLLOW_RED_HANDOFF_ALIGN
                "red_align=%u "
#endif
#if FOLLOW_RED_FIRST_BEND_BOOST
                "red_first=%u "
#endif
#if FOLLOW_RED_BEND_AFTER_HANDOFF
                "red_handoff=%u "
#endif
#if FOLLOW_TWO_TURN
                "corner=%s "
#elif FOLLOW_CORNER_DIRECTION != 0
                "corner=%s direction=" FOLLOW_CORNER_NAME " "
#endif
#if FOLLOW_CLIPPED_LOOP
                "last_lease=%u lease_limit=%s lease_alarm=%u motor_err=%d "
#endif
#if FOLLOW_STOP_TRACE
                "stop_t=%u stop_rgb=%s/%s stop_sample=%u stop_ir=%u/%u "
                "stop_join=%u stop_join_ms=%u "
#endif
                "ENC=%d/%d PIO_BAD=%u inhibit=%u track=" FOLLOW_TRACK_NAME
                " target=" FOLLOW_TARGET_NAME " DRIVE=%s]\n",
                black_follow_phase_name(motion.follow.phase),
                black_follow_reason_name(motion.follow.reason),
                (UINT)motion.follow.ready, (UINT)motion.follow.left_norm,
                (UINT)motion.follow.right_norm, (UINT)FOLLOW_DUTY_LIMIT_PCT,
                (UINT)FOLLOW_TURN_PEAK_PCT, (INT)motion.follow.turn_pct,
                (UINT)motion.follow.elapsed_ms, (INT)motion.motion.left_pct,
                (INT)motion.motion.right_pct, (UINT)motion.motion.lease_ms,
                (UINT)(now-motion.sampled_ms),
#if FOLLOW_MOTOR_HALF_PCT
                (INT)motion.follow.left_half_pct, (INT)motion.follow.right_half_pct,
#endif
#if FOLLOW_YELLOW_LEAD_CUE
                (UINT)motion.follow.yellow_lead_used,
#endif
#if FOLLOW_BLUE_REVERSAL_DAMPING
                (UINT)motion.follow.blue_reversal_damped,
#endif
#if FOLLOW_MULTICOLOUR_CORRIDOR
                (UINT)motion.follow.join_active, (UINT)motion.follow.join_count,
#endif
#if FOLLOW_BLUE_RED_HANDOFF
                (UINT)motion.follow.blue_red_join_used,
#endif
#if FOLLOW_BLUE_SECOND_BOOST
                (UINT)motion.follow.blue_second_used,
#endif
#if FOLLOW_BLUE_OUTER_EDGE
                (UINT)motion.follow.blue_outer_used,
#endif
#if FOLLOW_BLUE_OUTER_RECOVERY
                (UINT)motion.follow.blue_outer_recovery_used,
#endif
#if FOLLOW_BLUE_BEND_SLOW
                (UINT)motion.follow.blue_bend_slow_used,
#endif
#if FOLLOW_BLUE_CONFIRM_GRACE
                (UINT)motion.follow.blue_late_confirmed,
#endif
#if FOLLOW_BLUE_EXIT_LEAD
                (UINT)motion.follow.blue_exit_lead_used,
#endif
#if FOLLOW_BLUE_RED_CONFIRM_GRACE
                (UINT)motion.follow.red_approach_grace_used,
#endif
#if FOLLOW_BLUE_EXIT_AFTER_RECOVERY
                (UINT)motion.follow.blue_recovered_exit_used,
#endif
#if FOLLOW_RED_EXIT_LEAD
                (UINT)motion.follow.red_exit_lead_used,
#endif
#if FOLLOW_RED_SECOND_PIVOT
                (UINT)motion.follow.red_second_pivot_used,
#endif
#if FOLLOW_RED_SECOND_SLOW
                (UINT)motion.follow.red_second_slow_used,
#endif
#if FOLLOW_RED_RETURN_GRACE
                (UINT)motion.follow.red_return_grace_used,
#endif
#if FOLLOW_RED_EXIT_TURN
                (UINT)motion.follow.red_exit_turn_used,
#endif
#if FOLLOW_RED_INSIDE_RELEASE
                (UINT)motion.follow.red_second_release_used,
#endif
#if FOLLOW_RED_BETWEEN_BENDS
                (UINT)motion.follow.red_clear_pivot_used,
#endif
#if FOLLOW_RED_EXIT_ALIGN
                (UINT)motion.follow.red_exit_alignment_used,
#endif
#if FOLLOW_RED_HANDOFF_ALIGN
                (UINT)motion.follow.red_alignment_used,
#endif
#if FOLLOW_RED_FIRST_BEND_BOOST
                (UINT)motion.follow.red_first_pivot_used,
#endif
#if FOLLOW_RED_BEND_AFTER_HANDOFF
                (UINT)motion.follow.red_handoff_armed,
#endif
#if FOLLOW_TWO_TURN || FOLLOW_CORNER_DIRECTION != 0
                black_follow_corner_name(motion.follow.corner_phase),
#endif
#if FOLLOW_CLIPPED_LOOP
                (UINT)motion.follow.last_lease_ms,
                black_follow_reason_name(motion.follow.lease_limit),
                (UINT)motion.follow.lease_alarm, (INT)motion.motor.error,
#endif
#if FOLLOW_STOP_TRACE
                (UINT)p_stop->captured_ms,
                p_stop->valid ? rgb_class_name(rgb_classification_class(p_stop->rgb_result)) : "NONE",
                p_stop->valid ? rgb_reason_name(rgb_classification_reason(p_stop->rgb_result)) : "NONE",
                (UINT)p_stop->rgb_ms, (UINT)p_stop->left_raw, (UINT)p_stop->right_raw,
                (UINT)p_stop->join_active, (UINT)p_stop->join_age_ms,
#endif
                (INT)motion.left_count,
                (INT)motion.right_count, (UINT)motion.pio_invalid_flags,
                (UINT)MOTOR_TEST_PROBE,
                (!MOTOR_TEST_PROBE && motion.motor.active) ? "ON" : "OFF");
        }
        else
        {
            tm_printf((UB *)"FOLLOW[read=0 err=%d]\n", (INT)motion_error);
        }
#if AAI05_FOLLOW_MQTT && AAI05_REMOTE_APP
        drive_publish_counters_t publish;
        drive_service_counters(&publish);
        tm_printf((UB *)"NET[ack_fail=%u ack_retry_ok=%u ack_abandoned=%u "
                  "telemetry_fail=%u route_fail=%u]\n",
                  (UINT)publish.ack_failed, (UINT)publish.ack_retried,
                  (UINT)publish.ack_abandoned, (UINT)publish.telemetry_failed,
                  (UINT)publish.route_failed);
#endif
#else
        if ((ir_error == E_OK) && (motion_error == E_OK))
        {
            colour_report_t colour = {0};
            bool colour_available = subsystem1_colour_latest(&colour);
            now = app_now_ms();
            ir_age = now - ir.sampled_ms;
            motion_age = now - motion.sampled_ms;
#if TM_WIFI_CYW43
            T_CYW43_UTK_STATUS radio;
            cyw43_utk_get_status(&radio);
            radio_ready = (UINT)radio.ready;
#endif
            tm_printf((UB *)"t=%u IR[L=%u R=%u valid=%u err=%d age=%u] "
                      "COLOUR[read=%u L=%u/%u R=%u/%u t=%u drop=%u] "
                      "ENC[L=%d R=%d AB=%u/%u "
#if AAI05_ENCODER_PIO
                      "sat=%u age=%u] ENC_CAPTURE=PIO PIO=%u "
                      "ENC_READY=%u err=%d PIO_BAD=%u/%u "
#else
                      "bad=%u/%u sat=%u age=%u] "
                      "ENC_READY=%u err=%d irq=%u over=%u/%u "
#endif
                      "ENC_UNCERTAIN=%u "
#if MOTOR_TEST_PROBE
                      "MOTOR_MODE=PROBE "
#endif
#if AAI05_MOTION_APP
                      "MOTION[state=%s L=%d R=%d lease=%u seq=%u epoch=%u "
                      "reason=%u cutoff=%u err=%d] "
#else
                      "MOTOR[phase=%s next=%u M=%u D=%u pct=%u us=%u cutoff=%u stop=%u err=%d] "
#endif
                      "DRIVE=%s RADIO_READY=%u"
#if TM_NET_LWIP
#if AAI05_REMOTE_APP
                      " NETWORK_MODE=REMOTE_MOTION WIFI_LINK=%d WIFI_IP=%u.%u.%u.%u"
#elif AAI05_COMMS_APP
                      " NETWORK_MODE=COMMS_APP WIFI_LINK=%d WIFI_IP=%u.%u.%u.%u"
#else
                      " NETWORK_MODE=TRANSPORT_PROBE WIFI_LINK=%d WIFI_IP=%u.%u.%u.%u"
#endif
#endif
                      "\n",
                      (UINT)now, (UINT)ir.left_raw, (UINT)ir.right_raw,
                      (UINT)(ir.valid && (ir.sequence != 0U) && (ir_age <= 100U)),
                      (INT)ir.error, (UINT)ir_age,
                      (UINT)colour_available,
                      (UINT)colour_result_class(colour.observation.result[0]),
                      (UINT)colour_result_reason(colour.observation.result[0]),
                      (UINT)colour_result_class(colour.observation.result[1]),
                      (UINT)colour_result_reason(colour.observation.result[1]),
                      (UINT)colour.observation.sampled_ms, (UINT)colour.dropped,
                      (INT)motion.left_count, (INT)motion.right_count,
                      (UINT)motion.left_ab, (UINT)motion.right_ab,
#if AAI05_ENCODER_PIO
                      (UINT)motion.count_saturated, (UINT)motion_age,
                      (UINT)motion.pio_block,
                      (UINT)motion.capture_ready, (INT)motion.capture_error,
                      (UINT)((motion.pio_invalid_flags & 1U) != 0U),
                      (UINT)((motion.pio_invalid_flags & 2U) != 0U),
#else
                      (UINT)motion.left_invalid, (UINT)motion.right_invalid,
                      (UINT)motion.count_saturated, (UINT)motion_age,
                      (UINT)motion.capture_ready, (INT)motion.capture_error,
                      (UINT)motion.irq_count, (UINT)motion.left_overrun,
                      (UINT)motion.right_overrun,
#endif
                      (UINT)((motion.left_invalid != 0U) ||
                             (motion.right_invalid != 0U) ||
                             (motion.left_overrun != 0U) ||
                             (motion.right_overrun != 0U) ||
                             motion.count_saturated ||
                             (motion.pio_invalid_flags != 0U) ||
                             (motion.capture_error != E_OK) ||
                             !motion.capture_ready),
#if AAI05_MOTION_APP
                      motor_phase_name(motion.motion.phase),
                      (INT)motion.motion.left_pct, (INT)motion.motion.right_pct,
                      (UINT)motion.motion.lease_ms, (UINT)motion.motion.sequence,
                      (UINT)motion.motion.epoch,
                      (UINT)motion.motion.reason,
                      (UINT)motion.motor.cutoff_count, (INT)motion.motor.error,
#else
                      motor_phase_name(motion.motor_phase),
                      (UINT)(motion.motor_step < 4U ? motion.motor_step + 1U : 0U),
                      (UINT)motion.motor.wheel, (UINT)motion.motor.direction,
                      (UINT)motion.motor.duty, (UINT)motion.motor.elapsed_us,
                      (UINT)motion.motor.cutoff_count, (UINT)motion.motor_stop_reason,
                      (INT)motion.motor.error,
#endif
                      (!MOTOR_TEST_PROBE && motion.motor.active) ? "ON" : "OFF", radio_ready
#if TM_NET_LWIP
                      , (INT)radio.link_status, (UINT)(radio.ip_addr & 0xFFU),
                      (UINT)((radio.ip_addr >> 8U) & 0xFFU),
                      (UINT)((radio.ip_addr >> 16U) & 0xFFU),
                      (UINT)((radio.ip_addr >> 24U) & 0xFFU)
#endif
                      );
        }
        else
        {
            tm_printf((UB *)
#if MOTOR_TEST_PROBE
                      "MOTOR_MODE=PROBE DRIVE=OFF "
#endif
                      "DIAG snapshot busy/error: IR=%d motion=%d\n",
                      (INT)ir_error, (INT)motion_error);
        }
#endif
#if AAI05_RGB_ACTIVE
        rgb_diag_report();
#endif
        (void)tk_dly_tsk(250);
    }
}

EXPORT ER subsystem3_init(void)
{
    return E_OK;
}

EXPORT ER subsystem3_start(void)
{
    ID task = app_start_task((FP)report_task, APP_PRI_REPORT, 0U, "report");
    return (task < E_OK) ? task : E_OK;
}
