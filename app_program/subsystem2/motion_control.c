#include "motion_control.h"
#include <stddef.h>

void motion_control_init(motion_control_t *p_control)
{
    *p_control = (motion_control_t){0};
}

motion_result_t motion_control_submit(motion_control_t *p_control,
                                      const motion_command_t *p_command,
                                      uint32_t now_ms)
{
    if ((p_control == NULL) || (p_command == NULL) ||
        (p_command->left_pct < -MOTION_MAX_DUTY) ||
        (p_command->left_pct > MOTION_MAX_DUTY) ||
        (p_command->right_pct < -MOTION_MAX_DUTY) ||
        (p_command->right_pct > MOTION_MAX_DUTY))
    {
        return MOTION_BAD_COMMAND;
    }
    if ((p_control->phase == MOTION_STOPPED) ||
        (p_control->phase == MOTION_FAULT))
    {
        return MOTION_LATCHED;
    }
    if ((p_control->phase == MOTION_RUNNING) &&
        ((uint32_t)(now_ms - p_control->command.issued_ms) >=
         MOTION_COMMAND_TTL_MS))
    {
        motion_control_stop(p_control, MOTION_REASON_EXPIRED);
        return MOTION_LATCHED;
    }
    /* Future timestamps also subtract to an age outside this short window. */
    if ((uint32_t)(now_ms - p_command->issued_ms) >= MOTION_COMMAND_TTL_MS)
    {
        return MOTION_STALE;
    }
    if (p_control->has_sequence)
    {
        uint16_t delta = (uint16_t)(p_command->sequence -
                                     p_control->command.sequence);
        if (delta == 0U)
        {
            return ((p_command->issued_ms == p_control->command.issued_ms) &&
                    (p_command->left_pct == p_control->command.left_pct) &&
                    (p_command->right_pct == p_control->command.right_pct)) ?
                   MOTION_DUPLICATE : MOTION_CONFLICT;
        }
        if (delta >= UINT16_C(32768))
        {
            return MOTION_STALE;
        }
    }
    p_control->command = *p_command;
    p_control->has_command = true;
    p_control->has_sequence = true;
    return MOTION_ACCEPTED;
}

void motion_control_stop(motion_control_t *p_control, uint8_t reason)
{
    if ((p_control->phase != MOTION_STOPPED) &&
        (p_control->phase != MOTION_FAULT))
    {
        p_control->phase = MOTION_STOPPED;
        p_control->reason = reason;
        p_control->has_command = false;
    }
}

void motion_control_step(motion_control_t *p_control, uint32_t now_ms,
                          bool is_stop_pressed, bool is_hardware_ready,
                          bool is_driver_active, motion_output_t *p_output)
{
    *p_output = (motion_output_t){0};
    if (is_stop_pressed)
    {
        motion_control_stop(p_control, MOTION_REASON_BUTTON);
    }
    if ((p_control->phase == MOTION_STOPPED) ||
        (p_control->phase == MOTION_FAULT))
    {
        return;
    }
    if (!is_hardware_ready)
    {
        p_control->phase = MOTION_FAULT;
        p_control->reason = MOTION_REASON_HARDWARE;
        p_control->has_command = false;
        return;
    }
    if ((p_control->phase == MOTION_RUNNING) && !is_driver_active)
    {
        /* Never restart silently after the independent lease stopped output. */
        motion_control_stop(p_control, MOTION_REASON_EXPIRED);
        return;
    }
    if (!p_control->has_command)
    {
        return;
    }
    uint32_t age_ms = now_ms - p_control->command.issued_ms;
    if (age_ms >= MOTION_COMMAND_TTL_MS)
    {
        motion_control_stop(p_control, MOTION_REASON_EXPIRED);
        return;
    }
    if ((p_control->command.left_pct == 0) &&
        (p_control->command.right_pct == 0))
    {
        p_control->phase = MOTION_READY;
        p_control->has_command = false;
        return;
    }
    p_control->phase = MOTION_RUNNING;
    p_output->left_pct = p_control->command.left_pct;
    p_output->right_pct = p_control->command.right_pct;
    p_output->lease_ms = (uint8_t)(MOTION_COMMAND_TTL_MS - age_ms);
}
