#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include "../app.h"
#pragma GCC diagnostic pop
#include "motion_runtime.h"
#include "motion_button.h"
#include "motor_pulse.h"

motion_result_t subsystem2_motion_submit_chassis(
    const motion_chassis_command_t *p_request)
{
    motion_command_t command;
    motion_result_t result = motion_chassis_convert(p_request, &command);
    if (result == MOTION_ACCEPTED)
    {
        result = subsystem2_motion_submit(&command);
    }
    return result;
}

#if AAI05_FOLLOW_MQTT
/* follow_remote.c owns this profile's Motion command boundary. */
#elif AAI05_MOTION_APP
#if TK_SUPPORT_SMP
#include <tk/smp.h>
static T_SPLOCK g_motion_runtime_lock;
#endif
static motion_control_t g_motion_runtime_control;
#if !AAI05_REMOTE_APP
static motion_button_t g_motion_runtime_button;
#else
static bool g_motion_runtime_arm_pending;
#endif
static uint16_t g_motion_runtime_epoch;
static bool g_motion_runtime_initialized;
static bool g_motion_runtime_available;
#if AAI05_DISTANCE_RUN
static uint32_t g_distance_deadline_us;
#endif

_Static_assert(MOTION_COMMAND_TTL_MS == MOTOR_DRIVE_MAX_LEASE_MS,
               "Motion and driver lease limits must match");
_Static_assert(MOTION_MAX_DUTY == MOTOR_PULSE_MAX_DUTY,
               "Motion and driver duty limits must match");

static UINT motion_runtime_lock(void)
{
    UINT saved;
#if TK_SUPPORT_SMP
    (void)ISpinLock(&g_motion_runtime_lock, &saved);
#else
    DI(saved);
#endif
    return saved;
}

static void motion_runtime_unlock(UINT saved)
{
#if TK_SUPPORT_SMP
    (void)ISpinUnlock(&g_motion_runtime_lock, saved);
#else
    EI(saved);
#endif
}

int32_t motion_runtime_init(void)
{
#if TK_SUPPORT_SMP
    ER error = InitSpinLock(&g_motion_runtime_lock);
    if (error != E_OK)
    {
        return error;
    }
#endif
    motion_control_init(&g_motion_runtime_control);
#if !AAI05_REMOTE_APP
    motion_button_init(&g_motion_runtime_button, app_now_ms());
#else
    g_motion_runtime_arm_pending = false;
#endif
    g_motion_runtime_epoch = 0U;
    g_motion_runtime_available = false;
    g_motion_runtime_initialized = true;
    return E_OK;
}

motion_result_t subsystem2_motion_submit(const motion_command_t *p_command)
{
    if (!g_motion_runtime_initialized)
    {
        return MOTION_UNAVAILABLE;
    }
    UINT saved = motion_runtime_lock();
    bool is_available = g_motion_runtime_available;
#if AAI05_REMOTE_APP
    is_available = is_available && !g_motion_runtime_arm_pending;
#endif
    motion_result_t result = is_available ?
        motion_control_submit(&g_motion_runtime_control, p_command,
                               app_now_ms()) : MOTION_UNAVAILABLE;
    motion_runtime_unlock(saved);
    return result;
}

motion_result_t subsystem2_motion_stop(void)
{
    if (!g_motion_runtime_initialized)
    {
        return MOTION_UNAVAILABLE;
    }
    UINT saved = motion_runtime_lock();
#if AAI05_REMOTE_APP
    if (g_motion_runtime_arm_pending)
    {
        g_motion_runtime_arm_pending = false;
        ++g_motion_runtime_epoch; /* Acknowledge cancellation, never rearm. */
    }
#endif
    motion_control_stop(&g_motion_runtime_control, MOTION_REASON_REQUEST);
    motion_runtime_unlock(saved);
    return MOTION_ACCEPTED;
}

motion_result_t subsystem2_motion_arm(uint16_t *p_epoch)
{
#if AAI05_REMOTE_APP
    if (p_epoch == NULL)
    {
        return MOTION_BAD_COMMAND;
    }
    if (!g_motion_runtime_initialized)
    {
        return MOTION_UNAVAILABLE;
    }
    UINT saved = motion_runtime_lock();
    motion_result_t result = MOTION_ACCEPTED;
    if (!g_motion_runtime_available || g_motion_runtime_arm_pending)
    {
        result = MOTION_UNAVAILABLE;
    }
    else if ((g_motion_runtime_control.phase == MOTION_RUNNING) ||
             (g_motion_runtime_control.phase == MOTION_FAULT))
    {
        result = MOTION_LATCHED;
    }
    else
    {
        g_motion_runtime_arm_pending = true;
        *p_epoch = (uint16_t)(g_motion_runtime_epoch + 1U);
    }
    motion_runtime_unlock(saved);
    return result;
#else
    (void)p_epoch;
    return MOTION_UNAVAILABLE;
#endif
}

