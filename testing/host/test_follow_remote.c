/* Real command mailbox with a single-threaded RTOS lock substitute. */
#define AAI05_APP_H
#define __TK_SMP_H__
#define AAI05_FOLLOW_MQTT 1
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

typedef int ER;
typedef unsigned int UINT;
typedef bool T_SPLOCK;
#define E_OK 0

static uint32_t test_clock_ms;

static ER InitSpinLock(T_SPLOCK *p_lock)
{
    *p_lock = false;
    return E_OK;
}

static ER ISpinLock(T_SPLOCK *p_lock, UINT *p_saved)
{
    assert(!*p_lock);
    *p_lock = true;
    *p_saved = 1U;
    return E_OK;
}

static ER ISpinUnlock(T_SPLOCK *p_lock, UINT saved)
{
    assert(*p_lock && saved == 1U);
    *p_lock = false;
    return E_OK;
}

static uint32_t app_now_ms(void)
{
    return test_clock_ms;
}

#include "../../app_program/subsystem2/follow_remote.c"

int main(void)
{
    uint16_t epoch = 0U;
    assert(subsystem2_follow_arm(&epoch, false) == MOTION_UNAVAILABLE);
    assert(motion_runtime_init() == E_OK);
    assert(subsystem2_follow_arm(&epoch, false) == MOTION_ACCEPTED && epoch == 1U);
    assert(!follow_remote_take().start);
    assert(subsystem2_motion_stop() == MOTION_ACCEPTED);
    motion_command_t feed = {0U, 1U, 0, 0};
    assert(subsystem2_motion_submit(&feed) == MOTION_LATCHED);
    assert(!follow_remote_take().start);

    test_clock_ms = UINT32_MAX - 49U;
    assert(subsystem2_follow_arm(&epoch, true) == MOTION_ACCEPTED && epoch == 2U);
    assert(subsystem2_follow_arm(&epoch, false) == MOTION_LATCHED);
    feed.issued_ms = test_clock_ms;
    feed.left_pct = 25;
    assert(subsystem2_motion_submit(&feed) == MOTION_BAD_COMMAND);
    feed.left_pct = 0;
    assert(subsystem2_motion_submit(&feed) == MOTION_ACCEPTED);
    follow_remote_permission_t initial = follow_remote_take();
    assert(initial.start && initial.epoch == epoch && initial.straight);
    assert(follow_remote_remaining(initial, test_clock_ms) == 100U);
    follow_remote_permission_t renewed = follow_remote_take();
    assert(!renewed.start && renewed.straight);
    test_clock_ms = 20U;
    assert(follow_remote_remaining(follow_remote_take(), test_clock_ms) == 30U);
    feed.issued_ms = test_clock_ms;
    assert(subsystem2_motion_submit(&feed) == MOTION_ACCEPTED);
    assert(!follow_remote_take().start);
    test_clock_ms = 120U;
    feed.issued_ms = test_clock_ms;
    assert(subsystem2_motion_submit(&feed) == MOTION_LATCHED);
    assert(follow_remote_remaining(follow_remote_take(), test_clock_ms) == 0U);
    assert(subsystem2_motion_stop() == MOTION_ACCEPTED);
    assert(subsystem2_follow_arm(&epoch, false) == MOTION_ACCEPTED && epoch == 3U);
    assert(!follow_remote_take().start);
    puts("PASS: only fresh FOLLOW authorizes PWM, STOP wins, and expired feed cannot rearm");
    return 0;
}
