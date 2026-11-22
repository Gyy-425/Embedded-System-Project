#include "drive_control.h"
#include <stddef.h>

static bool drive_control_is_running(const drive_control_t *p_control)
{
    return (p_control->phase == DRIVE_ARMING) ||
           (p_control->phase == DRIVE_RUNNING);
}

static void drive_control_rotate_grant(drive_control_t *p_control,
                                        uint32_t now_ms)
{
    ++p_control->grant;
    if (p_control->grant == 0U)
    {
        ++p_control->grant;
    }
    p_control->grant_ms = now_ms;
}

static bool drive_control_expire(drive_control_t *p_control, uint32_t now_ms,
                                  drive_action_t *p_action)
{
    if (drive_control_is_running(p_control) &&
        ((uint32_t)(now_ms - p_control->renewed_ms) >= DRIVE_LEASE_MS))
    {
        drive_control_halt(p_control, now_ms, false);
        p_action->kind = DRIVE_ACTION_STOP;
        return true;
    }
    return false;
}

static void drive_control_refresh_grant(drive_control_t *p_control,
                                         uint32_t now_ms)
{
    if (p_control->is_ready &&
        ((uint32_t)(now_ms - p_control->grant_ms) >= DRIVE_GRANT_MS))
    {
        drive_control_rotate_grant(p_control, now_ms);
    }
}

static bool drive_control_same(const drive_command_t *p_left,
                                const drive_command_t *p_right)
{
    return (p_left->session == p_right->session) &&
           (p_left->grant == p_right->grant) &&
           (p_left->opcode == p_right->opcode) &&
           (p_left->target == p_right->target) &&
           (p_left->left_pct == p_right->left_pct) &&
           (p_left->right_pct == p_right->right_pct);
}

void drive_control_reset(drive_control_t *p_control, uint32_t session,
                          uint32_t grant, uint32_t now_ms, bool is_ready)
{
    *p_control = (drive_control_t){.session = session, .grant = grant,
        .grant_ms = now_ms, .target = DRIVE_UNKNOWN,
        .phase = is_ready ? DRIVE_IDLE : DRIVE_STOPPED,
        .is_ready = is_ready && (session != 0U) && (grant != 0U)};
}

drive_result_t drive_control_receive(drive_control_t *p_control,
                                      const drive_command_t *p_command,
                                      bool is_retained, uint32_t now_ms,
                                      drive_action_t *p_action)
{
    *p_action = (drive_action_t){0};
    if ((p_control == NULL) || (p_command == NULL))
    {
        return DRIVE_BAD_VALUE;
    }
    if (is_retained)
    {
        return DRIVE_RETAINED;
    }
    if (!p_control->is_ready)
    {
        return DRIVE_NOT_READY;
    }
    (void)drive_control_expire(p_control, now_ms, p_action);
    drive_control_refresh_grant(p_control, now_ms);
    if (p_command->opcode == DRIVE_OP_STATUS)
    {
        return DRIVE_OK;
    }
    if (p_command->session != p_control->session)
    {
        return DRIVE_SESSION;
    }
    if (p_command->opcode == DRIVE_OP_STOP)
    {
        bool was_stopped = (p_control->phase == DRIVE_STOPPED) ||
                           (p_control->phase == DRIVE_FAULT);
        drive_control_halt(p_control, now_ms, false);
        p_action->kind = DRIVE_ACTION_STOP;
        return was_stopped ? DRIVE_DUPLICATE : DRIVE_OK;
    }
    if (p_control->has_sequence)
    {
        uint16_t delta = (uint16_t)((uint32_t)p_command->sequence -
                                     p_control->previous.sequence);
        if (delta == 0U)
        {
            return drive_control_same(p_command, &p_control->previous) ?
                   DRIVE_DUPLICATE : DRIVE_CONFLICT;
        }
        if (delta >= UINT16_C(32768))
        {
            return DRIVE_STALE_SEQUENCE;
        }
    }
    if ((p_command->grant != p_control->grant) ||
        ((uint32_t)(now_ms - p_control->grant_ms) >= DRIVE_GRANT_MS))
    {
        return DRIVE_STALE_GRANT;
    }
#if AAI05_FOLLOW_MQTT
    bool is_start = (p_command->opcode == DRIVE_OP_FOLLOW) ||
                    (p_command->opcode == DRIVE_OP_STRAIGHT);
    if (!is_start && p_command->opcode != DRIVE_OP_KEEPALIVE)
    {
        return DRIVE_BAD_VALUE;
    }
#else
    bool is_start = p_command->opcode == DRIVE_OP_START;
#endif
    bool is_keep = p_command->opcode == DRIVE_OP_KEEPALIVE;
    if ((is_keep && (p_control->phase != DRIVE_RUNNING)) ||
        (is_start && (p_control->phase != DRIVE_IDLE) &&
         (p_control->phase != DRIVE_STOPPED)) ||
        (!is_start && !is_keep && drive_control_is_running(p_control)))
    {
        return DRIVE_BAD_STATE;
    }
    if (is_start)
    {
        p_control->left_pct = p_command->left_pct;
        p_control->right_pct = p_command->right_pct;
        p_control->phase = DRIVE_ARMING;
        p_control->is_arm_queued = false;
        p_control->renewed_ms = now_ms;
        p_action->kind = DRIVE_ACTION_ARM;
#if AAI05_FOLLOW_MQTT
        p_action->straight = (p_command->opcode == DRIVE_OP_STRAIGHT);
#endif
    }
    else if (is_keep)
    {
        p_control->renewed_ms = now_ms;
    }
    else
    {
        p_control->target = (p_command->opcode == DRIVE_OP_TARGET) ?
                            p_command->target : DRIVE_UNKNOWN;
    }
    p_control->previous = *p_command;
    p_control->has_sequence = true;
    drive_control_rotate_grant(p_control, now_ms);
    return DRIVE_OK;
}

