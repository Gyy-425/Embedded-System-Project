/* MQTT lane selection on the real follow owner (v0.87 home steering).
 * The remote permission is faked; steering, joins and the classifier are real. */
#include "black_follow_owner_harness.h"

#if !AAI05_FOLLOW_MQTT || !FOLLOW_MULTICOLOUR_CORRIDOR
#error Build with -DAAI05_FOLLOW_MQTT=1 and the multicolour corridor profile
#endif

static follow_remote_permission_t g_permission;
static uint16_t g_epoch;

follow_remote_permission_t follow_remote_take(void)
{
    follow_remote_permission_t result = g_permission;
    g_permission.start = false; /* One-shot, like follow_remote.c. */
    return result;
}

/* One motion-task cycle; the network owner has fed the permission now. */
static void step(black_follow_owner_t *p_owner, uint32_t now,
                 black_follow_status_t *p_follow)
{
    g_now = now;
    g_permission.issued_ms = now;
    uint32_t released = (1U << 20U) | (1U << 21U);
    motor_pulse_status_t motor = {.cutoff_count = g_cutoff_count,
                                  .active = g_active, .ready = true};
    motion_status_t motion;
    black_follow_owner_step(p_owner, released, true, &motor, p_follow, &motion);
}

static void next_sample(uint8_t colour)
{
    g_lane_colour = colour;
    ++g_rgb_sequence;
}

static void reset(black_follow_owner_t *p_owner, uint32_t base, uint8_t lane,
                  black_follow_status_t *p_follow)
{
    g_now = base; g_active = false; g_wrong_target = false;
    g_left = CENTRE_LEFT; g_right = CENTRE_RIGHT;
    g_join_reading = NULL; g_rgb_age_ms = 0U; g_rgb_sequence = 1U;
    g_lane_colour = lane;
    g_permission = (follow_remote_permission_t){0};
    black_follow_owner_init(p_owner);
    step(p_owner, base, p_follow);
    step(p_owner, base + 40U, p_follow);
    assert(p_follow->phase != FOLLOW_RUNNING && !g_active);
}

/* Arm FOLLOW like subsystem2_follow_arm() plus the first network feed. */
static void start(black_follow_owner_t *p_owner, uint32_t now, uint8_t target,
                  black_follow_status_t *p_follow)
{
    g_permission = (follow_remote_permission_t){.epoch = ++g_epoch,
        .armed = true, .fed = true, .start = true, .target = target};
    step(p_owner, now, p_follow);
}

static void check_start_refused_off_the_selected_lane(void)
{
    black_follow_owner_t owner;
    black_follow_status_t follow;
    reset(&owner, 1000U, RGB_CLASS_BLUE, &follow);
    unsigned int arms = g_arm_calls;
    start(&owner, 1050U, RGB_CLASS_RED, &follow);
    assert(follow.phase == FOLLOW_STOPPED && follow.reason == FOLLOW_TARGET_MISMATCH);
    assert(!g_active && g_arm_calls == arms);
    step(&owner, 1060U, &follow);
    assert(follow.phase == FOLLOW_STOPPED && !g_active); /* Start is one-shot. */
}

static void check_blue_lane_ends_when_red_is_confirmed(void)
{
    black_follow_owner_t owner;
    black_follow_status_t follow;
    reset(&owner, 2000U, RGB_CLASS_BLUE, &follow);
    start(&owner, 2050U, RGB_CLASS_BLUE, &follow);
    assert(follow.phase == FOLLOW_RUNNING && g_active);
    step(&owner, 2060U, &follow);
    assert(follow.phase == FOLLOW_RUNNING && g_active);
    next_sample(RGB_CLASS_RED);
    step(&owner, 2070U, &follow);
    assert(follow.phase == FOLLOW_RUNNING && g_active); /* One sample only. */
    next_sample(RGB_CLASS_RED);
    step(&owner, 2080U, &follow);
    assert(follow.phase == FOLLOW_STOPPED && follow.reason == FOLLOW_TARGET_END);
    assert(!g_active && g_applied_left == 0 && g_applied_right == 0);
    step(&owner, 2090U, &follow);
    assert(follow.phase == FOLLOW_STOPPED && !g_active); /* No automatic restart. */
}

static void check_single_stray_frame_keeps_the_lane(void)
{
    black_follow_owner_t owner;
    black_follow_status_t follow;
    reset(&owner, 3000U, RGB_CLASS_BLUE, &follow);
    start(&owner, 3050U, RGB_CLASS_BLUE, &follow);
    next_sample(RGB_CLASS_RED);
    step(&owner, 3060U, &follow);
    next_sample(RGB_CLASS_BLUE);
    step(&owner, 3070U, &follow);
    for (uint32_t t = 3080U; t <= 3200U; t += 10U)
    {
        if ((t % 30U) == 0U)
        {
            next_sample(RGB_CLASS_BLUE);
        }
        step(&owner, t, &follow);
        assert(follow.phase == FOLLOW_RUNNING && g_active);
    }
}

static void check_whole_route_still_joins_the_next_lane(void)
{
    black_follow_owner_t owner;
    black_follow_status_t follow;
    reset(&owner, 4000U, RGB_CLASS_BLUE, &follow);
    start(&owner, 4050U, FOLLOW_TARGET_ANY, &follow);
    assert(follow.phase == FOLLOW_RUNNING && g_active);
    for (uint32_t t = 4060U; t <= 4100U; t += 10U)
    {
        next_sample(RGB_CLASS_RED);
        step(&owner, t, &follow);
        assert(follow.reason != FOLLOW_TARGET_END);
    }
    assert(follow.phase == FOLLOW_RUNNING && g_active);
}

static void check_red_lane_selected_ends_at_yellow(void)
{
    black_follow_owner_t owner;
    black_follow_status_t follow;
    reset(&owner, 5000U, RGB_CLASS_RED, &follow);
    start(&owner, 5050U, RGB_CLASS_RED, &follow);
    assert(follow.phase == FOLLOW_RUNNING && g_active);
    next_sample(RGB_CLASS_YELLOW);
    step(&owner, 5060U, &follow);
    next_sample(RGB_CLASS_YELLOW);
    step(&owner, 5070U, &follow);
    assert(follow.phase == FOLLOW_STOPPED && follow.reason == FOLLOW_TARGET_END && !g_active);
}

int main(void)
{
    check_start_refused_off_the_selected_lane();
    check_blue_lane_ends_when_red_is_confirmed();
    check_single_stray_frame_keeps_the_lane();
    check_whole_route_still_joins_the_next_lane();
    check_red_lane_selected_ends_at_yellow();
    puts("Follow target: refused start, lane end, stray frame, whole route PASS");
    return 0;
}
