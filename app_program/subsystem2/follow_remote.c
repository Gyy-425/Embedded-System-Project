#if AAI05_FOLLOW_MQTT
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include "../app.h"
#include <tk/smp.h>
#pragma GCC diagnostic pop
#include "follow_remote.h"
#include "motion_runtime.h"

static T_SPLOCK remote_lock;
static follow_remote_permission_t permission;
static bool initialized;

int32_t motion_runtime_init(void)
{
    ER error = InitSpinLock(&remote_lock);
    initialized = error == E_OK;
    return error;
}

motion_result_t subsystem2_follow_arm(uint16_t *p_epoch, bool straight)
{
    if (p_epoch == NULL)
    {
        return MOTION_BAD_COMMAND;
    }
    if (!initialized)
    {
        return MOTION_UNAVAILABLE;
    }
    UINT saved;
    (void)ISpinLock(&remote_lock, &saved);
    motion_result_t result = MOTION_LATCHED;
    if (!permission.armed)
    {
        permission = (follow_remote_permission_t){
            .epoch = (uint16_t)(permission.epoch + 1U),
            .armed = true,
            .start = true,
            .straight = straight
        };
        *p_epoch = permission.epoch;
        result = MOTION_ACCEPTED;
    }
    (void)ISpinUnlock(&remote_lock, saved);
    return result;
}

motion_result_t subsystem2_motion_submit(const motion_command_t *p_command)
{
    if (p_command == NULL || p_command->left_pct != 0 || p_command->right_pct != 0)
    {
        return MOTION_BAD_COMMAND;
    }
    if (!initialized)
    {
        return MOTION_UNAVAILABLE;
    }
    UINT saved;
    (void)ISpinLock(&remote_lock, &saved);
    uint32_t now_ms = app_now_ms();
    motion_result_t result = MOTION_LATCHED;
    if (permission.armed &&
        (!permission.fed || follow_remote_remaining(permission, now_ms) != 0U))
    {
        if (now_ms - p_command->issued_ms >= MOTION_COMMAND_TTL_MS)
        {
            result = MOTION_STALE;
        }
        else
        {
            permission.issued_ms = p_command->issued_ms;
            permission.fed = true;
            result = MOTION_ACCEPTED;
        }
    }
    (void)ISpinUnlock(&remote_lock, saved);
    return result;
}

motion_result_t subsystem2_motion_stop(void)
{
    if (!initialized)
    {
        return MOTION_UNAVAILABLE;
    }
    UINT saved;
    (void)ISpinLock(&remote_lock, &saved);
    permission.armed = false;
    (void)ISpinUnlock(&remote_lock, saved);
    return MOTION_ACCEPTED;
}

follow_remote_permission_t follow_remote_take(void)
{
    UINT saved;
    (void)ISpinLock(&remote_lock, &saved);
    follow_remote_permission_t result = permission;
    result.start = permission.start &&
                   follow_remote_remaining(permission, app_now_ms()) != 0U;
    if (result.start)
    {
        permission.start = false;
    }
    (void)ISpinUnlock(&remote_lock, saved);
    return result;
}
#endif
