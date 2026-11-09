#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include "subsystem1.h"
#pragma GCC diagnostic pop
#include "drv_ir.h"
#include "../../config/colour_calibration_config.h"

static ID snapshot_mutex;
static ir_snapshot_t latest;
static colour_runtime_t g_ir_colour;

static void ir_sample(ir_snapshot_t *p_next, bool *p_opened)
{
    ER error = E_OK;
    if (!*p_opened)
    {
        error = drv_ir_open();
        *p_opened = (error == E_OK);
    }
    p_next->valid = false;
    if (*p_opened)
    {
        error = drv_ir_read(&p_next->left_raw, &p_next->right_raw);
        p_next->valid = (error == E_OK);
    }
    p_next->error = error;
    p_next->sampled_ms = app_now_ms();
    ++p_next->sequence;
    if (tk_loc_mtx(snapshot_mutex, TMO_FEVR) == E_OK)
    {
        colour_runtime_update(&g_ir_colour, p_next->left_raw, p_next->right_raw,
                                p_next->valid, p_next->sampled_ms, app_now_ms());
        latest = *p_next;
        (void)tk_unl_mtx(snapshot_mutex);
    }
}

static void ir_task(INT start_code, void *extra)
{
    ir_snapshot_t next = {0};
    bool opened = false;
    (void)start_code;
    (void)extra;
    for (;;) {
        ir_sample(&next, &opened);
        (void)tk_dly_tsk(opened ? 10 : 100);
    }
}

EXPORT ER subsystem1_init(void)
{
    T_CMTX attributes = {0};
    attributes.mtxatr = TA_INHERIT;
    (void)colour_runtime_init(&g_ir_colour, &g_colour_boot_calibration);
    snapshot_mutex = tk_cre_mtx(&attributes);
    return (snapshot_mutex < E_OK) ? snapshot_mutex : E_OK;
}

bool subsystem1_colour_latest(colour_report_t *p_report)
{
    if ((p_report == NULL) || (tk_loc_mtx(snapshot_mutex, TMO_POL) != E_OK))
    {
        return false;
    }
    bool available = colour_runtime_latest(&g_ir_colour, app_now_ms(), p_report);
    (void)tk_unl_mtx(snapshot_mutex);
    return available;
}

bool subsystem1_colour_pop(colour_report_t *p_report)
{
    if ((p_report == NULL) || (tk_loc_mtx(snapshot_mutex, TMO_POL) != E_OK))
    {
        return false;
    }
    bool available = colour_runtime_pop(&g_ir_colour, p_report);
    (void)tk_unl_mtx(snapshot_mutex);
    return available;
}

bool subsystem1_colour_begin(colour_report_t *p_report)
{
    if ((p_report == NULL) || (tk_loc_mtx(snapshot_mutex, TMO_POL) != E_OK))
    {
        return false;
    }
    colour_runtime_reset_events(&g_ir_colour);
    bool available = colour_runtime_latest(&g_ir_colour, app_now_ms(), p_report);
    (void)tk_unl_mtx(snapshot_mutex);
    return available;
}

bool subsystem1_colour_take(colour_report_t *p_report, bool *p_is_snapshot,
                             bool allow_snapshot)
{
    if ((p_report == NULL) || (p_is_snapshot == NULL) ||
        (tk_loc_mtx(snapshot_mutex, TMO_POL) != E_OK))
    {
        return false;
    }
    bool available = colour_runtime_pop(&g_ir_colour, p_report);
    *p_is_snapshot = false;
    if (!available && allow_snapshot)
    {
        available = colour_runtime_latest(&g_ir_colour, app_now_ms(), p_report);
        *p_is_snapshot = true;
    }
    (void)tk_unl_mtx(snapshot_mutex);
    return available;
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
