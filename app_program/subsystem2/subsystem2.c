#include "subsystem2.h"
#include "motor_io.h"
#if AAI05_FOLLOW_APP
#include "black_follow_owner.h"
#include "follow_snapshot.h"
#if AAI05_FOLLOW_MQTT
#include "../subsystem1/rgb_classifier.h"
#endif
#else

static ID snapshot_mutex;
static motion_snapshot_t latest;
#endif

static ER configure_test_buttons(void)
{
    const UINT pins[] = {MOTOR_NEXT_PIN, MOTOR_STOP_PIN};
    for (UINT i = 0U; i < 2U; ++i) {
        ER error = gpio_set_pin(pins[i], GPIO_MODE_IN);
        if (error < E_OK) {
            return error;
        }
        clr_w(GPIO(pins[i]), GPIO_PDE);
        set_w(GPIO(pins[i]), GPIO_PUE);
    }
    return E_OK;
}

static void motion_task(INT start_code, void *extra)
{
    motion_snapshot_t next = {0};
    encoder_capture_t capture = {.pio_block = UINT8_MAX};
#if AAI05_FOLLOW_APP
    black_follow_owner_t follower;
#if AAI05_FOLLOW_MQTT
    route_motion_sample_t last_route_sample = {0};
    bool was_following = false;
#endif
#elif !AAI05_MOTION_APP
    motor_test_t test;
#endif
    motor_pulse_status_t motor;
    ER buttons_error;
    ER motor_error;
    (void)start_code;
    (void)extra;
    next.capture_error = (ER)encoder_capture_start();
    next.capture_ready = (next.capture_error == E_OK);
    buttons_error = configure_test_buttons();
    motor_error = (ER)motor_pulse_init();
#if AAI05_FOLLOW_APP
    black_follow_owner_init(&follower);
#elif !AAI05_MOTION_APP
    motor_test_init(&test, app_now_ms());
#endif
    for (;;) {
#if AAI05_FOLLOW_MQTT
        uint32_t capture_started_ms = app_now_ms();
#endif
        if (next.capture_ready)
        {
            encoder_capture_read(&capture);
            next.capture_error = capture.error;
        }
#if AAI05_FOLLOW_MQTT
        uint32_t capture_finished_ms = app_now_ms();
#endif
        motor_pulse_read(&motor);
        uint32_t now = app_now_ms();
        UW buttons = in_w(GPIO_IN);
#if AAI05_REMOTE_APP || AAI05_FOLLOW_APP || AAI05_DISTANCE_RUN
        bool healthy = next.capture_ready && (next.capture_error == E_OK) && motor.ready &&
            ((motor.error == E_OK) || (motor.error == E_TMOUT)) &&
            (buttons_error == E_OK) && (motor_error == E_OK) &&
            !capture.left.count_saturated && !capture.right.count_saturated;
#if AAI05_DISTANCE_RUN || AAI05_FOLLOW_MQTT
        healthy = healthy && (capture.pio_invalid_flags == 0U);
#endif
#else
        bool healthy = motor_test_inputs_ready(&capture, &motor,
                            next.capture_ready, buttons_error, motor_error);
#endif
#if AAI05_FOLLOW_APP
        black_follow_owner_step(&follower, (uint32_t)buttons, healthy, &motor,
                                  &next.follow, &next.motion);
#if AAI05_FOLLOW_MQTT
        bool following = next.follow.phase == FOLLOW_RUNNING;
        bool started = following && !was_following;
        if (started)
        {
            next.route_epoch = next.motion.epoch;
        }
        bool valid = next.capture_ready && capture.error == E_OK &&
            !capture.left.count_saturated && !capture.right.count_saturated &&
            capture.pio_invalid_flags == 0U;
        const route_motion_sample_t *p_route_sample = NULL;
        if (!valid || capture_started_ms == capture_finished_ms)
        {
            last_route_sample = (route_motion_sample_t){
                .sampled_ms = capture_started_ms, .epoch = 1U,
                .left_count = capture.left.count, .right_count = capture.right.count,
                .valid = valid
            };
            p_route_sample = &last_route_sample;
        }
        else if (started && last_route_sample.valid &&
                 capture_started_ms - last_route_sample.sampled_ms <= 60U)
        {
            /* Last coherent, stationary reading when a tick spanned this capture. */
            p_route_sample = &last_route_sample;
        }
        rgb_classification_t classified = rgb_classify(rgb_default_calibration(),
            &follower.rgb, app_now_ms(), RGB_GAIN, RGB_INTEGRATION_US);
        uint8_t colour = rgb_classification_reason(classified) == RGB_CLASS_MATCH ?
                         rgb_classification_class(classified) : RGB_CLASS_UNKNOWN;
        follow_route_step(following, next.route_epoch, p_route_sample,
                          &follower.rgb, colour, &next.route);
        if (next.route.status == UINT8_MAX)
        {
            next.route.colour = colour;
        }
        was_following = following;
#endif
#elif AAI05_MOTION_APP
        motion_runtime_step(now, (uint32_t)buttons, healthy, motor.active,
                              &next.motion);
#else
        uint8_t action = motor_test_update(&test, now,
                            (buttons & (1U << MOTOR_NEXT_PIN)) == 0U,
                            (buttons & (1U << MOTOR_STOP_PIN)) == 0U,
                            healthy, motor.active);
        if (action == MOTOR_TEST_START) {
            uint8_t wheel = test.step < 2U ? 1U : 2U;
            uint8_t direction = (uint8_t)((test.step % 2U) + 1U);
            ER error = (ER)motor_pulse_start(wheel, direction, MOTOR_PULSE_MAX_DUTY, MOTOR_PULSE_MS);
            if (error < E_OK) {
                motor_test_abort(&test, MOTOR_STOP_DRIVER);
                motor_pulse_stop();
            }
        } else if (action == MOTOR_TEST_STOP) {
            motor_pulse_stop();
        }
#endif
        motor_pulse_read(&next.motor);
        if (buttons_error < E_OK || motor_error < E_OK) {
            next.motor.error = buttons_error < E_OK ? buttons_error : motor_error;
        }
#if !AAI05_MOTION_APP && !AAI05_FOLLOW_APP
        next.motor_phase = test.phase;
        next.motor_step = test.step;
        next.motor_stop_reason = test.stop_reason;
#endif
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
        next.pio_invalid_flags = capture.pio_invalid_flags;
        next.pio_block = capture.pio_block;
        next.sampled_ms = now;
        ++next.sequence;
#if AAI05_FOLLOW_APP
        next.sampled_ms = app_now_ms();
        follow_snapshot_publish(&next);
#else
        if (tk_loc_mtx(snapshot_mutex, TMO_FEVR) == E_OK) {
            latest = next;
            (void)tk_unl_mtx(snapshot_mutex);
        }
#endif
        /* Snapshot publication; acquisition runs in GPIO IRQ or PIO hardware. */
        (void)tk_dly_tsk(10);
    }
}

EXPORT ER subsystem2_init(void)
{
#if !AAI05_FOLLOW_APP
    T_CMTX attributes = {0};
#endif
    drv_motor_coast_init();
    ER timer_error = (ER)motor_timebase_init();
    if (timer_error < E_OK) {
        return timer_error;
    }
    ER runtime_error = (ER)motion_runtime_init();
    if (runtime_error < E_OK) {
        return runtime_error;
    }
#if AAI05_FOLLOW_APP
    return (ER)follow_snapshot_init();
#else
    attributes.mtxatr = TA_INHERIT;
    snapshot_mutex = tk_cre_mtx(&attributes);
    return (snapshot_mutex < E_OK) ? snapshot_mutex : E_OK;
#endif
}

EXPORT ER subsystem2_start(void)
{
    ID task = app_start_task((FP)motion_task, APP_PRI_MOTION, 1U, "motion diagnostics");
    return (task < E_OK) ? task : E_OK;
}

EXPORT ER subsystem2_get_snapshot(motion_snapshot_t *snapshot)
{
#if AAI05_FOLLOW_APP
    return (ER)follow_snapshot_read(snapshot);
#else
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
#endif
}
