#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include "motion_control.h"
#include "motion_button.h"

static void test_start_refresh_and_expiry(void)
{
    motion_control_t control;
    motion_command_t command = {1000U, 1U, 50, -35};
    motion_output_t output;

    motion_control_init(&control);
    motion_control_step(&control, 1000U, false, true, false, &output);
    assert(control.phase == MOTION_READY && output.lease_ms == 0U);
    assert(motion_control_submit(&control, &command, 1010U) == MOTION_ACCEPTED);
    motion_control_step(&control, 1020U, false, true, false, &output);
    assert(control.phase == MOTION_RUNNING);
    assert(output.left_pct == 50 && output.right_pct == -35);
    assert(output.lease_ms == 80U);
    assert(motion_control_submit(&control, &command, 1080U) == MOTION_DUPLICATE);
    motion_control_step(&control, 1099U, false, true, true, &output);
    assert(output.lease_ms == 1U);
    motion_control_step(&control, 1100U, false, true, true, &output);
    assert(control.phase == MOTION_STOPPED);
    assert(control.reason == MOTION_REASON_EXPIRED);
    assert(output.left_pct == 0 && output.right_pct == 0);
    command.issued_ms = 1100U;
    command.sequence = 2U;
    assert(motion_control_submit(&control, &command, 1100U) == MOTION_LATCHED);
}

static void test_invalid_and_ordering(void)
{
    motion_control_t control;
    motion_command_t command = {100U, UINT16_MAX, -50, 50};

    motion_control_init(&control);
    assert(motion_control_submit(&control, NULL, 100U) == MOTION_BAD_COMMAND);
    assert(motion_control_submit(NULL, &command, 100U) == MOTION_BAD_COMMAND);
    command.left_pct = INT8_MIN;
    assert(motion_control_submit(&control, &command, 100U) == MOTION_BAD_COMMAND);
    command.left_pct = -50;
    command.right_pct = 51;
    assert(motion_control_submit(&control, &command, 100U) == MOTION_BAD_COMMAND);
    command.right_pct = 50;
    assert(motion_control_submit(&control, &command, 99U) == MOTION_STALE);
    assert(motion_control_submit(&control, &command, 200U) == MOTION_STALE);
    assert(motion_control_submit(&control, &command, 100U) == MOTION_ACCEPTED);
    command.left_pct = -49;
    assert(motion_control_submit(&control, &command, 110U) == MOTION_CONFLICT);
    command.sequence = 0U;
    command.issued_ms = 110U;
    assert(motion_control_submit(&control, &command, 110U) == MOTION_ACCEPTED);
    command.sequence = UINT16_MAX;
    assert(motion_control_submit(&control, &command, 110U) == MOTION_STALE);
    command.sequence = 32768U;
    assert(motion_control_submit(&control, &command, 110U) == MOTION_STALE);
}

static void test_fresh_refresh_stop_and_hardware_loss(void)
{
    motion_control_t control;
    motion_command_t command = {0U, 1U, 40, 0};
    motion_output_t output;

    motion_control_init(&control);
    assert(motion_control_submit(&control, &command, 0U) == MOTION_ACCEPTED);
    motion_control_step(&control, 0U, false, true, false, &output);
    command.issued_ms = 80U;
    command.sequence = 2U;
    command.right_pct = -20;
    assert(motion_control_submit(&control, &command, 80U) == MOTION_ACCEPTED);
    motion_control_step(&control, 90U, false, true, true, &output);
    assert(output.lease_ms == 90U && output.right_pct == -20);
    motion_control_step(&control, 100U, true, false, true, &output);
    assert(control.phase == MOTION_STOPPED);
    assert(control.reason == MOTION_REASON_BUTTON && output.lease_ms == 0U);
    motion_control_stop(&control, MOTION_REASON_REQUEST);
    assert(control.reason == MOTION_REASON_BUTTON);

    motion_control_init(&control);
    assert(motion_control_submit(&control, &command, 90U) == MOTION_ACCEPTED);
    motion_control_step(&control, 90U, false, true, false, &output);
    motion_control_step(&control, 100U, false, true, false, &output);
    assert(control.phase == MOTION_STOPPED);
    assert(control.reason == MOTION_REASON_EXPIRED && output.lease_ms == 0U);

    motion_control_init(&control);
    motion_control_step(&control, 100U, false, false, false, &output);
    assert(control.phase == MOTION_FAULT);
    assert(control.reason == MOTION_REASON_HARDWARE);
    assert(motion_control_submit(&control, &command, 100U) == MOTION_LATCHED);
}

