#include <assert.h>
#include <stdio.h>
#include "drive_control.h"

static drive_command_t test_start(const drive_control_t *p_control,
                                    uint16_t sequence)
{
    return (drive_command_t){.session = p_control->session,
        .grant = p_control->grant, .sequence = sequence,
        .opcode = DRIVE_OP_START, .target = 7U, .left_pct = 50};
}

static void test_start_renew_duplicate_and_expiry(void)
{
    drive_control_t control;
    drive_action_t action;
    drive_motion_view_t view = {.is_available = true};
    drive_control_reset(&control, 123U, 100U, 0U, true);
    drive_command_t start = test_start(&control, 65535U);
    assert(drive_control_receive(&control, &start, false, 0U, &action) == DRIVE_OK);
    assert(action.kind == DRIVE_ACTION_ARM && control.phase == DRIVE_ARMING);
    drive_command_t conflict = start;
    conflict.left_pct = 49;
    assert(drive_control_receive(&control, &conflict, false, 1U, &action) ==
           DRIVE_CONFLICT);
    conflict.sequence = 65534U;
    assert(drive_control_receive(&control, &conflict, false, 1U, &action) ==
           DRIVE_STALE_SEQUENCE);
    assert(drive_control_arm_result(&control, MOTION_ACCEPTED, 1U, 0U) == DRIVE_OK);
    drive_control_poll(&control, 10U, true, &view, &action);
    assert(action.kind == DRIVE_ACTION_NONE);
    view.status.epoch = 1U;
    view.status.phase = MOTION_READY;
    drive_control_poll(&control, 20U, true, &view, &action);
    assert(control.phase == DRIVE_RUNNING && action.kind == DRIVE_ACTION_SUBMIT);
    assert(action.command.left_pct == 50 && action.command.right_pct == 0);
    assert(action.command.issued_ms == 20U && action.command.sequence == 1U);
    assert(drive_control_receive(&control, &start, false, 500U, &action) ==
           DRIVE_DUPLICATE);
    assert(drive_control_lease(&control, 500U) == 250U);
    drive_command_t keep = {.session = 123U, .grant = control.grant,
        .sequence = 0U, .opcode = DRIVE_OP_KEEPALIVE, .target = 7U};
    assert(drive_control_receive(&control, &keep, false, 600U, &action) == DRIVE_OK);
    assert(drive_control_lease(&control, 600U) == 750U);
    assert(drive_control_receive(&control, &keep, false, 1300U, &action) ==
           DRIVE_DUPLICATE);
    assert(drive_control_lease(&control, 1300U) == 50U);
    view.status.phase = MOTION_RUNNING;
    view.is_active = true;
    drive_control_poll(&control, 1350U, true, &view, &action);
    assert(action.kind == DRIVE_ACTION_STOP && control.phase == DRIVE_STOPPED);
    keep.sequence = 1U;
    keep.grant = control.grant;
    assert(drive_control_receive(&control, &keep, false, 1350U, &action) ==
           DRIVE_BAD_STATE);
}

static void test_retained_stale_grant_and_stop_priority(void)
{
    drive_control_t control;
    drive_action_t action;
    drive_control_reset(&control, 123U, 100U, 0U, true);
    drive_command_t start = test_start(&control, 10U);
    assert(drive_control_receive(&control, &start, true, 0U, &action) ==
           DRIVE_RETAINED);
    assert(action.kind == DRIVE_ACTION_NONE && control.grant == 100U);
    assert(drive_control_receive(&control, &start, false, 1000U, &action) ==
           DRIVE_STALE_GRANT);
    start = test_start(&control, 10U);
    assert(drive_control_receive(&control, &start, false, 1000U, &action) == DRIVE_OK);
    assert(drive_control_arm_result(&control, MOTION_LATCHED, 0U, 1000U) ==
           DRIVE_MOTION_FAILURE);
    assert(control.phase == DRIVE_STOPPED);
    drive_command_t stop = {.session = 123U, .sequence = 1U,
        .opcode = DRIVE_OP_STOP, .target = 7U};
    assert(drive_control_receive(&control, &stop, false, 1001U, &action) ==
           DRIVE_DUPLICATE);
    assert(action.kind == DRIVE_ACTION_STOP && control.previous.sequence == 10U);
    start = test_start(&control, 11U);
    assert(drive_control_receive(&control, &start, false, 1010U, &action) == DRIVE_OK);
    assert(drive_control_receive(&control, &stop, false, 1011U, &action) == DRIVE_OK);
    assert(control.phase == DRIVE_STOPPED && action.kind == DRIVE_ACTION_STOP);
    start.session = 999U;
    assert(drive_control_receive(&control, &start, false, 1012U, &action) ==
           DRIVE_SESSION);
}