drive_result_t drive_control_arm_result(drive_control_t *p_control,
                                        motion_result_t result,
                                        uint16_t epoch, uint32_t now_ms)
{
    if ((p_control->phase != DRIVE_ARMING) || (result != MOTION_ACCEPTED))
    {
        drive_control_halt(p_control, now_ms, false);
        return DRIVE_MOTION_FAILURE;
    }
    p_control->expected_epoch = epoch;
    p_control->is_arm_queued = true;
    p_control->motion_sequence = 0U;
    return DRIVE_OK;
}

void drive_control_poll(drive_control_t *p_control, uint32_t now_ms,
                         bool is_wifi_up, const drive_motion_view_t *p_motion,
                         drive_action_t *p_action)
{
    *p_action = (drive_action_t){0};
    if (drive_control_expire(p_control, now_ms, p_action))
    {
        return;
    }
    drive_control_refresh_grant(p_control, now_ms);
    if (!drive_control_is_running(p_control))
    {
        return;
    }
    if (!p_control->is_ready || !is_wifi_up || !p_motion->is_available)
    {
        drive_control_halt(p_control, now_ms, false);
        p_action->kind = DRIVE_ACTION_STOP;
        return;
    }
    if (p_control->phase == DRIVE_ARMING)
    {
        if (!p_control->is_arm_queued ||
            (p_motion->status.epoch != p_control->expected_epoch))
        {
            return;
        }
        if ((p_motion->status.phase != MOTION_READY) || p_motion->is_active)
        {
            drive_control_halt(p_control, now_ms,
                                p_motion->status.phase == MOTION_FAULT);
            p_action->kind = DRIVE_ACTION_STOP;
            return;
        }
        p_control->phase = DRIVE_RUNNING;
        p_control->fed_ms = now_ms - DRIVE_FEED_MS;
    }
    if ((p_motion->status.epoch != p_control->expected_epoch) ||
        (p_motion->status.phase == MOTION_STOPPED) ||
        (p_motion->status.phase == MOTION_FAULT))
    {
        drive_control_halt(p_control, now_ms,
                            p_motion->status.phase == MOTION_FAULT);
        p_action->kind = DRIVE_ACTION_STOP;
        return;
    }
    if ((uint32_t)(now_ms - p_control->fed_ms) >= DRIVE_FEED_MS)
    {
        ++p_control->motion_sequence;
        p_action->kind = DRIVE_ACTION_SUBMIT;
        p_action->command = (motion_command_t){now_ms,
            p_control->motion_sequence, p_control->left_pct,
            p_control->right_pct};
        p_control->fed_ms = now_ms;
    }
}

void drive_control_halt(drive_control_t *p_control, uint32_t now_ms,
                         bool is_fault)
{
    uint8_t next_phase = (is_fault || (p_control->phase == DRIVE_FAULT)) ?
                         DRIVE_FAULT : DRIVE_STOPPED;
    if (p_control->phase != next_phase)
    {
        drive_control_rotate_grant(p_control, now_ms);
    }
    p_control->phase = next_phase;
    p_control->is_arm_queued = false;
    p_control->left_pct = 0;
    p_control->right_pct = 0;
}

uint16_t drive_control_lease(const drive_control_t *p_control, uint32_t now_ms)
{
    uint32_t age = now_ms - p_control->renewed_ms;
    return (drive_control_is_running(p_control) && (age < DRIVE_LEASE_MS)) ?
           (uint16_t)(DRIVE_LEASE_MS - age) : 0U;
}
