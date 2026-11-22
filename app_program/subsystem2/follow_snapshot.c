#include "follow_snapshot.h"
#if AAI05_FOLLOW_APP
#include <tk/smp.h>
static T_SPLOCK g_follow_snapshot_lock;
static motion_snapshot_t g_follow_snapshot;
static bool g_follow_snapshot_ready;

int32_t follow_snapshot_init(void)
{
    int32_t error = InitSpinLock(&g_follow_snapshot_lock);
    g_follow_snapshot = (motion_snapshot_t){0};
    g_follow_snapshot_ready = error == 0;
    return error;
}

void follow_snapshot_publish(const motion_snapshot_t *p_value)
{
    if (!g_follow_snapshot_ready || (p_value == NULL))
    {
        return;
    }
    UINT saved;
    (void)ISpinLock(&g_follow_snapshot_lock, &saved);
    g_follow_snapshot = *p_value;
    (void)ISpinUnlock(&g_follow_snapshot_lock, saved);
}

int32_t follow_snapshot_read(motion_snapshot_t *p_value)
{
    if (p_value == NULL)
    {
        return E_PAR;
    }
    if (!g_follow_snapshot_ready)
    {
        return E_NOEXS;
    }
    UINT saved;
    (void)ISpinLock(&g_follow_snapshot_lock, &saved);
    *p_value = g_follow_snapshot;
    (void)ISpinUnlock(&g_follow_snapshot_lock, saved);
    return 0;
}
#endif
