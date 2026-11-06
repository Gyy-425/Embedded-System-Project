#include "subsystem2.h"
#include "motor_io.h"

static ID snapshot_mutex;
static motion_snapshot_t latest;

static void motion_task(INT start_code, void *extra)
{
    motion_snapshot_t next = {0};
    encoder_capture_t capture = {0};
    (void)start_code;
    (void)extra;
    next.capture_error = (ER)encoder_capture_start();
    next.capture_ready = (next.capture_error == E_OK);
    for (;;) {
        if (next.capture_ready) encoder_capture_read(&capture);
        next.left_count = capture.left.count;
        next.right_count = capture.right.count;
        next.left_invalid = capture.left.invalid_transitions;
        next.right_invalid = capture.right.invalid_transitions;
        next.left_ab = capture.left.ab;
        next.right_ab = capture.right.ab;
        next.count_saturated = capture.left.count_saturated || capture.right.count_saturated;
        next.irq_count = capture.irq_count;
        next.left_overrun = capture.left_overrun;
        next.right_overrun = capture.right_overrun;
        next.sampled_ms = app_now_ms();
        ++next.sequence;
        if (tk_loc_mtx(snapshot_mutex, TMO_FEVR) == E_OK) {
            latest = next;
            (void)tk_unl_mtx(snapshot_mutex);
        }
        /* Snapshot publication only; edge acquisition runs in the handler. */
        (void)tk_dly_tsk(10);
    }
}

EXPORT ER subsystem2_init(void)
{
    T_CMTX attributes = {0};
    drv_motor_coast_init();
    attributes.mtxatr = TA_INHERIT;
    snapshot_mutex = tk_cre_mtx(&attributes);
    return (snapshot_mutex < E_OK) ? snapshot_mutex : E_OK;
}

EXPORT ER subsystem2_start(void)
{
    ID task = app_start_task((FP)motion_task, APP_PRI_MOTION, 1U, "motion diagnostics");
    return (task < E_OK) ? task : E_OK;
}

EXPORT ER subsystem2_get_snapshot(motion_snapshot_t *snapshot)
{
    ER error;
    if (snapshot == NULL) {
        return E_PAR;
    }
    error = tk_loc_mtx(snapshot_mutex, TMO_POL);
    if (error == E_OK) {
        *snapshot = latest;
        (void)tk_unl_mtx(snapshot_mutex);
    }
    return error;
}
