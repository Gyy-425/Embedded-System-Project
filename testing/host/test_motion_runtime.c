/* Real runtime adapter/policy with pthread and motor boundary substitutes. */
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#define AAI05_APP_H
#define __TK_SMP_H__
#define AAI05_MOTION_APP 1
#define TK_SUPPORT_SMP 1
#define E_OK 0
#define E_TMOUT (-50)
typedef unsigned int UINT;
typedef int ER;
typedef pthread_mutex_t T_SPLOCK;

#include "motion_runtime.h"
#include "motor_pulse.h"

static _Thread_local uint32_t test_now;
static _Thread_local unsigned int lock_depth;
static bool is_lock_initialized;
static int8_t applied_left;
static int8_t applied_right;
static uint32_t applied_lease;
static int32_t driver_error;

static ER InitSpinLock(T_SPLOCK *p_lock)
{
    if (!is_lock_initialized)
    {
        assert(pthread_mutex_init(p_lock, NULL) == 0);
        is_lock_initialized = true;
    }
    return E_OK;
}

static ER ISpinLock(T_SPLOCK *p_lock, UINT *p_saved)
{
    assert(pthread_mutex_lock(p_lock) == 0);
    assert(lock_depth == 0U);
    lock_depth = 1U;
    *p_saved = 17U;
    return E_OK;
}

static ER ISpinUnlock(T_SPLOCK *p_lock, UINT saved)
{
    assert(saved == 17U && lock_depth == 1U);
    lock_depth = 0U;
    assert(pthread_mutex_unlock(p_lock) == 0);
    return E_OK;
}

uint32_t app_now_ms(void)
{
    return test_now;
}

int32_t motor_drive_set(int8_t left_pct, int8_t right_pct, uint32_t lease_ms)
{
    assert(lock_depth == 1U);
    assert(lease_ms > 0U && lease_ms <= 100U);
    if (driver_error != 0)
    {
        return driver_error;
    }
    applied_left = left_pct;
    applied_right = right_pct;
    applied_lease = lease_ms;
    return E_OK;
}

void motor_pulse_stop(void)
{
    assert(lock_depth == 1U);
    applied_left = 0;
    applied_right = 0;
    applied_lease = 0U;
}

#include "../../app_program/subsystem2/motion_runtime.c"

#define BUTTONS_RELEASED ((1U << 20U) | (1U << 21U))

static void test_owner_step(uint32_t now_ms, uint32_t buttons,
                             bool is_healthy, bool is_active,
                             motion_status_t *p_status)
{
    test_now = now_ms;
    motion_runtime_step(now_ms, buttons, is_healthy, is_active, p_status);
}

static void test_owner_applies_and_latches(void)
{
    motion_command_t command = {0U, 1U, 30, -30};
    motion_status_t status;

    assert(subsystem2_motion_submit(&command) == MOTION_UNAVAILABLE);
    assert(motion_runtime_init() == E_OK);
    assert(subsystem2_motion_submit(&command) == MOTION_UNAVAILABLE);
    test_owner_step(0U, BUTTONS_RELEASED, true, false, &status);
    assert(status.phase == MOTION_READY && applied_lease == 0U);
    assert(subsystem2_motion_submit(&command) == MOTION_ACCEPTED);
    assert(applied_lease == 0U); /* Producer never directly drives hardware. */
    test_owner_step(20U, BUTTONS_RELEASED, true, false, &status);
    assert(status.phase == MOTION_RUNNING);
    assert(applied_left == 30 && applied_right == -30 && applied_lease == 80U);
    test_owner_step(100U, BUTTONS_RELEASED, true, true, &status);
    assert(status.phase == MOTION_STOPPED);
    assert(status.reason == MOTION_REASON_EXPIRED && applied_lease == 0U);
    command.issued_ms = 100U;
    command.sequence = 2U;
    test_now = 100U;
    assert(subsystem2_motion_submit(&command) == MOTION_LATCHED);

    assert(motion_runtime_init() == E_OK);
    test_owner_step(100U, BUTTONS_RELEASED, true, false, &status);
    assert(subsystem2_motion_submit(&command) == MOTION_ACCEPTED);
    assert(subsystem2_motion_stop() == MOTION_ACCEPTED);
    test_owner_step(120U, BUTTONS_RELEASED, true, false, &status);
    assert(status.phase == MOTION_STOPPED);
    assert(status.reason == MOTION_REASON_REQUEST && applied_lease == 0U);
}