static void test_configuration_motion_loss_and_wrap(void)
{
    drive_control_t control;
    drive_action_t action;
    drive_motion_view_t view = {.is_available = true};
    uint32_t base = UINT32_MAX - 200U;
    drive_control_reset(&control, 123U, UINT32_MAX, base, true);
    drive_command_t target = {.session = 123U, .grant = UINT32_MAX,
        .sequence = 5U, .opcode = DRIVE_OP_TARGET, .target = 6U};
    assert(drive_control_receive(&control, &target, false, base, &action) == DRIVE_OK);
    assert(control.target == 6U && control.grant != 0U);
    drive_command_t start = test_start(&control, 6U);
    assert(drive_control_receive(&control, &start, false, base, &action) == DRIVE_OK);
    target.grant = control.grant;
    target.sequence = 7U;
    assert(drive_control_receive(&control, &target, false, base, &action) ==
           DRIVE_BAD_STATE);
    assert(drive_control_arm_result(&control, MOTION_ACCEPTED, 1U, base) == DRIVE_OK);
    view.status.epoch = 1U;
    drive_control_poll(&control, base + 20U, true, &view, &action);
    assert(action.kind == DRIVE_ACTION_SUBMIT);
    view.status.phase = MOTION_STOPPED;
    view.status.reason = MOTION_REASON_BUTTON;
    drive_control_poll(&control, base + 40U, true, &view, &action);
    assert(control.phase == DRIVE_STOPPED && action.kind == DRIVE_ACTION_STOP);
    drive_control_poll(&control, base + 60U, true, &view, &action);
    assert(action.kind == DRIVE_ACTION_NONE); /* No recovery/rearm loop. */
    start = test_start(&control, 7U);
    assert(drive_control_receive(&control, &start, false, base + 70U, &action) == DRIVE_OK);
    assert(drive_control_arm_result(&control, MOTION_ACCEPTED, 2U, base + 70U) == DRIVE_OK);
    drive_control_poll(&control, base + 80U, false, &view, &action);
    assert(control.phase == DRIVE_STOPPED && action.kind == DRIVE_ACTION_STOP);
    drive_control_reset(&control, 0U, 0U, 0U, false);
    assert(drive_control_receive(&control, &start, false, 0U, &action) ==
           DRIVE_NOT_READY);

    drive_control_reset(&control, 123U, 100U, base, true);
    start = test_start(&control, 1U);
    assert(drive_control_receive(&control, &start, false, base, &action) == DRIVE_OK);
    assert(drive_control_arm_result(&control, MOTION_ACCEPTED, 1U, base) == DRIVE_OK);
    view.status.epoch = 1U;
    view.status.phase = MOTION_READY;
    drive_control_poll(&control, base + 20U, true, &view, &action);
    view.status.phase = MOTION_RUNNING;
    view.is_active = true;
    assert(drive_control_lease(&control, base + 749U) == 1U);
    drive_control_poll(&control, base + 750U, true, &view, &action);
    assert(control.phase == DRIVE_STOPPED && action.kind == DRIVE_ACTION_STOP);
    drive_control_reset(&control, 123U, 100U, base, true);
    start = test_start(&control, 1U);
    assert(drive_control_receive(&control, &start, false, base + 1000U,
                                  &action) == DRIVE_STALE_GRANT);
}

int main(void)
{
    test_start_renew_duplicate_and_expiry();
    test_retained_stale_grant_and_stop_priority();
    test_configuration_motion_loss_and_wrap();
    puts("PASS: remote lifecycle, grant/sequence replay, STOP, expiry and wrap");
    return 0;
}
