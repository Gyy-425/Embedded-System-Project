#include "subsystem3.h"
#if TM_WIFI_CYW43
#include "cyw43_utk.h"
#endif

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
                      "ENC[L=%d R=%d AB=%u/%u bad=%u/%u sat=%u age=%u] "
                      "ENC_READY=%u err=%d irq=%u over=%u/%u DRIVE=OFF RADIO_READY=%u\n",
                      (UINT)now, (UINT)ir.left_raw, (UINT)ir.right_raw,
                      (UINT)(ir.valid && (ir.sequence != 0U) && (ir_age <= 100U)),
                      (INT)ir.error, (UINT)ir_age,
                      (INT)motion.left_count, (INT)motion.right_count,
                      (UINT)motion.left_ab, (UINT)motion.right_ab,
                      (UINT)motion.left_invalid, (UINT)motion.right_invalid,
                      (UINT)motion.count_saturated, (UINT)motion_age,
                      (UINT)motion.capture_ready, (INT)motion.capture_error,
                      (UINT)motion.irq_count, (UINT)motion.left_overrun,
                      (UINT)motion.right_overrun, radio_ready);
        } else {
            tm_printf((UB *)"DIAG snapshot busy/error: IR=%d motion=%d\n",
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
