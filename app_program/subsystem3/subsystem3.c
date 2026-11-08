#include "subsystem3.h"
#if TM_WIFI_CYW43
#include "cyw43_utk.h"
#endif

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

static void report_task(INT start_code, void *extra)
{
    ir_snapshot_t ir;
    motion_snapshot_t motion;
    uint32_t now;
    uint32_t ir_age;
    uint32_t motion_age;
    ER ir_error;
    ER motion_error;
    UINT radio_ready = 0U;
    (void)start_code;
    (void)extra;
    for (;;) {
        ir_error = subsystem1_get_snapshot(&ir);
        motion_error = subsystem2_get_snapshot(&motion);
        if ((ir_error == E_OK) && (motion_error == E_OK)) {
            now = app_now_ms();
            ir_age = now - ir.sampled_ms;
            motion_age = now - motion.sampled_ms;
#if TM_WIFI_CYW43
            T_CYW43_UTK_STATUS radio;
            cyw43_utk_get_status(&radio);
            radio_ready = (UINT)radio.ready;
#endif
            tm_printf((UB *)"t=%u IR[L=%u R=%u valid=%u err=%d age=%u] "
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
        } else {
            tm_printf((UB *)
#if MOTOR_TEST_PROBE
                      "MOTOR_MODE=PROBE DRIVE=OFF "
#endif
                      "DIAG snapshot busy/error: IR=%d motion=%d\n",
                      (INT)ir_error, (INT)motion_error);
        }
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
