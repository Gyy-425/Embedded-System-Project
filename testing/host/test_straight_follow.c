#include "black_follow.h"
#include <assert.h>
#include <stdio.h>

static void check_blue_ambiguity_threshold(bool straight_mode)
{
    black_follow_t state;
    black_follow_init(&state, 100U);
    black_follow_input_t input = {
        .now_ms = 100U, .ir_ms = 100U, .left_raw = 640U, .right_raw = 610U,
        .has_ir = true, .ir_valid = true, .hardware_ready = true,
        .rgb_target_matched = true, .rgb_healthy = true, .blue_surface = true,
        .straight_mode = straight_mode, .remote_start = true,
    };
    black_follow_output_t output = black_follow_step(&state, &input);
    assert(state.status.ready && output.start && output.drive);

    input.remote_start = false;
    input.now_ms = input.ir_ms = 120U;
    input.left_raw = 710U;
    input.right_raw = 699U;
    output = black_follow_step(&state, &input);
    assert(output.drive);

    input.now_ms = input.ir_ms = 140U;
    input.right_raw = 700U;
    output = black_follow_step(&state, &input);
    assert(!output.drive && state.status.reason == FOLLOW_AMBIGUOUS);

    input.now_ms = input.ir_ms = 160U;
    input.left_raw = 640U;
    input.right_raw = 610U;
    input.remote_start = true;
    output = black_follow_step(&state, &input);
    assert(state.status.ready && output.start && output.drive);

    input.now_ms = input.ir_ms = 180U;
    input.remote_start = false;
    input.rgb_target_matched = false;
    input.rgb_join_active = true;
    output = black_follow_step(&state, &input);
    assert(!output.drive && state.status.reason == FOLLOW_AMBIGUOUS);

    input.now_ms = input.ir_ms = 200U;
    input.remote_start = true;
    input.rgb_target_matched = true;
    input.rgb_join_active = false;
    input.blue_surface = false;
    input.red_surface = true;
    input.left_raw = input.right_raw = 500U;
    output = black_follow_step(&state, &input);
    assert(!state.status.ready && !output.start && !output.drive);
}

int main(void)
{
    check_blue_ambiguity_threshold(true);
    check_blue_ambiguity_threshold(false);
    black_follow_t state;
    black_follow_init(&state, 100U);
    black_follow_input_t input = {
        .now_ms = 100U, .ir_ms = 100U, .left_raw = 270U, .right_raw = 215U,
        .has_ir = true, .ir_valid = true, .hardware_ready = true,
        .rgb_target_matched = true, .rgb_healthy = true, .blue_surface = true,
        .straight_mode = true, .remote_start = true,
    };
    black_follow_output_t output = black_follow_step(&state, &input);
    assert(output.start && output.drive && !output.pivot && output.forward_pct == 50);
    assert(output.turn_pct == 0);
    input.remote_start = false;
    input.now_ms = input.ir_ms = 120U;
    input.left_raw = 690U;
    input.blue_exit_fade = true;
    output = black_follow_step(&state, &input);
    assert(output.drive && !output.pivot && !output.blue_exit_pivot);
    assert(output.forward_pct == 50 && output.turn_pct > 0 &&
           output.turn_pct <= FOLLOW_TURN_LIMIT_PCT);

    input.now_ms = input.ir_ms = 140U;
    input.left_raw = 270U;
    input.rgb_target_matched = false;
    input.rgb_join_active = true;
    input.blue_red_blend = true;
    output = black_follow_step(&state, &input);
    assert(output.drive && !output.pivot && output.turn_pct == 0);

    input.now_ms = input.ir_ms = 160U;
    input.rgb_target_matched = true;
    input.rgb_join_active = false;
    input.blue_surface = false;
    input.left_raw = 240U;
    input.right_raw = 550U;
    input.red_surface = true;
    input.red_exit_fade = true;
    output = black_follow_step(&state, &input);
    assert(output.drive && !output.pivot && output.forward_pct == 50);
    assert(output.turn_pct < 0 && output.turn_pct >= -FOLLOW_TURN_LIMIT_PCT);

    input.now_ms = input.ir_ms = 180U;
    input.rgb_target_matched = false;
    input.rgb_join_timed_out = true;
    output = black_follow_step(&state, &input);
    assert(!output.drive && state.status.reason == FOLLOW_JOIN_TIMEOUT);
    puts("PASS: straight start, symmetric bounded steering, join crossing and no bend manoeuvres");
    return 0;
}