void motion_runtime_step(uint32_t now_ms, uint32_t buttons,
                          bool is_hardware_ready, bool is_driver_active,
                          motion_status_t *p_status)
{
#if !AAI05_REMOTE_APP
    motion_command_t command;
#endif
    motion_output_t output;
    UINT saved = motion_runtime_lock();
    /* A producer on the other core may have accepted a newer clock sample. */
    now_ms = app_now_ms();
    bool is_stop_pressed = (buttons & (1U << MOTOR_STOP_PIN)) == 0U;
#if AAI05_REMOTE_APP
    if (g_motion_runtime_arm_pending)
    {
        g_motion_runtime_arm_pending = false;
        ++g_motion_runtime_epoch;
        motor_pulse_stop();
        is_driver_active = false;
        if (is_stop_pressed)
        {
            motion_control_stop(&g_motion_runtime_control, MOTION_REASON_BUTTON);
        }
        else if (!is_hardware_ready ||
                 (g_motion_runtime_control.phase == MOTION_FAULT) ||
                 (motor_drive_rearm() != E_OK))
        {
            g_motion_runtime_control.phase = MOTION_FAULT;
            g_motion_runtime_control.reason = MOTION_REASON_HARDWARE;
            g_motion_runtime_control.has_command = false;
        }
        else
        {
            motion_control_init(&g_motion_runtime_control);
        }
    }
#else
    bool is_next_pressed = (buttons & (1U << MOTOR_NEXT_PIN)) == 0U;
    bool is_available = is_hardware_ready &&
        ((g_motion_runtime_control.phase == MOTION_READY) ||
         (g_motion_runtime_control.phase == MOTION_RUNNING));
    if (motion_button_update(&g_motion_runtime_button, now_ms,
                              is_next_pressed, is_available, &command))
    {
#if AAI05_DISTANCE_RUN
        if (command.sequence == 1U)
        {
            /* Later renewals can never extend the hardware beyond this bound. */
            g_distance_deadline_us = motor_drive_now_us() +
                                     MOTION_BUTTON_RUN_MS * 1000U;
        }
#endif
        (void)motion_control_submit(&g_motion_runtime_control, &command,
                                     now_ms);
    }
#endif
    motion_control_step(&g_motion_runtime_control, now_ms, is_stop_pressed,
                          is_hardware_ready, is_driver_active, &output);
    if (output.lease_ms == 0U)
    {
        motor_pulse_stop();
    }
    else
    {
        int32_t error;
#if AAI05_DISTANCE_RUN
        if (g_motion_runtime_button.was_started)
        {
            uint32_t now_us = motor_drive_now_us();
            uint32_t remaining_us = g_distance_deadline_us - now_us;
            uint32_t lease_us = (uint32_t)output.lease_ms * 1000U;
            if (remaining_us == 0U || remaining_us > MOTION_BUTTON_RUN_MS * 1000U)
            {
                error = E_TMOUT;
            }
            else
            {
                /* The driver rejects deadlines more than one lease away. */
                if (lease_us > remaining_us)
                {
                    lease_us = remaining_us;
                }
                error = motor_drive_set_until(output.left_pct, output.right_pct,
                                               now_us + lease_us);
            }
        }
        else
#endif
        {
            error = motor_drive_set(output.left_pct, output.right_pct,
                                     output.lease_ms);
        }
        if (error != E_OK)
        {
            if (error == E_TMOUT)
            {
                motion_control_stop(&g_motion_runtime_control,
                                      MOTION_REASON_EXPIRED);
            }
            else
            {
                g_motion_runtime_control.phase = MOTION_FAULT;
                g_motion_runtime_control.reason = MOTION_REASON_HARDWARE;
                g_motion_runtime_control.has_command = false;
            }
            output = (motion_output_t){0};
            motor_pulse_stop();
        }
    }
    *p_status = (motion_status_t){
        .issued_ms = g_motion_runtime_control.command.issued_ms,
        .sequence = g_motion_runtime_control.command.sequence,
        .left_pct = output.left_pct,
        .right_pct = output.right_pct,
        .epoch = g_motion_runtime_epoch,
        .phase = g_motion_runtime_control.phase,
        .reason = g_motion_runtime_control.reason,
        .lease_ms = output.lease_ms,
        .has_sequence = g_motion_runtime_control.has_sequence};
    /* Publish acceptance readiness after fault/STOP processing as well. */
    g_motion_runtime_available = is_hardware_ready;
    motion_runtime_unlock(saved);
}
#else
int32_t motion_runtime_init(void)
{
    return 0;
}

motion_result_t subsystem2_motion_submit(const motion_command_t *p_command)
{
    (void)p_command;
    return MOTION_UNAVAILABLE;
}

motion_result_t subsystem2_motion_stop(void)
{
    return MOTION_UNAVAILABLE;
}

motion_result_t subsystem2_motion_arm(uint16_t *p_epoch)
{
    (void)p_epoch;
    return MOTION_UNAVAILABLE;
}
#endif
