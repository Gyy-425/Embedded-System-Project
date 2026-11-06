#include "subsystem2.h"
#include "motor_io.h"

static ID snapshot_mutex;
static encoder_state_t left_encoder;
static encoder_state_t right_encoder;
static motion_snapshot_t latest;

static uint8_t ab_state(UW levels, UINT pin_a, UINT pin_b)
{
    return (uint8_t)((((levels >> pin_a) & 1U) << 1U) |
                     ((levels >> pin_b) & 1U));
}

static void motion_task(INT start_code, void *extra)
{
    motion_snapshot_t next = {0};
    UW levels;
    (void)start_code;
    (void)extra;
    for (;;) {
        levels = in_w(GPIO_IN);
        encoder_update(&left_encoder, ab_state(levels, APP_ENCODER_LEFT_A, APP_ENCODER_LEFT_B));
        encoder_update(&right_encoder, ab_state(levels, APP_ENCODER_RIGHT_A, APP_ENCODER_RIGHT_B));
        next.left_count = left_encoder.count;
        next.right_count = right_encoder.count;
        next.left_invalid = left_encoder.invalid_transitions;
        next.right_invalid = right_encoder.invalid_transitions;
        next.left_ab = left_encoder.ab;
        next.right_ab = right_encoder.ab;
        next.count_saturated = left_encoder.count_saturated || right_encoder.count_saturated;
        next.sampled_ms = app_now_ms();
        ++next.sequence;
        if (tk_loc_mtx(snapshot_mutex, TMO_FEVR) == E_OK) {
            latest = next;
            (void)tk_unl_mtx(snapshot_mutex);
        }
        /* Wiring diagnostic only: slowly rotate wheels by hand, not at motor speed. */
        (void)tk_dly_tsk(1);
    }
}

EXPORT ER subsystem2_init(void)
{
    T_CMTX attributes = {0};
    const UINT encoder_pins[] = {APP_ENCODER_LEFT_A, APP_ENCODER_LEFT_B,
                                APP_ENCODER_RIGHT_A, APP_ENCODER_RIGHT_B};
    UW levels;
    ER error;
    drv_motor_coast_init();
    for (UINT i = 0U; i < 4U; ++i) {
        error = gpio_set_pin(encoder_pins[i], GPIO_MODE_IN);
        if (error < E_OK) {
            return error;
        }
    }
    levels = in_w(GPIO_IN);
    encoder_init(&left_encoder, ab_state(levels, APP_ENCODER_LEFT_A, APP_ENCODER_LEFT_B));
    encoder_init(&right_encoder, ab_state(levels, APP_ENCODER_RIGHT_A, APP_ENCODER_RIGHT_B));
    attributes.mtxatr = TA_INHERIT;
    snapshot_mutex = tk_cre_mtx(&attributes);
    return (snapshot_mutex < E_OK) ? snapshot_mutex : E_OK;
}

EXPORT ER subsystem2_start(void)
{
    ID task = app_start_task((FP)motion_task, APP_PRI_MOTION, 0U, "motion diagnostics");
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
