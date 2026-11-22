/* Literal corridor geometry and existing bounded-run safety contract. */
#include "black_follow.h"
#include <assert.h>
#include <stdio.h>

static black_follow_input_t healthy(uint32_t now)
{
    return (black_follow_input_t){.now_ms=now, .ir_ms=now,
        .left_raw=255U, .right_raw=236U, .has_ir=true, .ir_valid=true,
        .rgb_target_matched=true, .hardware_ready=true};
}

static void start(black_follow_t *p_state, uint32_t base)
{
    black_follow_init(p_state, base);
    black_follow_input_t in=healthy(base);
    assert(!black_follow_step(p_state, &in).drive);
    in=healthy(base+40U); (void)black_follow_step(p_state, &in);
    in=healthy(base+50U); in.start_pressed=true;
    (void)black_follow_step(p_state, &in);
    in=healthy(base+90U); in.start_pressed=true;
    black_follow_output_t out=black_follow_step(p_state, &in);
    assert(out.start && out.drive && out.turn_pct==0);
}

int main(void)
{
    black_follow_t state;
    start(&state,100U);
    black_follow_input_t in=healthy(200U);
    in.left_raw=500U;
    black_follow_output_t out=black_follow_step(&state,&in);
    assert(out.drive && out.turn_pct==8); /* Left border: steer RIGHT. */
    in=healthy(220U); in.right_raw=500U;
    out=black_follow_step(&state,&in);
    assert(out.drive && out.turn_pct==-8); /* Right border: steer LEFT. */
    in=healthy(240U); in.left_raw=378U;
    out=black_follow_step(&state,&in);
    assert(out.drive && out.turn_pct==4); /* Gradual response below500. */
    in=healthy(260U); in.left_raw=500U; in.right_raw=500U;
    assert(!black_follow_step(&state,&in).drive);
    assert(state.status.reason==FOLLOW_AMBIGUOUS);
    in=healthy(280U);
    assert(!black_follow_step(&state,&in).drive); /* No auto-restart. */
    start(&state,1000U); in=healthy(1100U); in.rgb_target_matched=false;
    assert(!black_follow_step(&state,&in).drive);
    assert(state.status.reason==FOLLOW_RGB);
    start(&state,2000U); in=healthy(2100U); in.stop_pressed=true;
    assert(!black_follow_step(&state,&in).drive);
    assert(state.status.reason==FOLLOW_BUTTON);
    start(&state,3000U); in=healthy(3191U); in.ir_ms=3090U;
    assert(!black_follow_step(&state,&in).drive);
    assert(state.status.reason==FOLLOW_IR);
    start(&state,4000U); in=healthy(4100U); in.ir_valid=false;
    assert(!black_follow_step(&state,&in).drive);
    assert(state.status.reason==FOLLOW_IR);
    start(&state,5000U); in=healthy(5100U); in.hardware_ready=false;
    assert(!black_follow_step(&state,&in).drive);
    assert(state.status.reason==FOLLOW_HARDWARE);
    start(&state,6000U); in=healthy(7090U);
    assert(!black_follow_step(&state,&in).drive);
    assert(state.status.reason==FOLLOW_COMPLETE);
    start(&state,UINT32_MAX-100U); in=healthy(989U);
    assert(!black_follow_step(&state,&in).drive);
    assert(state.status.reason==FOLLOW_COMPLETE);
    black_follow_init(&state,0U);
    for(uint32_t t=0U;t<200U;t+=20U)
    {
        in=healthy(t); in.start_pressed=true;
        assert(!black_follow_step(&state,&in).drive);
    }
    puts("Bordered corridor: border polarity, gradual correction, STOP/restart/freshness/wrap PASS");
    return 0;
}
