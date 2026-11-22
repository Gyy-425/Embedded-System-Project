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

#if AAI05_DISTANCE_RUN
uint32_t motor_drive_now_us(void)
{
    return test_now * 1000U;
}

int32_t motor_drive_set_until(int8_t left_pct, int8_t right_pct,
                               uint32_t deadline_us)
{
    uint32_t remaining_us = deadline_us - motor_drive_now_us();
    if (remaining_us == 0U || remaining_us > MOTOR_DRIVE_MAX_LEASE_MS * 1000U)
    {
        return E_TMOUT;
    }
    return motor_drive_set(left_pct, right_pct, remaining_us / 1000U);
}
#endif

void motor_pulse_stop(void)
{
    assert(lock_depth == 1U);
    applied_left = 0;
    applied_right = 0;
    applied_lease = 0U;
}

int32_t motor_drive_rearm(void)
{
    assert(lock_depth == 1U);
    assert(applied_left == 0 && applied_right == 0 && applied_lease == 0U);
    driver_error = 0;
    return E_OK;
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

/* Breaks caught: logical adapter bypasses the owner, raw sequence domain,
 * readiness/arm guards, STOP/fault latch, zero semantics or wrapped expiry. */
static void test_chassis_submission_uses_existing_guards(void)
{
    motion_chassis_command_t request = {1000U, UINT16_MAX, 40U, 10};
    motion_status_t status;

    assert(subsystem2_motion_submit_chassis(NULL) == MOTION_BAD_COMMAND);
    test_now = 1000U;
    assert(motion_runtime_init() == E_OK);
    assert(subsystem2_motion_submit_chassis(&request) == MOTION_UNAVAILABLE);
    test_owner_step(1000U, BUTTONS_RELEASED, true, false, &status);
#if AAI05_REMOTE_APP
    uint16_t epoch;
    assert(subsystem2_motion_arm(&epoch) == MOTION_ACCEPTED);
    assert(subsystem2_motion_submit_chassis(&request) == MOTION_UNAVAILABLE);
    test_owner_step(1000U, BUTTONS_RELEASED, true, false, &status);
#endif
    request.issued_ms = 900U;
    assert(subsystem2_motion_submit_chassis(&request) == MOTION_STALE);
    request.issued_ms = 1001U;
    assert(subsystem2_motion_submit_chassis(&request) == MOTION_STALE);
    request.issued_ms = 1000U;
    assert(subsystem2_motion_submit_chassis(&request) == MOTION_ACCEPTED);
    assert(applied_lease == 0U);
    assert(subsystem2_motion_submit_chassis(&request) == MOTION_DUPLICATE);
    motion_command_t raw = {1000U, UINT16_MAX, -50, 30};
    assert(subsystem2_motion_submit(&raw) == MOTION_DUPLICATE);
    raw.right_pct = 31;
    assert(subsystem2_motion_submit(&raw) == MOTION_CONFLICT);
    test_owner_step(1020U, BUTTONS_RELEASED, true, false, &status);
    assert(status.phase == MOTION_RUNNING);
    assert(applied_left == -50 && applied_right == 30 && applied_lease == 80U);
    request.forward_pct = 51U;
    assert(subsystem2_motion_submit_chassis(&request) == MOTION_BAD_COMMAND);
    assert(applied_left == -50 && applied_right == 30 && applied_lease == 80U);
    request.forward_pct = 40U;
    raw = (motion_command_t){1020U, 0U, -25, 25};
    assert(subsystem2_motion_submit(&raw) == MOTION_ACCEPTED);
    assert(subsystem2_motion_submit_chassis(&request) == MOTION_STALE);
    request = (motion_chassis_command_t){1020U, 1U, 0U, 50};
    assert(subsystem2_motion_submit_chassis(&request) == MOTION_ACCEPTED);
    test_owner_step(1020U, BUTTONS_RELEASED, true, true, &status);
    assert(status.phase == MOTION_READY && applied_lease == 0U);
    request.sequence = 2U;
    request.forward_pct = 30U;
    request.turn_pct = 0;
    assert(subsystem2_motion_submit_chassis(&request) == MOTION_ACCEPTED);
    test_owner_step(1040U, BUTTONS_RELEASED, true, false, &status);
    assert(applied_left == -30 && applied_right == 30);
    test_owner_step(1050U, 1U << 20U, true, true, &status);
    assert(status.phase == MOTION_STOPPED && applied_lease == 0U);
    assert(status.reason == MOTION_REASON_BUTTON);
    request.issued_ms = 1050U;
    request.sequence = 3U;
    assert(subsystem2_motion_submit_chassis(&request) == MOTION_LATCHED);

    assert(motion_runtime_init() == E_OK);
    test_owner_step(1050U, BUTTONS_RELEASED, false, false, &status);
    assert(status.phase == MOTION_FAULT);
    assert(subsystem2_motion_submit_chassis(&request) == MOTION_UNAVAILABLE);
    test_owner_step(1050U, BUTTONS_RELEASED, true, false, &status);
    assert(subsystem2_motion_submit_chassis(&request) == MOTION_LATCHED);
    assert(applied_lease == 0U);
}

static void test_chassis_wrapped_expiry(void)
{
    motion_status_t status;
    motion_chassis_command_t request = {UINT32_MAX - 20U, UINT16_MAX, 40U, -10};
    test_now = request.issued_ms;
    assert(motion_runtime_init() == E_OK);
    test_owner_step(test_now, BUTTONS_RELEASED, true, false, &status);
    assert(subsystem2_motion_submit_chassis(&request) == MOTION_ACCEPTED);
    test_owner_step(9U, BUTTONS_RELEASED, true, false, &status);
    assert(applied_left == -30 && applied_right == 50 && applied_lease == 70U);
    test_owner_step(79U, BUTTONS_RELEASED, true, true, &status);
    assert(status.phase == MOTION_STOPPED && applied_lease == 0U);
    assert(status.reason == MOTION_REASON_EXPIRED);
    request.issued_ms = 79U;
    request.sequence = 0U;
    assert(subsystem2_motion_submit_chassis(&request) == MOTION_LATCHED);
}

static void test_owner_applies_and_latches(void)
{
    motion_command_t command = {0U, 1U, 30, -30};
    motion_chassis_command_t chassis = {0U, 1U, 30U, 0};
    motion_status_t status;

    assert(subsystem2_motion_submit(&command) == MOTION_UNAVAILABLE);
    assert(subsystem2_motion_submit_chassis(&chassis) == MOTION_UNAVAILABLE);
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
#if !AAI05_REMOTE_APP
    test_owner_step(0U, BUTTONS_RELEASED, true, false, &status);
    test_owner_step(40U, BUTTONS_RELEASED, true, false, &status);
    test_owner_step(60U, 1U << 21U, true, false, &status);
    test_owner_step(100U, 1U << 21U, true, false, &status);
#if AAI05_DISTANCE_RUN
    assert(status.phase == MOTION_RUNNING && applied_left == -25);
    assert(applied_right == 25);
#else
    assert(status.phase == MOTION_RUNNING && applied_left == 50);
    assert(applied_right == 0);
#endif
    test_owner_step(120U, 0U, true, true, &status);
    assert(status.phase == MOTION_STOPPED);
    assert(status.reason == MOTION_REASON_BUTTON && applied_lease == 0U);
#endif

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

#if AAI05_DISTANCE_RUN
static void test_distance_run_has_fixed_deadline(void)
{
    motion_status_t status;
    /* The 32-bit hardware clock wraps during this run. */
    const uint32_t start_ms = UINT32_MAX / 1000U - 500U;
    test_now = start_ms - 100U;
    assert(motion_runtime_init() == E_OK);
    test_owner_step(start_ms - 100U, BUTTONS_RELEASED, true, false, &status);
    test_owner_step(start_ms - 60U, BUTTONS_RELEASED, true, false, &status);
    test_owner_step(start_ms - 40U, 1U << 21U, true, false, &status);
    assert(applied_lease == 0U);
    test_owner_step(start_ms, 1U << 21U, true, false, &status);
    assert(applied_left == -25 && applied_right == 25 && applied_lease == 100U);

    for (uint32_t elapsed_ms = 10U; elapsed_ms < 2000U; elapsed_ms += 10U)
    {
        uint32_t remaining_ms = 2000U - elapsed_ms;
        test_owner_step(start_ms + elapsed_ms, BUTTONS_RELEASED, true, true, &status);
        assert(status.phase == MOTION_RUNNING);
        assert(applied_lease == (remaining_ms < 100U ? remaining_ms : 100U));
    }
    test_owner_step(start_ms + 2000U, BUTTONS_RELEASED, true, true, &status);
    assert(status.phase == MOTION_STOPPED);
    assert(status.reason == MOTION_REASON_EXPIRED && applied_lease == 0U);
    test_owner_step(start_ms + 2020U, 1U << 21U, true, false, &status);
    test_owner_step(start_ms + 2060U, 1U << 21U, true, false, &status);
    assert(status.phase == MOTION_STOPPED && applied_lease == 0U);
}
#endif

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

#if AAI05_REMOTE_APP
static void test_remote_arm_and_stop_precedence(void)
{
    motion_status_t status;
    motion_command_t command = {20U, 1U, 50, 0};
    uint16_t epoch = 0U;

    test_now = 0U;
    assert(motion_runtime_init() == E_OK);
    test_owner_step(0U, BUTTONS_RELEASED, true, false, &status);
    assert(subsystem2_motion_arm(&epoch) == MOTION_ACCEPTED && epoch == 1U);
    test_now = 20U;
    assert(subsystem2_motion_submit(&command) == MOTION_UNAVAILABLE);
    test_owner_step(20U, BUTTONS_RELEASED, true, false, &status);
    assert(status.epoch == epoch && status.phase == MOTION_READY);
    assert(applied_lease == 0U);
    assert(subsystem2_motion_submit(&command) == MOTION_ACCEPTED);
    test_owner_step(40U, BUTTONS_RELEASED, true, false, &status);
    assert(status.phase == MOTION_RUNNING);
    assert(subsystem2_motion_arm(&epoch) == MOTION_LATCHED);

    assert(subsystem2_motion_stop() == MOTION_ACCEPTED);
    assert(subsystem2_motion_arm(&epoch) == MOTION_ACCEPTED);
    assert(subsystem2_motion_stop() == MOTION_ACCEPTED);
    test_owner_step(60U, BUTTONS_RELEASED, true, true, &status);
    assert(status.epoch == epoch && status.phase == MOTION_STOPPED);
    assert(applied_lease == 0U);
    assert(subsystem2_motion_arm(&epoch) == MOTION_ACCEPTED);
    test_owner_step(80U, 1U << 20U, true, false, &status);
    assert(status.epoch == epoch && status.phase == MOTION_STOPPED);
    assert(applied_lease == 0U); /* A held GP21 cannot be rearmed remotely. */
    assert(subsystem2_motion_arm(&epoch) == MOTION_ACCEPTED);
    test_owner_step(100U, BUTTONS_RELEASED, true, false, &status);
    assert(status.epoch == epoch && status.phase == MOTION_READY);
}
#endif

int main(void)
{
    test_owner_applies_and_latches();
    test_chassis_submission_uses_existing_guards();
    test_chassis_wrapped_expiry();
    test_button_and_driver_failure();
#if AAI05_DISTANCE_RUN
    test_distance_run_has_fixed_deadline();
#endif
    test_concurrent_stop_and_commands();
    test_newer_submission_precedes_old_owner_sample();
#if AAI05_REMOTE_APP
    test_remote_arm_and_stop_precedence();
#endif
    puts("PASS: real Motion adapter ownership, readiness, STOP/expiry, "
         "local producer and concurrent command/STOP serialization");
    return 0;
}