static void test_button_and_driver_failure(void)
{
    motion_status_t status;

    test_now = 0U;
    assert(motion_runtime_init() == E_OK);
    test_owner_step(0U, BUTTONS_RELEASED, true, false, &status);
    test_owner_step(40U, BUTTONS_RELEASED, true, false, &status);
    test_owner_step(60U, 1U << 21U, true, false, &status);
    test_owner_step(100U, 1U << 21U, true, false, &status);
    assert(status.phase == MOTION_RUNNING && applied_left == 50);
    assert(applied_right == 0);
    test_owner_step(120U, 0U, true, true, &status);
    assert(status.phase == MOTION_STOPPED);
    assert(status.reason == MOTION_REASON_BUTTON && applied_lease == 0U);

    assert(motion_runtime_init() == E_OK);
    test_owner_step(0U, BUTTONS_RELEASED, true, false, &status);
    motion_command_t command = {0U, 1U, 50, 50};
    assert(subsystem2_motion_submit(&command) == MOTION_ACCEPTED);
    driver_error = E_TMOUT;
    test_owner_step(20U, BUTTONS_RELEASED, true, false, &status);
    assert(status.phase == MOTION_STOPPED);
    assert(status.reason == MOTION_REASON_EXPIRED && applied_lease == 0U);
    driver_error = 0;
}

static void *test_concurrent_producer(void *p_unused)
{
    (void)p_unused;
    for (uint16_t sequence = 1U; sequence <= 2000U; ++sequence)
    {
        motion_command_t command = {0U, sequence, 35, -35};
        motion_result_t result = subsystem2_motion_submit(&command);
        assert(result == MOTION_ACCEPTED || result == MOTION_LATCHED);
    }
    return NULL;
}

static void *test_concurrent_owner(void *p_unused)
{
    motion_status_t status;
    (void)p_unused;
    for (uint32_t index = 0U; index < 2000U; ++index)
    {
        test_owner_step(0U, BUTTONS_RELEASED, true, true, &status);
        assert(status.left_pct == 0 ||
               (status.left_pct == 35 && status.right_pct == -35));
    }
    return NULL;
}

static void test_concurrent_stop_and_commands(void)
{
    pthread_t producer;
    pthread_t owner;
    motion_status_t status;

    assert(motion_runtime_init() == E_OK);
    test_owner_step(0U, BUTTONS_RELEASED, true, false, &status);
    assert(pthread_create(&producer, NULL, test_concurrent_producer, NULL) == 0);
    assert(pthread_create(&owner, NULL, test_concurrent_owner, NULL) == 0);
    assert(subsystem2_motion_stop() == MOTION_ACCEPTED);
    assert(pthread_join(producer, NULL) == 0);
    assert(pthread_join(owner, NULL) == 0);
    test_owner_step(0U, BUTTONS_RELEASED, true, true, &status);
    assert(status.phase == MOTION_STOPPED && applied_lease == 0U);
    motion_command_t command = {0U, 2001U, 50, 50};
    assert(subsystem2_motion_submit(&command) == MOTION_LATCHED);
}

static void test_newer_submission_precedes_old_owner_sample(void)
{
    motion_status_t status;
    motion_command_t command = {101U, 1U, 40, -40};

    test_now = 100U;
    assert(motion_runtime_init() == E_OK);
    motion_runtime_step(100U, BUTTONS_RELEASED, true, false, &status);
    /* Other core wins the lock at a newer clock tick. */
    test_now = 101U;
    assert(subsystem2_motion_submit(&command) == MOTION_ACCEPTED);
    motion_runtime_step(100U, BUTTONS_RELEASED, true, false, &status);
    assert(status.phase == MOTION_RUNNING && status.lease_ms == 100U);
}

int main(void)
{
    test_owner_applies_and_latches();
    test_button_and_driver_failure();
    test_concurrent_stop_and_commands();
    test_newer_submission_precedes_old_owner_sample();
    puts("PASS: real Motion adapter ownership, readiness, STOP/expiry, "
         "local producer and concurrent command/STOP serialization");
    return 0;
}