static void test_neutral_and_timestamp_wrap(void)
{
    motion_control_t control;
    motion_command_t command = {UINT32_MAX - 49U, 1U, 30, -30};
    motion_output_t output;

    motion_control_init(&control);
    assert(motion_control_submit(&control, &command, 0U) == MOTION_ACCEPTED);
    motion_control_step(&control, 0U, false, true, false, &output);
    assert(output.lease_ms == 50U);
    motion_control_step(&control, 49U, false, true, true, &output);
    assert(output.lease_ms == 1U);
    command.issued_ms = 49U;
    command.sequence = 2U;
    command.left_pct = 0;
    command.right_pct = 0;
    assert(motion_control_submit(&control, &command, 49U) == MOTION_ACCEPTED);
    motion_control_step(&control, 49U, false, true, true, &output);
    assert(control.phase == MOTION_READY && output.lease_ms == 0U);
    motion_control_step(&control, 200U, false, true, false, &output);
    assert(control.phase == MOTION_READY);
    motion_control_stop(&control, MOTION_REASON_REQUEST);
    assert(control.phase == MOTION_STOPPED);
    assert(motion_control_submit(&control, &command, 49U) == MOTION_LATCHED);
}

static void test_late_fresh_request_cannot_hide_expiry(void)
{
    motion_control_t control;
    motion_command_t command = {0U, 1U, 50, 0};
    motion_output_t output;

    motion_control_init(&control);
    assert(motion_control_submit(&control, &command, 0U) == MOTION_ACCEPTED);
    motion_control_step(&control, 0U, false, true, false, &output);
    command.issued_ms = 100U;
    command.sequence = 2U;
    assert(motion_control_submit(&control, &command, 100U) == MOTION_LATCHED);
    motion_control_step(&control, 100U, false, true, true, &output);
    assert(control.reason == MOTION_REASON_EXPIRED && output.lease_ms == 0U);
}

static void test_local_producer_is_finite_and_requires_release(void)
{
    motion_button_t button;
    motion_command_t command;
    uint32_t base = UINT32_MAX - 1000U;

    motion_button_init(&button, base);
    for (uint32_t elapsed = 0U; elapsed < 100U; elapsed += 20U)
    {
        assert(!motion_button_update(&button, base + elapsed, true, true,
                                      &command));
    }
    assert(!motion_button_update(&button, base + 100U, false, true, &command));
    assert(!motion_button_update(&button, base + 140U, false, true, &command));
    assert(!motion_button_update(&button, base + 160U, true, true, &command));
    assert(motion_button_update(&button, base + 200U, true, true, &command));
    assert(command.left_pct == 50 && command.right_pct == 0);
    assert(command.sequence == 1U && command.issued_ms == base + 200U);
    assert(motion_button_update(&button, base + 2199U, false, true, &command));
    assert(command.sequence == 2U && command.issued_ms == base + 2199U);
    assert(!motion_button_update(&button, base + 2200U, false, true, &command));
    assert(!motion_button_update(&button, base + 2240U, true, true, &command));
    assert(!motion_button_update(&button, base + 2280U, true, true, &command));
    assert(button.was_started && !button.is_running);

    motion_button_init(&button, 0U);
    assert(!motion_button_update(&button, 0U, false, false, &command));
    assert(!motion_button_update(&button, 40U, false, false, &command));
    assert(!motion_button_update(&button, 60U, true, false, &command));
    assert(!motion_button_update(&button, 100U, true, false, &command));
    assert(!button.was_started);
}

int main(void)
{
    test_start_refresh_and_expiry();
    test_invalid_and_ordering();
    test_fresh_refresh_stop_and_hardware_loss();
    test_neutral_and_timestamp_wrap();
    test_late_fresh_request_cannot_hide_expiry();
    test_local_producer_is_finite_and_requires_release();
    printf("PASS: Motion start/refresh, neutral, expiry/latch, validation, "
           "ordering and wrap; command=%zu control=%zu output=%zu bytes\n",
           sizeof(motion_command_t), sizeof(motion_control_t),
           sizeof(motion_output_t));
    return 0;
}
