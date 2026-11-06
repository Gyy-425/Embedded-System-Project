#include "subsystem1.h"
#include "drv_ir.h"

static ID snapshot_mutex;
static ir_snapshot_t latest;

static void ir_task(INT start_code, void *extra)
{
    ir_snapshot_t next = {0};
    bool opened = false;
    ER error;
    (void)start_code;
    (void)extra;
    for (;;) {
        if (!opened) {
            error = drv_ir_open();
            opened = (error == E_OK);
        } else {
            error = E_OK;
        }
        next.valid = false;
        if (opened) {
            error = drv_ir_read(&next.left_raw, &next.right_raw);
            next.valid = (error == E_OK);
        }
        next.error = error;
        next.sampled_ms = app_now_ms();
        ++next.sequence;
        if (tk_loc_mtx(snapshot_mutex, TMO_FEVR) == E_OK) {
            latest = next;
            (void)tk_unl_mtx(snapshot_mutex);
        }
        (void)tk_dly_tsk(opened ? 10 : 100);
    }
}

EXPORT ER subsystem1_init(void)
{
    T_CMTX attributes = {0};
    attributes.mtxatr = TA_INHERIT;
    snapshot_mutex = tk_cre_mtx(&attributes);
    return (snapshot_mutex < E_OK) ? snapshot_mutex : E_OK;
}

EXPORT ER subsystem1_start(void)
{
    ID task = app_start_task((FP)ir_task, APP_PRI_IR, 0U, "ir");
    return (task < E_OK) ? task : E_OK;
}

EXPORT ER subsystem1_get_snapshot(ir_snapshot_t *snapshot)
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
