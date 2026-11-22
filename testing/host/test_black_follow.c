#include "black_follow.h"
#include "../../config/follow_config.h"
#if FOLLOW_TURN_LIMIT_PCT == 8
#define EXPECT_TURN 8
#elif FOLLOW_TURN_LIMIT_PCT == 20
#define EXPECT_TURN 20
#else
#error Steering fixtures use literal8/20 limits
#endif
#include <assert.h>
#include <stdio.h>

static black_follow_input_t healthy(uint32_t now)
{
    return (black_follow_input_t){.now_ms=now, .ir_ms=now, .left_raw=268U,
        .right_raw=304U, .ir_valid=true, .has_ir=true,
        .rgb_target_matched=true, .hardware_ready=true};
}

static black_follow_output_t start(black_follow_t *p_state, uint32_t now)
{
    black_follow_init(p_state, now);
    black_follow_input_t in = healthy(now);
    (void)black_follow_step(p_state, &in);
    in = healthy(now + 40U); (void)black_follow_step(p_state, &in);
    in.start_pressed = true; in.now_ms = now + 50U;
    in.ir_ms = in.now_ms; (void)black_follow_step(p_state, &in);
    in.now_ms = now + 90U; in.ir_ms = in.now_ms;
    return black_follow_step(p_state, &in);
}

int main(void)
{
    black_follow_t state;
    black_follow_output_t out = start(&state, 100U);
    assert(out.start && out.drive && out.turn_pct == 0 && out.forward_pct == 50);
    black_follow_input_t in = healthy(200U);
    in.right_raw = 1444U;
    out = black_follow_step(&state, &in);
    assert(out.drive && out.turn_pct == EXPECT_TURN && !out.start);
    in.left_raw = 1368U; in.right_raw = 304U;
    out = black_follow_step(&state, &in);
    assert(out.drive && out.turn_pct == -EXPECT_TURN);
    in.left_raw = 4095U; in.right_raw = 4095U;
    out = black_follow_step(&state, &in);
    assert(!out.drive && state.status.reason == FOLLOW_AMBIGUOUS);
    in = healthy(220U); out = black_follow_step(&state, &in);
    assert(!out.drive); /* No automatic restart after sensor recovery. */
    out = start(&state, 1000U);
    in = healthy(1091U); in.stop_pressed = true;
    out = black_follow_step(&state, &in);
    assert(!out.drive && state.status.reason == FOLLOW_BUTTON);
    out = start(&state, 2000U);
    in = healthy(2091U); in.rgb_target_matched = false;
    out = black_follow_step(&state, &in);
    assert(!out.drive && state.status.reason == FOLLOW_RGB);
    out = start(&state, 3000U);
    in = healthy(3191U); in.ir_ms = 3090U;
    out = black_follow_step(&state, &in);
    assert(!out.drive && state.status.reason == FOLLOW_IR);
    out = start(&state, 4000U);
    in = healthy(4091U); in.left_raw = 4096U;
    out = black_follow_step(&state, &in);
    assert(!out.drive && state.status.reason == FOLLOW_IR);
    out = start(&state, 5000U);
    in = healthy(6090U);
    out = black_follow_step(&state, &in);
    assert(!out.drive && state.status.reason == FOLLOW_COMPLETE);
    out = start(&state, UINT32_MAX - 100U);
    assert(out.drive);
    in = healthy(989U); /* Exactly1000ms after wrapped start time. */
    out = black_follow_step(&state, &in);
    assert(!out.drive && state.status.reason == FOLLOW_COMPLETE);
    black_follow_init(&state, 0U);
    for (uint32_t t = 0U; t < 500U; t += 20U)
    {
        in = healthy(t); in.start_pressed = true;
        out = black_follow_step(&state, &in);
        assert(!out.drive); /* Held at boot never starts. */
    }
    out = start(&state, 100U);
    black_follow_abort(&state, FOLLOW_DRIVER);
    in = healthy(200U); in.start_pressed = true;
    assert(!black_follow_step(&state, &in).drive);
    in.start_pressed = false; in.now_ms = 210U; in.ir_ms = 210U;
    (void)black_follow_step(&state, &in);
    in.now_ms = 250U; in.ir_ms = 250U; (void)black_follow_step(&state, &in);
    in.start_pressed = true; in.now_ms = 260U; in.ir_ms = 260U;
    (void)black_follow_step(&state, &in);
    in.now_ms = 300U; in.ir_ms = 300U;
    assert(black_follow_step(&state, &in).start);
    puts("Black follow: direction, start/STOP, stale, ambiguity, deadline/wrap PASS");
    return 0;
}
