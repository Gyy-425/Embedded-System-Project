/* Real owner, steering, classifier and motion policies; fake acquisition/driver. */
#include "black_follow_owner_harness.h"

static void cycle(black_follow_owner_t *p_owner, uint32_t now, bool pressed,
                    bool stop, black_follow_status_t *p_follow)
{
    g_now = now;
    uint32_t buttons = (pressed ? 0U : 1U << 20U) | (stop ? 0U : 1U << 21U);
    motor_pulse_status_t motor = {.cutoff_count=g_cutoff_count, .active=g_active, .ready=true};
    motion_status_t motion;
    black_follow_owner_step(p_owner, buttons, true, &motor, p_follow, &motion);
}

static void begin(black_follow_owner_t *p_owner, uint32_t base,
                    black_follow_status_t *p_follow)
{
    g_now=base; g_active=false; g_ir_busy=false; g_rgb_busy=false; g_rgb_valid=true;
    g_left=CENTRE_LEFT; g_right=CENTRE_RIGHT; g_wrong_target=false; g_drive_error=0; g_rearm_error=0; g_apply_delay_ms=0U;
#if FOLLOW_MULTICOLOUR_CORRIDOR
    g_lane_colour=RGB_CLASS_BLUE;
    g_join_reading=NULL;
    g_rgb_sequence=0U;
    g_rgb_age_ms=0U;
#endif
    black_follow_owner_init(p_owner);
    cycle(p_owner,base,false,false,p_follow);
    assert(g_lease==0U);
    cycle(p_owner,base+40U,false,false,p_follow);
    cycle(p_owner,base+50U,true,false,p_follow);
    cycle(p_owner,base+90U,true,false,p_follow);
    assert(p_follow->phase==FOLLOW_RUNNING);
    assert(g_applied_left==-EXPECT_STRAIGHT && g_applied_right==EXPECT_STRAIGHT && g_lease==90U);
#if FOLLOW_DUTY_HALF_STEP
    assert(p_follow->left_half_pct==-55 && p_follow->right_half_pct==55);
#endif
}

/* Shared RGB and duty fixtures, hoisted unchanged from main(). */
#if (FOLLOW_RED_SECOND_SLOW) && (FOLLOW_RED_BEND_AFTER_HANDOFF)
static const rgb_reading_t blue_fade = {211U,232U,243U,552U};
#endif
#if (FOLLOW_RED_SECOND_SLOW) && (FOLLOW_RED_BEND_AFTER_HANDOFF)
static const rgb_reading_t blue_loss = {219U,236U,243U,553U};
#endif
#if (FOLLOW_RED_SECOND_SLOW) && (FOLLOW_BLUE_RED_JOIN_REFINEMENT)
static const rgb_reading_t blue_near_edge = {214U,241U,257U,561U};
#endif
#if (FOLLOW_RED_SECOND_SLOW) && (FOLLOW_BLUE_RED_JOIN_REFINEMENT) && (FOLLOW_BLUE_EXIT_PIVOT)
static const int8_t expected_pivot_left = FOLLOW_BLUE_EXIT_LEFT_BOOST ? -60 : -50;
#endif
#if (FOLLOW_RED_SECOND_SLOW)
static const rgb_reading_t fading_red = {325U,248U,247U,635U};
#endif
#if (FOLLOW_RED_SECOND_SLOW)
static const int8_t first_left = FOLLOW_RED_FIRST_LEFT_BOOST ? -60 : -50;
#endif
#if (FOLLOW_RED_SECOND_SLOW)
static const rgb_reading_t red_return = {355U,269U,269U,668U};
#endif
#if (FOLLOW_RED_SECOND_SLOW)
static const rgb_reading_t red_second_fade = {350U,270U,270U,660U};
#endif
#if (FOLLOW_RED_SECOND_SLOW)
static const int8_t second_reverse = FOLLOW_RED_SECOND_EXIT_REFINEMENT ? -35 : -25;
#endif
#if !(FOLLOW_RED_SECOND_SLOW) && !(FOLLOW_TWO_TURN) && (FOLLOW_CLIPPED_LOOP) && (FOLLOW_BLUE_EXIT_TURN == 22)
static const rgb_reading_t before_exit = {210U,233U,246U,548U};
#endif
#if !(FOLLOW_RED_SECOND_SLOW) && !(FOLLOW_TWO_TURN) && (FOLLOW_CLIPPED_LOOP) && (FOLLOW_LOOP_RECOVERY_DIRECTION != 0)
static const rgb_reading_t blue_loss = {269U, 279U, 281U, 605U};
#endif
#if !(FOLLOW_RED_SECOND_SLOW) && !(FOLLOW_TWO_TURN) && (FOLLOW_CLIPPED_LOOP) && (FOLLOW_BLUE_EXIT_LEAD)
static const int8_t expected_blue_lead = FOLLOW_BLUE_RED_BEND_CUES ? 25 : FOLLOW_BLUE_EXIT_TURN;
#endif
#if !(FOLLOW_RED_SECOND_SLOW) && !(FOLLOW_TWO_TURN) && (FOLLOW_CLIPPED_LOOP) && (FOLLOW_RED_EXIT_LEAD)
static const int8_t expected_red_lead = FOLLOW_BLUE_RED_BEND_TRIAL ? 25 : 22;
#endif
#if !(FOLLOW_RED_SECOND_SLOW) && !(FOLLOW_TWO_TURN) && (FOLLOW_CLIPPED_LOOP) && (FOLLOW_RED_EXIT_LEAD)
static const rgb_reading_t fading_red = {331U,256U,255U,636U};
#endif
#if !(FOLLOW_RED_SECOND_SLOW) && !(FOLLOW_TWO_TURN) && (FOLLOW_CLIPPED_LOOP) && (FOLLOW_BLUE_EXIT_TURN == 22)
static const rgb_reading_t off_blue = {219U,236U,243U,553U};
#endif
#if !(FOLLOW_RED_SECOND_SLOW) && !(FOLLOW_TWO_TURN) && (FOLLOW_CLIPPED_LOOP) && (FOLLOW_RED_EXIT_LEAD)
static const rgb_reading_t red_background = {276U,258U,248U,602U};
#endif
#if !(FOLLOW_RED_SECOND_SLOW) && !(FOLLOW_TWO_TURN) && (FOLLOW_CLIPPED_LOOP) && (FOLLOW_BLUE_RED_HANDOFF)
static const rgb_reading_t seam[] = {
    {244U, 263U, 279U, 586U}, {260U, 264U, 276U, 596U},
    {291U, 268U, 278U, 623U}, {333U, 273U, 278U, 656U},
    {252U, 269U, 286U, 599U}, {278U, 276U, 290U, 621U},
    {312U, 275U, 284U, 647U},
};
#endif
#if !(FOLLOW_RED_SECOND_SLOW) && !(FOLLOW_TWO_TURN) && (FOLLOW_CLIPPED_LOOP) && (FOLLOW_RED_EXIT_LEAD)
static const rgb_reading_t solid_red = {320U,242U,240U,627U};
#endif
#if !(FOLLOW_RED_SECOND_SLOW) && !(FOLLOW_TWO_TURN) && (FOLLOW_CLIPPED_LOOP) && (FOLLOW_LOOP_RECOVERY_DIRECTION != 0)
static const int8_t turn_direction = FOLLOW_LOOP_RECOVERY_DIRECTION;
#endif
#if !(FOLLOW_RED_SECOND_SLOW) && !(FOLLOW_TWO_TURN) && (FOLLOW_CLIPPED_LOOP) && (FOLLOW_LOOP_RECOVERY_DIRECTION != 0)
static const rgb_reading_t yellow_loss = {315U, 300U, 284U, 688U};
#endif

#if (FOLLOW_RED_SECOND_SLOW) && FOLLOW_RED_BEND_AFTER_HANDOFF
/* Sustained left edge is new evidence on the same fading red frame. */
static void check_red_bend_after_handoff(void)
{
    black_follow_owner_t owner;
    black_follow_status_t follow;
    const rgb_reading_t towards_red = {325U,284U,281U,642U};
    const rgb_reading_t red_first = {335U,263U,261U,646U};
    const rgb_reading_t red_confirmed = {325U,253U,252U,636U};
    const rgb_reading_t red_at_outer_edge = {328U,251U,249U,639U};
    const rgb_reading_t red_mix = {322U,262U,260U,630U};
#if FOLLOW_BLUE_RED_JOIN_REFINEMENT
    const rgb_reading_t blue_blend = {274U,279U,283U,608U};
#if FOLLOW_BLUE_DIAGONAL_EXIT
    const rgb_reading_t late_blend = {283U,279U,285U,617U};
#endif
    const rgb_reading_t late_red = {320U,272U,273U,642U};
    const rgb_reading_t confirmed_red_fade = {325U,273U,270U,636U};
#endif
    const int8_t first_reverse = FOLLOW_RED_FIRST_BEND_TIGHTER ? -40 : -30;
    const unsigned handoff_cases = 1U + FOLLOW_RED_FIRST_BEND_BOOST +
        FOLLOW_RED_SECOND_EXIT_REFINEMENT + 2U * FOLLOW_RED_BETWEEN_BENDS + FOLLOW_RED_RETURN_GRACE;
    const uint32_t alignment_delay = FOLLOW_RED_HANDOFF_ALIGN ? 120U : 0U;
#if FOLLOW_RED_EXIT_ALIGN && !FOLLOW_RED_EXIT_TURN_STRONG
    const rgb_reading_t red_exit_mix = {325U,267U,264U,633U};
#endif
    const unsigned diagonal_cases = FOLLOW_RED_DIAGONAL_REACQUIRE ? 3U : 0U;
    for (unsigned handoff_case=0U; handoff_case<handoff_cases+diagonal_cases; ++handoff_case)
    {
        bool outer_edge = handoff_case == 1U;
#if FOLLOW_RED_EXIT_TURN && !FOLLOW_RED_EXIT_TURN_STRONG
        const rgb_reading_t late_red_fade = FOLLOW_RED_EXIT_TURN_EARLY ?
            (rgb_reading_t){325U,250U,249U,632U} : (rgb_reading_t){323U,256U,254U,633U};
#endif
        begin(&owner,3000U,&follow);
        g_left=1058U; g_right=251U;
        cycle(&owner,3110U,false,false,&follow);
        cycle(&owner,3150U,false,false,&follow);
        g_left=224U;
        g_right=FOLLOW_BLUE_EXIT_EDGE_CONFIRM ? 367U : 220U;
        g_join_reading=&blue_fade; ++g_rgb_sequence;
        cycle(&owner,3190U,false,false,&follow);
        assert(g_active && follow.blue_exit_lead_used);
        g_right=220U;
        g_join_reading=&blue_loss;
        for (uint32_t now = 3230U; now < 3630U; now += 40U)
        {
#if FOLLOW_BLUE_RED_JOIN_REFINEMENT
            if (!outer_edge && now == 3590U)
            {
                g_join_reading=&blue_blend;
#if FOLLOW_BLUE_DIAGONAL_EXIT
                g_join_reading=&late_blend;
#endif
            }
#endif
            ++g_rgb_sequence;
            cycle(&owner,now,false,false,&follow);
            assert(g_active && follow.join_active);
        }
        g_join_reading=&towards_red;
#if FOLLOW_BLUE_RED_JOIN_REFINEMENT
        if (!outer_edge)
        {
            assert(follow.blue_red_join_used && !owner.policy.blue_exit_lead_seen);
#if FOLLOW_BLUE_DIAGONAL_EXIT
            assert(follow.left_half_pct==-40 && follow.right_half_pct==20);
#endif
            g_join_reading=&late_red;
        }
#endif
        ++g_rgb_sequence;
        cycle(&owner,3630U,false,false,&follow);
        cycle(&owner,3690U,false,false,&follow);
        g_join_reading=&red_first; ++g_rgb_sequence;
        cycle(&owner,3730U,false,false,&follow);
        assert(g_active && follow.join_active && !follow.red_second_pivot_used);
        g_join_reading=outer_edge ? &red_at_outer_edge : &red_confirmed; ++g_rgb_sequence;
        cycle(&owner,3790U,false,false,&follow);
        assert(g_active && !follow.join_active && follow.join_count==1U);
        assert(follow.left_half_pct==-40 && follow.red_second_slow_used);
        assert(follow.right_half_pct==(FOLLOW_RED_HANDOFF_ALIGN ? 30 : 40));
        assert(follow.red_handoff_armed);
#if FOLLOW_RED_HANDOFF_ALIGN
        const rgb_reading_t alignment_mix = {332U,269U,266U,639U};
        assert(follow.red_alignment_used && !follow.red_first_pivot_used);
        if (outer_edge)
        {
            g_left=724U; g_right=207U;
            cycle(&owner,3830U,false,false,&follow);
            cycle(&owner,3890U,false,false,&follow);
            assert(g_active && follow.left_half_pct==-40 && follow.right_half_pct==30);
        }
        else
        {
            /* An inside-edge reading still gets its regular correction.
             * v0.85: in the red corner a right edge now spins right. */
            g_right=650U;
            cycle(&owner,3830U,false,false,&follow);
#if FOLLOW_RED_RIGHT_EDGE_OUTSIDE && !FOLLOW_RED_CORNER_REFINE
            assert(g_active && follow.turn_pct==25 && follow.right_half_pct<0);
#else
            assert(g_active && follow.turn_pct<0);
#endif
            g_right=202U;
            g_join_reading=&alignment_mix; ++g_rgb_sequence;
            cycle(&owner,3890U,false,false,&follow);
            assert(g_active && follow.join_active && !follow.red_first_pivot_used);
            assert(follow.left_half_pct==-40 && follow.right_half_pct==30);
        }
#endif
        if (outer_edge)
        {
            g_left=749U;
            cycle(&owner,3810U+alignment_delay,false,false,&follow);
            assert(g_active && !follow.red_second_pivot_used);
            /* Sustained left edge is new evidence on the same fading red frame. */
            cycle(&owner,3860U+alignment_delay,false,false,&follow);
            assert(g_active && !follow.red_second_pivot_used);
            g_join_reading=&fading_red;
        }
        else
        {
            cycle(&owner,3830U+alignment_delay,false,false,&follow);
            assert(g_active && !follow.red_second_pivot_used);
            g_join_reading=&red_mix;
#if FOLLOW_BLUE_RED_JOIN_REFINEMENT
            g_join_reading=&confirmed_red_fade;
#endif
        }
        ++g_rgb_sequence;
        cycle(&owner,3890U+alignment_delay,false,false,&follow);
        assert(g_active && follow.join_active == !outer_edge);
#if FOLLOW_RED_FIRST_BEND_BOOST
        assert(follow.red_first_pivot_used && !follow.red_second_pivot_used);
#else
        assert(follow.red_second_pivot_used);
#endif
        assert(follow.left_half_pct==(FOLLOW_RED_FIRST_BEND_BOOST ? first_left : -45));
        assert(follow.right_half_pct==(FOLLOW_RED_FIRST_BEND_BOOST ? first_reverse : -25));
        g_left=224U;
        uint32_t yellow_ms = outer_edge ? 4170U : 3930U;
        if (outer_edge)
        {
            /* Reacquiring solid red after the handoff turn still permits
             * the original, separately timed second red pivot. */
            cycle(&owner,3950U+alignment_delay,false,false,&follow);
            cycle(&owner,4010U+alignment_delay,false,false,&follow);
            assert(g_active && follow.left_half_pct==first_left && follow.right_half_pct==first_reverse);
            cycle(&owner,4050U+alignment_delay,false,false,&follow);
            assert(g_active && follow.left_half_pct==-40 && follow.right_half_pct==0);
            g_join_reading=&red_return; ++g_rgb_sequence;
            cycle(&owner,4090U+alignment_delay,false,false,&follow);
            assert(g_active && follow.left_half_pct==-40 && follow.right_half_pct==40);
            const rgb_reading_t offset_red_fade = {336U,257U,255U,646U};
            if (FOLLOW_BEND_CUE_REFINEMENT)
            {
                g_left=320U; g_right=217U;
            }
            g_join_reading=FOLLOW_BEND_CUE_REFINEMENT ? &offset_red_fade : &red_second_fade;
            ++g_rgb_sequence;
            cycle(&owner,4130U+alignment_delay,false,false,&follow);
            assert(g_active && follow.red_second_pivot_used);
            assert(follow.left_half_pct==-45);
            assert(follow.right_half_pct==second_reverse);
#if FOLLOW_RED_EXIT_ALIGN
            g_left=224U; g_right=220U;
            for (uint32_t now=4190U; now<=4430U; now+=60U)
            {
                ++g_rgb_sequence;
                cycle(&owner,now+alignment_delay,false,false,&follow);
                assert(g_active);
            }
#if FOLLOW_RED_EXIT_TURN_STRONG
            const int8_t final_arc_left = FOLLOW_RED_FINAL_ARC_STRONG ? -70 :
                (FOLLOW_RED_FINAL_LEFT_BOOST ? -60 : -50);
#if FOLLOW_RED_FINAL_ARC_EXTEND
            const int8_t extended_arc_left = FOLLOW_RED_FINAL_ARC_STRONG ? -80 : -70;
#endif
            const rgb_reading_t early_exit_fade = {317U,246U,244U,626U};
            g_left=443U; g_right=220U;
            cycle(&owner,4500U+alignment_delay,false,false,&follow);
            assert(g_active && !follow.red_exit_alignment_used && !follow.red_exit_turn_used);

            g_join_reading=&early_exit_fade; ++g_rgb_sequence;
            cycle(&owner,4530U+alignment_delay,false,false,&follow);
            assert(g_active && follow.red_exit_turn_used);
            assert(follow.left_half_pct==final_arc_left && follow.right_half_pct==10);
            assert(g_applied_left==final_arc_left/2 && g_applied_right==5);

            g_left=251U; g_right=235U;
            cycle(&owner,4590U+alignment_delay,false,false,&follow);
            assert(g_active && follow.left_half_pct==final_arc_left && follow.right_half_pct==10);
            g_right=456U;
            cycle(&owner,4630U+alignment_delay,false,false,&follow);
#if FOLLOW_RED_RIGHT_EDGE_OUTSIDE
            assert(g_active && follow.turn_pct==25 && follow.right_half_pct<0);
#else
            assert(g_active && follow.turn_pct<0 && follow.left_half_pct>=-50);
#endif

            g_right=220U;
#if FOLLOW_RED_SPIN_HOLD
            /* v0.87: the spin holds on the old frame; a fresh red frame ends it. */
            cycle(&owner,4660U+alignment_delay,false,false,&follow);
            assert(g_active && follow.turn_pct==25 && follow.right_half_pct<0);
            ++g_rgb_sequence;
#endif
            cycle(&owner,4700U+alignment_delay,false,false,&follow);
#if FOLLOW_RED_EXIT_TURN_HOLD
            assert(g_active && follow.left_half_pct==final_arc_left && follow.right_half_pct==10);
            g_right=289U;
            cycle(&owner,4730U+alignment_delay,false,false,&follow);
            assert(g_active && follow.turn_pct<0);
            g_right=220U;
            cycle(&owner,4750U+alignment_delay,false,false,&follow);
            assert(g_active && follow.left_half_pct==final_arc_left && follow.right_half_pct==10);

            const rgb_reading_t red_like_loss = {308U,251U,248U,616U};
            g_join_reading=&red_like_loss; ++g_rgb_sequence;
            cycle(&owner,4780U+alignment_delay,false,false,&follow);
            assert(g_active && follow.join_active && follow.right_half_pct!=10 &&
                   follow.left_half_pct>=-50);
            g_join_reading=&early_exit_fade; ++g_rgb_sequence;
            cycle(&owner,4820U+alignment_delay,false,false,&follow);
#if FOLLOW_RED_FINAL_ARC_EXTEND
            /* v0.79 was still on steady red when the 280 ms arc expired. */
            const rgb_reading_t late_steady_red = {335U,269U,267U,643U};
            g_join_reading=&late_steady_red;
#endif
            ++g_rgb_sequence;
            cycle(&owner,4860U+alignment_delay,false,false,&follow);
#if FOLLOW_RED_FINAL_ARC_EXTEND
            assert(g_active && !follow.join_active &&
                   follow.left_half_pct==extended_arc_left && follow.right_half_pct==10);
            assert(g_applied_left==extended_arc_left/2 && g_applied_right==5);
            g_right=331U;
            cycle(&owner,4880U+alignment_delay,false,false,&follow);
            assert(g_active && follow.turn_pct<0 && follow.left_half_pct>=-50);
            g_right=220U;
            cycle(&owner,4900U+alignment_delay,false,false,&follow);
            assert(g_active && follow.left_half_pct==extended_arc_left && follow.right_half_pct==10);
            cycle(&owner,4920U+alignment_delay,false,false,&follow);
            yellow_ms=4960U;
#else
            yellow_ms=4900U;
#endif
            assert(g_active && !follow.join_active &&
                   follow.left_half_pct==-50 && follow.right_half_pct==50);
#else
            assert(g_active && follow.left_half_pct==-40 && follow.right_half_pct==30);
            cycle(&owner,4750U+alignment_delay,false,false,&follow);
            assert(g_active && follow.left_half_pct==-50 && follow.right_half_pct==50);
            yellow_ms=4790U;
#endif
#else
            cycle(&owner,4500U+alignment_delay,false,false,&follow);
            assert(g_active && follow.red_exit_alignment_used);
            assert(follow.left_half_pct==-40 && follow.right_half_pct==30);

            g_right=265U;
            g_join_reading=&red_exit_mix; ++g_rgb_sequence;
            cycle(&owner,4560U+alignment_delay,false,false,&follow);
            assert(g_active && follow.join_active);
            assert(follow.left_half_pct==-40 && follow.right_half_pct==30);

            g_right=500U;
            cycle(&owner,4600U+alignment_delay,false,false,&follow);
            assert(g_active && follow.turn_pct<0);

            g_right=220U;
#if FOLLOW_RED_EXIT_TURN_EARLY
            g_join_reading=&late_red_fade; ++g_rgb_sequence;
            cycle(&owner,4640U+alignment_delay,false,false,&follow);
            assert(g_active && follow.join_active && !follow.red_exit_turn_used);
            ++g_rgb_sequence;
            cycle(&owner,4670U+alignment_delay,false,false,&follow);
            assert(g_active && !follow.join_active && follow.red_exit_turn_used);
            assert(follow.left_half_pct==-45 &&
                   follow.right_half_pct==(FOLLOW_RED_EXIT_TURN_TIGHTER ? 15 : 25));
            cycle(&owner,4730U+alignment_delay,false,false,&follow);
            assert(g_active && follow.left_half_pct==-45 &&
                   follow.right_half_pct==(FOLLOW_RED_EXIT_TURN_TIGHTER ? 15 : 25));

            g_right=456U;
            cycle(&owner,4770U+alignment_delay,false,false,&follow);
            assert(g_active && follow.turn_pct<0);
            g_right=220U;
            cycle(&owner,4830U+alignment_delay,false,false,&follow);
            assert(g_active && follow.left_half_pct==-50 && follow.right_half_pct==50);
            yellow_ms=4870U;
#else
            cycle(&owner,4670U+alignment_delay,false,false,&follow);
            assert(g_active && follow.left_half_pct==-40 && follow.right_half_pct==30);
            cycle(&owner,4740U+alignment_delay,false,false,&follow);
            assert(g_active && follow.left_half_pct==-50 && follow.right_half_pct==50);
            yellow_ms=4790U;
#if FOLLOW_RED_EXIT_TURN
            g_join_reading=&late_red_fade; ++g_rgb_sequence;
            cycle(&owner,4780U+alignment_delay,false,false,&follow);
            assert(g_active && follow.join_active && follow.right_half_pct==50);
            ++g_rgb_sequence;
            cycle(&owner,4820U+alignment_delay,false,false,&follow);
            assert(g_active && !follow.join_active && follow.red_exit_turn_used);
            assert(follow.left_half_pct==-45 && follow.right_half_pct==25);

            g_right=289U;
            cycle(&owner,4860U+alignment_delay,false,false,&follow);
            assert(g_active && follow.left_half_pct==-45 && follow.right_half_pct==25);
            g_right=650U;
            cycle(&owner,4900U+alignment_delay,false,false,&follow);
            assert(g_active && follow.turn_pct<0);

            g_right=220U;
            cycle(&owner,4940U+alignment_delay,false,false,&follow);
            assert(g_active && follow.left_half_pct==-45 && follow.right_half_pct==25);
            cycle(&owner,4980U+alignment_delay,false,false,&follow);
            assert(g_active && follow.left_half_pct==-50 && follow.right_half_pct==50);
            yellow_ms=5020U;
#endif
#endif
#endif
#endif
        }
#if FOLLOW_RED_SECOND_EXIT_REFINEMENT
        if (handoff_case == 2U)
        {
            const rgb_reading_t red_loss = {338U,275U,273U,643U};
            const rgb_reading_t red_return_first = {339U,272U,270U,645U};
            const rgb_reading_t red_return_confirmed = {324U,251U,249U,634U};
            g_join_reading=&fading_red;
            for (uint32_t now=3930U; now<=4230U; now+=60U)
            {
                ++g_rgb_sequence;
                cycle(&owner,now+alignment_delay,false,false,&follow);
                assert(g_active && !follow.red_second_pivot_used);
            }
            g_join_reading=&red_loss; ++g_rgb_sequence;
            cycle(&owner,4250U+alignment_delay,false,false,&follow);
            cycle(&owner,4310U+alignment_delay,false,false,&follow);
            cycle(&owner,4370U+alignment_delay,false,false,&follow);
            assert(g_active && follow.join_active);
            g_join_reading=&red_return_first; ++g_rgb_sequence;
            cycle(&owner,4410U+alignment_delay,false,false,&follow);
            cycle(&owner,4450U+alignment_delay,false,false,&follow);
            assert(g_active && follow.join_active && !follow.red_second_pivot_used);
            g_left=223U; g_right=251U;
            g_join_reading=&red_return_confirmed; ++g_rgb_sequence;
            cycle(&owner,4490U+alignment_delay,false,false,&follow);
            assert(g_active && !follow.join_active && follow.join_count==1U);
            assert(follow.red_second_pivot_used && follow.left_half_pct==-45 && follow.right_half_pct==-35);
            yellow_ms=4530U;
        }
#endif
#if FOLLOW_RED_DIAGONAL_REACQUIRE
        if (handoff_case >= handoff_cases)
        {
            const rgb_reading_t recovered_red = {352U,266U,265U,664U};
            const rgb_reading_t diagonal_red[] = {{370U,286U,286U,678U}, {365U,285U,285U,672U}};
            const rgb_reading_t diagonal_fade[] = {{299U,246U,241U,610U}, {293U,249U,244U,606U}};
            const unsigned trial = handoff_case-handoff_cases;
            const unsigned capture = trial == 0U ? 0U : 1U;
            const bool inside_edge = trial < 2U;

            g_left=224U; g_right=220U;
            for (uint32_t now=3930U; now<=4050U; now+=60U)
            {
                cycle(&owner,now+alignment_delay,false,false,&follow);
            }
            g_join_reading=&recovered_red; ++g_rgb_sequence;
            cycle(&owner,4090U+alignment_delay,false,false,&follow);
            assert(g_active && follow.red_first_pivot_used && !follow.red_second_pivot_used);
            assert(!owner.policy.red_first_active);
            cycle(&owner,4150U+alignment_delay,false,false,&follow);

            g_join_reading=&diagonal_red[capture]; ++g_rgb_sequence;
            cycle(&owner,4210U+alignment_delay,false,false,&follow);
            assert(g_active && !follow.red_second_pivot_used);
            cycle(&owner,4270U+alignment_delay,false,false,&follow);
            g_right=inside_edge ? 622U : 220U;
            g_join_reading=&diagonal_fade[capture]; ++g_rgb_sequence;
            cycle(&owner,4330U+alignment_delay,false,false,&follow);
            assert(g_active && follow.join_active && !follow.red_second_pivot_used);

            g_right=218U;
            cycle(&owner,4370U+alignment_delay,false,false,&follow);
            assert(g_active && follow.red_second_pivot_used==inside_edge);
            if (inside_edge)
            {
                assert(follow.red_clear_pivot_used && follow.left_half_pct==-45 &&
                       follow.right_half_pct==second_reverse);
            }
            continue;
        }
#endif
#if FOLLOW_RED_BETWEEN_BENDS
        if (handoff_case >= 3U)
        {
            const rgb_reading_t bright_red = FOLLOW_RED_INSIDE_RELEASE ?
                (rgb_reading_t){343U,261U,261U,654U} : (rgb_reading_t){348U,267U,266U,660U};
            const rgb_reading_t bright_before_mix = {356U,279U,279U,666U};
            const rgb_reading_t bright_still_red = {348U,266U,266U,659U};
            const rgb_reading_t red_drop = {327U,252U,250U,636U};
            const rgb_reading_t red_drop_mix = {328U,272U,270U,633U};
            bool drops_to_mix = handoff_case >= 4U;

            g_join_reading=&fading_red;
            for (uint32_t now=3930U; now<=4050U; now+=60U)
            {
                ++g_rgb_sequence;
                cycle(&owner,now+alignment_delay,false,false,&follow);
                assert(g_active && !follow.red_second_pivot_used);
            }
            g_join_reading=drops_to_mix ? &bright_before_mix : &bright_red;
            ++g_rgb_sequence;
            cycle(&owner,4090U+alignment_delay,false,false,&follow);
            cycle(&owner,4150U+alignment_delay,false,false,&follow);
            g_join_reading=drops_to_mix ? &red_drop_mix :
                (FOLLOW_RED_INSIDE_RELEASE ? &bright_red : &bright_still_red);
            ++g_rgb_sequence;
            cycle(&owner,4210U+alignment_delay,false,false,&follow);
            assert(g_active && !follow.red_second_pivot_used);

            g_right=650U;
            g_join_reading=drops_to_mix ? &red_drop_mix : &red_drop;
            ++g_rgb_sequence;
            cycle(&owner,4270U+alignment_delay,false,false,&follow);
            assert(g_active && !follow.red_second_pivot_used);

            g_right=217U;
            cycle(&owner,4330U+alignment_delay,false,false,&follow);
            assert(g_active && follow.red_second_pivot_used);
            assert(follow.join_active == drops_to_mix && follow.red_clear_pivot_used);
            assert(follow.left_half_pct==-45 && follow.right_half_pct==second_reverse);
            yellow_ms=4370U;
#if FOLLOW_RED_INSIDE_RELEASE
            g_right=765U;
            cycle(&owner,4370U+alignment_delay,false,false,&follow);
            assert(g_active && follow.right_half_pct==second_reverse);
            cycle(&owner,4410U+alignment_delay,false,false,&follow);
#if FOLLOW_RED_RIGHT_EDGE_OUTSIDE
            assert(g_active && follow.red_second_release_used && follow.turn_pct==25 &&
                   follow.right_half_pct<0);
#else
            assert(g_active && follow.red_second_release_used && follow.turn_pct<0 && follow.right_half_pct>0);
#endif

            g_right=220U;
#if FOLLOW_RED_SPIN_HOLD
            /* v0.87: still spinning on the old frame; one fresh red frame ends
             * the hold, then the scenario's own reading returns. */
            cycle(&owner,4430U+alignment_delay,false,false,&follow);
            assert(g_active && follow.turn_pct==25 && follow.right_half_pct<0);
            g_join_reading=&red_drop; ++g_rgb_sequence;
            cycle(&owner,4450U+alignment_delay,false,false,&follow);
            g_join_reading=drops_to_mix ? &red_drop_mix : &red_drop; ++g_rgb_sequence;
#endif
            cycle(&owner,4470U+alignment_delay,false,false,&follow);
            assert(g_active && follow.red_exit_alignment_used);
            assert(follow.left_half_pct==-40 && follow.right_half_pct==30);

#if FOLLOW_RED_RETURN_GRACE
            if (drops_to_mix)
            {
                const rgb_reading_t late_red_return = {327U,250U,248U,635U};
                cycle(&owner,4530U+alignment_delay,false,false,&follow);
                cycle(&owner,4590U+alignment_delay,false,false,&follow);
                g_left=340U; g_right=197U;
                g_join_reading=&late_red_return; ++g_rgb_sequence;
                cycle(&owner,4650U+alignment_delay,false,false,&follow);
                assert(g_active && follow.join_active && follow.right_half_pct==30);
                g_left=235U; g_right=209U;
                cycle(&owner,4690U+alignment_delay,false,false,&follow);
                cycle(&owner,4730U+alignment_delay,false,false,&follow);
                assert(g_active && follow.join_active && follow.join_count==1U);
                assert(follow.red_return_grace_used);
                assert(follow.left_half_pct==-40 && follow.right_half_pct==30);

                if (handoff_case == 5U)
                {
                    g_join_reading=&red_drop_mix; ++g_rgb_sequence;
                    cycle(&owner,4770U+alignment_delay,false,false,&follow);
                    assert(!g_active && follow.reason==FOLLOW_JOIN_TIMEOUT);
                    continue;
                }

                ++g_rgb_sequence;
                cycle(&owner,4770U+alignment_delay,false,false,&follow);
                assert(g_active && !follow.join_active && follow.join_count==1U);
                g_join_reading=NULL; g_lane_colour=RGB_CLASS_YELLOW; ++g_rgb_sequence;
                cycle(&owner,4810U+alignment_delay,false,false,&follow);
                assert(g_active);
                continue;
            }
#endif
            g_join_reading=&red_drop; ++g_rgb_sequence;
            cycle(&owner,4530U+alignment_delay,false,false,&follow);
            ++g_rgb_sequence;
            cycle(&owner,4590U+alignment_delay,false,false,&follow);
            assert(g_active && !follow.join_active && follow.right_half_pct==30);
            cycle(&owner,4670U+alignment_delay,false,false,&follow);
            assert(g_active && follow.left_half_pct==-50 && follow.right_half_pct==50);
            yellow_ms=4710U;
#endif
        }
#endif
        g_join_reading=NULL; g_lane_colour=RGB_CLASS_YELLOW; ++g_rgb_sequence;
        cycle(&owner,yellow_ms+alignment_delay,false,false,&follow);
        assert(g_active && follow.right_half_pct>=0);
    }
}
#endif

#if (FOLLOW_RED_SECOND_SLOW) && FOLLOW_BLUE_RED_JOIN_REFINEMENT
/* Blue1's matched fade came after slowing for the confirmed outside edge. */
static void check_blue_red_join_refinement(void)
{
    black_follow_owner_t owner;
    black_follow_status_t follow;
    /* Blue1's matched fade came after slowing for the confirmed outside edge. */
    const rgb_reading_t late_blue_fade = {242U,264U,275U,582U};
    const rgb_reading_t bright_blue_fade = {250U,272U,283U,593U};
    const rgb_reading_t lost_blue = {245U,261U,265U,579U};
    begin(&owner,5000U,&follow);
    g_left=672U; g_right=203U;
    g_join_reading=&blue_near_edge; ++g_rgb_sequence;
    cycle(&owner,5110U,false,false,&follow);
    cycle(&owner,5150U,false,false,&follow);
    g_left=216U; g_right=230U;
    for (uint32_t now=5200U; now<=5500U; now+=50U)
    {
        ++g_rgb_sequence;
        cycle(&owner,now,false,false,&follow);
        assert(g_active && !follow.blue_exit_lead_used);
    }
    assert(follow.left_half_pct==-50 && follow.right_half_pct==50);
    g_join_reading=FOLLOW_BEND_CUE_REFINEMENT ? &bright_blue_fade : &late_blue_fade;
    ++g_rgb_sequence;
#if FOLLOW_BLUE_EXIT_EDGE_CONFIRM
    /* v0.76 Test 2: fading blue and a sustained left edge fired the old pivot. */
    const rgb_reading_t early_blue_fade = {231U,253U,268U,582U};
    g_left=860U; g_right=237U;
    g_join_reading=&early_blue_fade;
    cycle(&owner,5530U,false,false,&follow);
    cycle(&owner,5570U,false,false,&follow);
    cycle(&owner,5610U,false,false,&follow);
    cycle(&owner,5650U,false,false,&follow);
    assert(g_active && !follow.blue_exit_lead_used && follow.right_half_pct>0);

    g_left=225U; g_right=367U;
    cycle(&owner,5690U,false,false,&follow);
#else
    cycle(&owner,5530U,false,false,&follow);
#endif
#if FOLLOW_BLUE_EXIT_PIVOT
    assert(g_active && follow.blue_exit_lead_used &&
           follow.left_half_pct==expected_pivot_left && follow.right_half_pct==-10);
    assert(g_applied_left==expected_pivot_left/2 && g_applied_right==-5);
#else
    assert(g_active && follow.blue_exit_lead_used && follow.right_half_pct==0);
#endif
#if FOLLOW_BLUE_EXIT_PIVOT
    cycle(&owner,FOLLOW_BLUE_EXIT_EDGE_CONFIRM ? 5750U : 5590U,false,false,&follow);
    assert(g_active && follow.left_half_pct==expected_pivot_left && follow.right_half_pct==-10);
    cycle(&owner,FOLLOW_BLUE_EXIT_EDGE_CONFIRM ? 5790U : 5630U,false,false,&follow);
    assert(g_active && !follow.join_active &&
           follow.left_half_pct==-50 && follow.right_half_pct==0);
    assert(g_applied_left==-25 && g_applied_right==0);
#endif
    g_left=194U; g_right=509U;
    g_join_reading=&lost_blue; ++g_rgb_sequence;
    const uint32_t loss_ms = FOLLOW_BLUE_EXIT_EDGE_CONFIRM ? 5830U :
        (FOLLOW_BLUE_EXIT_PIVOT ? 5670U : 5570U);
    cycle(&owner,loss_ms,false,false,&follow);
    assert(g_active && follow.join_active && follow.turn_pct==25);
}
#endif

#if (FOLLOW_RED_SECOND_SLOW) && FOLLOW_BEND_CUE_REFINEMENT
static void check_bend_cue_refinement(void)
{
    black_follow_owner_t owner;
    black_follow_status_t follow;
    const rgb_reading_t weak_blue_fades[] = {
        {201U,225U,236U,539U}, {198U,222U,232U,535U}, {205U,230U,242U,546U},
#if FOLLOW_BLUE_DIAGONAL_EXIT
        {218U,240U,249U,558U}, {210U,236U,250U,556U},
#if FOLLOW_BLUE_EXIT_PIVOT
        {210U,233U,247U,551U}, /* v0.68 Test 3, before loss at the second bend. */
#if FOLLOW_BLUE_SECOND_LEFT_BOOST
        {205U,226U,236U,539U}, {206U,227U,237U,540U}, /* v0.74 Tests 2 and 3 */
#endif
#endif
#endif
    };
    for (size_t i=0U; i<sizeof(weak_blue_fades)/sizeof(weak_blue_fades[0]); ++i)
    {
        begin(&owner,6000U,&follow);
        g_left=203U; g_right=1045U;
        g_join_reading=&weak_blue_fades[i]; ++g_rgb_sequence;
        cycle(&owner,6110U,false,false,&follow);
        assert(g_active && !follow.blue_exit_lead_used && follow.turn_pct<0);

        g_left=677U; g_right=240U;
        g_join_reading=&blue_near_edge; ++g_rgb_sequence;
        cycle(&owner,6150U,false,false,&follow);
        g_left=FOLLOW_BLUE_SECOND_LEFT_BOOST && i == 6U ? 677U : 395U;
        cycle(&owner,6190U,false,false,&follow);
        assert(g_active && !follow.blue_exit_lead_used);

        g_left=203U; g_right=1045U;
#if FOLLOW_BLUE_SECOND_LEFT_BOOST
        if (i == 6U) {g_right=367U;} /* v0.74 Test 2, first right IR rise. */
#endif
        g_join_reading=&weak_blue_fades[i]; ++g_rgb_sequence;
        cycle(&owner,6250U,false,false,&follow);
        assert(g_active && follow.blue_exit_lead_used && follow.turn_pct==25);
#if FOLLOW_BLUE_EXIT_PIVOT
        const int8_t expected_second_blue_left = FOLLOW_BLUE_SECOND_LEFT_BOOST ? -70 : expected_pivot_left;
        assert(follow.left_half_pct==expected_second_blue_left && follow.right_half_pct==-10);
        assert(g_applied_left==expected_second_blue_left/2 && g_applied_right==-5);
#endif
        g_right=268U;
        cycle(&owner,6290U,false,false,&follow);
        assert(g_active && follow.turn_pct==25);
#if FOLLOW_BLUE_EXIT_PIVOT
        assert(follow.left_half_pct==expected_pivot_left);
#endif
#if FOLLOW_BLUE_DIAGONAL_EXIT
        const rgb_reading_t black_border = {260U,260U,252U,601U};
        g_join_reading=&black_border;
        g_left=213U; g_right=622U;
        for (uint32_t now=6310U; now<=6670U; now+=60U)
        {
            ++g_rgb_sequence;
            cycle(&owner,now,false,false,&follow);
            assert(g_active && follow.join_active && follow.turn_pct==25);
            assert(follow.left_half_pct==-50 && follow.right_half_pct==0);
        }
        cycle(&owner,6710U,false,false,&follow);
        assert(g_active && follow.join_active && follow.turn_pct<0);
#endif
    }
}
#endif

#if (FOLLOW_RED_SECOND_SLOW) && FOLLOW_BLUE_RED_LATE_CONFIRM
/* Each v0.69 capture reached its first red frame 400 ms into the join. */
static void check_blue_red_late_confirm(void)
{
    black_follow_owner_t owner;
    black_follow_status_t follow;
    /* Each v0.69 capture reached its first red frame 400 ms into the join. */
    const rgb_reading_t last_blend[] = {
        {313U,266U,271U,638U}, {276U,261U,271U,610U}, {290U,277U,284U,621U}
    };
    const rgb_reading_t first_red[] = {
        {335U,258U,258U,645U}, {334U,259U,261U,647U}, {330U,258U,259U,643U}
    };
    for (size_t trial=0U; trial<5U; ++trial)
    {
        size_t capture = trial < 3U ? trial : 2U;
        begin(&owner,7000U,&follow);
        g_left=1058U; g_right=251U;
        cycle(&owner,7110U,false,false,&follow);
        cycle(&owner,7150U,false,false,&follow);
        g_left=225U; g_right=215U;
        g_join_reading=&blue_fade; ++g_rgb_sequence;
        cycle(&owner,7190U,false,false,&follow);
        assert(g_active && follow.blue_exit_lead_used == !FOLLOW_BLUE_EXIT_EDGE_CONFIRM);
        cycle(&owner,7230U,false,false,&follow);

        g_join_reading=&blue_loss;
        for (uint32_t now=7290U; now<7690U; now+=50U)
        {
            if (now == 7590U)
            {
                g_join_reading=&last_blend[capture];
            }
            ++g_rgb_sequence;
            cycle(&owner,now,false,false,&follow);
            assert(g_active && follow.join_active);
        }
        g_join_reading=&first_red[capture]; ++g_rgb_sequence;
        g_left=capture == 2U ? 478U : 295U;
        cycle(&owner,7690U,false,false,&follow);
        assert(g_active && follow.join_active);
        g_left=290U;
        cycle(&owner,7750U,false,false,&follow);
        cycle(&owner,7790U,false,false,&follow);
        assert(g_active && follow.join_active);

        if (trial < 3U)
        {
            ++g_rgb_sequence;
            cycle(&owner,7810U,false,false,&follow);
            assert(g_active && !follow.join_active && owner.join.lane==RGB_CLASS_RED);
            assert(follow.red_handoff_armed);
        }
        else if (trial == 3U)
        {
            g_join_reading=&last_blend[capture]; ++g_rgb_sequence;
            cycle(&owner,7810U,false,false,&follow);
            assert(!g_active && follow.reason==FOLLOW_JOIN_TIMEOUT);
        }
        else
        {
            cycle(&owner,7850U,false,false,&follow);
            cycle(&owner,7890U,false,false,&follow);
            assert(g_active && follow.join_active);
#if FOLLOW_BLUE_RED_LATE_APPROACH
            cycle(&owner,7910U,false,false,&follow);
            assert(g_active && follow.join_active);
            cycle(&owner,7950U,false,false,&follow);
#else
            cycle(&owner,7910U,false,false,&follow);
#endif
            assert(!g_active && follow.reason==FOLLOW_JOIN_TIMEOUT);
        }
    }
}
#endif

#if (FOLLOW_RED_SECOND_SLOW) && FOLLOW_BLUE_RED_BLEND_GRACE
/* These powered runs saw red at or just after the cutoff. */
static void check_blue_red_blend_grace(void)
{
    black_follow_owner_t owner;
    black_follow_status_t follow;
    /* These powered runs saw red at or just after the cutoff. */
    const rgb_reading_t near_red[] = {
        {323U,266U,270U,648U}, {331U,262U,265U,651U}, /* v0.70 Tests 2 and 4 */
#if FOLLOW_BLUE_RED_BLEND_MARGIN
        {319U,261U,263U,639U}, {319U,265U,267U,641U}, /* v0.71 Tests 1 and 2 */
#endif
#if FOLLOW_BLUE_RED_LATE_APPROACH
        {324U,277U,281U,656U}, {328U,275U,278U,658U}, /* v0.77 Tests 2 and 3 */
#endif
    };
    const rgb_reading_t next_red[] = {
        {347U,264U,263U,660U}, {341U,258U,257U,651U},
#if FOLLOW_BLUE_RED_BLEND_MARGIN
        {333U,256U,255U,641U}, {334U,258U,257U,643U},
#endif
#if FOLLOW_BLUE_RED_LATE_APPROACH
        {348U,270U,269U,664U}, {345U,266U,265U,661U},
#endif
    };
    const size_t capture_count = sizeof(near_red)/sizeof(near_red[0]);
    for (size_t trial=0U; trial<=capture_count; ++trial)
    {
        bool confirm_red = trial < capture_count;
        size_t capture = confirm_red ? trial : 0U;
        begin(&owner,8000U,&follow);
        g_left=1058U; g_right=251U;
        cycle(&owner,8110U,false,false,&follow);
        cycle(&owner,8150U,false,false,&follow);
        g_left=225U; g_right=215U;
        g_join_reading=&blue_fade; ++g_rgb_sequence;
        cycle(&owner,8190U,false,false,&follow);
        assert(g_active && follow.blue_exit_lead_used == !FOLLOW_BLUE_EXIT_EDGE_CONFIRM);
        cycle(&owner,8230U,false,false,&follow);

        g_join_reading=&blue_loss;
        for (uint32_t now=8290U; now<8690U; now+=50U)
        {
            ++g_rgb_sequence;
            cycle(&owner,now,false,false,&follow);
            assert(g_active && follow.join_active);
        }
        g_join_reading=&near_red[capture]; ++g_rgb_sequence;
        g_left=capture == 2U ? 464U : 327U;
        g_right=211U;
        cycle(&owner,8690U,false,false,&follow);
        assert(g_active && follow.join_active && owner.join.lane==RGB_CLASS_BLUE);
        /* v0.71 Test 1 cleared the outer IR edge during the same RGB frame. */
        g_left=capture == 2U ? 273U : 465U;
        cycle(&owner,8750U,false,false,&follow);

        g_join_reading=confirm_red ? &next_red[capture] : &blue_loss;
        ++g_rgb_sequence;
        cycle(&owner,8790U,false,false,&follow);
        if (!confirm_red)
        {
            assert(!g_active && follow.reason==FOLLOW_JOIN_TIMEOUT);
            continue;
        }
        assert(g_active && follow.join_active && owner.join.lane==RGB_CLASS_BLUE);
        g_left=290U;
        cycle(&owner,8850U,false,false,&follow);
        assert(g_active && follow.join_active);
        ++g_rgb_sequence;
        /* Simulate an owner cycle that consumes the final frame 20 ms late. */
        uint32_t process_delay = FOLLOW_BLUE_RED_LATE_APPROACH && capture >= 4U ? 20U : 0U;
        g_rgb_age_ms=process_delay;
        cycle(&owner,8890U+process_delay,false,false,&follow);
        assert(g_active && !follow.join_active && owner.join.lane==RGB_CLASS_RED);
        assert(follow.red_handoff_armed);
    }
}
#endif

#if (FOLLOW_RED_SECOND_SLOW)
/* Track profile: home track. */
static void check_home_track_profile(void)
{
    black_follow_owner_t owner;
    black_follow_status_t follow;
    begin(&owner,1000U,&follow);
    assert(follow.left_half_pct==-50 && follow.right_half_pct==50);

    const rgb_reading_t solid_red = {334U,253U,252U,647U};
    g_lane_colour=RGB_CLASS_RED;
    g_left=702U; g_right=212U;
    g_join_reading=&solid_red; ++g_rgb_sequence;
    cycle(&owner,1110U,false,false,&follow);
    cycle(&owner,1150U,false,false,&follow);
    assert(g_active && follow.right_half_pct>0);
    g_left=270U; g_right=228U;
    g_join_reading=&fading_red; ++g_rgb_sequence;
    cycle(&owner,1190U,false,false,&follow);
    assert(g_active && follow.left_half_pct==first_left);
    assert(g_applied_left==first_left/2);
#if FOLLOW_RED_FIRST_BEND_TIGHTER
    assert(follow.right_half_pct==-40 && follow.red_first_pivot_used);
    assert(!follow.red_handoff_armed && !follow.red_second_pivot_used);
#else
    assert(follow.right_half_pct==0);
    assert(!follow.red_second_slow_used && !follow.red_second_pivot_used);
#endif
    cycle(&owner,1230U,false,false,&follow);
    g_left=229U; g_right=222U;
    g_join_reading=&red_return; ++g_rgb_sequence;
    cycle(&owner,1290U,false,false,&follow);
    assert(g_active && follow.left_half_pct==-40 && follow.right_half_pct==40);
    assert(follow.red_second_slow_used && !follow.red_second_pivot_used);

    g_join_reading=&red_second_fade; ++g_rgb_sequence;
    cycle(&owner,1330U,false,false,&follow);
    assert(g_active && follow.left_half_pct==-45 && follow.right_half_pct==second_reverse);
    assert(follow.red_second_pivot_used);
    cycle(&owner,1390U,false,false,&follow);
    cycle(&owner,1450U,false,false,&follow);
    cycle(&owner,1500U,false,false,&follow);
    assert(g_active && follow.right_half_pct==second_reverse && g_lease==20U);
    cycle(&owner,1510U,false,false,&follow);
#if FOLLOW_RED_SECOND_ARC_BOOST
    assert(g_active && follow.left_half_pct==-60 && follow.right_half_pct==0);
    assert(g_applied_left==-30 && g_applied_right==0);

    /* v0.74 Test 3 entered the forward arc with red briefly lost. */
    const rgb_reading_t second_arc_background = {319U,302U,287U,676U};
    g_join_reading=&second_arc_background; ++g_rgb_sequence;
    g_left=188U; g_right=276U;
    cycle(&owner,1520U,false,false,&follow);
    assert(g_active && follow.join_active && follow.left_half_pct==-60 && follow.right_half_pct==0);

    /* A strong right edge like v0.74 Test 2 must not get the extra drive. */
    g_right=533U;
    cycle(&owner,1530U,false,false,&follow);
#if FOLLOW_RED_CORNER_REFINE
    /* v0.86: after the first red pivot a right edge spins right instead. */
    assert(g_active && follow.turn_pct==25 && follow.right_half_pct<0);
#else
    assert(g_active && follow.left_half_pct==-40 && follow.right_half_pct==0);
#endif
    g_right=222U;
#else
    assert(g_active && follow.left_half_pct==-40 && follow.right_half_pct==0);
#endif

    g_join_reading=NULL; g_lane_colour=RGB_CLASS_YELLOW; ++g_rgb_sequence;
    cycle(&owner,1550U,false,false,&follow);
    assert(g_active && follow.left_half_pct==-50 && follow.right_half_pct==50);

    begin(&owner,2000U,&follow);
    g_left=224U; g_right=220U;
    g_join_reading=&g_trial_join; ++g_rgb_sequence;
    cycle(&owner,2110U,false,false,&follow);
    assert(g_active && follow.join_active && follow.left_half_pct==-50 && follow.right_half_pct==0);
    g_join_reading=&g_dark_black; ++g_rgb_sequence;
    cycle(&owner,2150U,false,false,&follow);
    assert(!g_active && follow.reason==FOLLOW_RGB);
#if FOLLOW_RED_BEND_AFTER_HANDOFF
    check_red_bend_after_handoff();
#endif
#if FOLLOW_BLUE_RED_JOIN_REFINEMENT
    check_blue_red_join_refinement();
#endif
#if FOLLOW_BEND_CUE_REFINEMENT
    check_bend_cue_refinement();
#endif
#if FOLLOW_BLUE_RED_LATE_CONFIRM
    check_blue_red_late_confirm();
#endif
#if FOLLOW_BLUE_RED_BLEND_GRACE
    check_blue_red_blend_grace();
#endif
    puts("25 percent drive and slowed red second bend PASS");
}
#endif

#if !(FOLLOW_RED_SECOND_SLOW) && (FOLLOW_TWO_TURN)
/* Track profile: two turn. */
static void check_two_turn_profile(void)
{
    black_follow_owner_t owner;
    black_follow_status_t follow;
    begin(&owner,1000U,&follow);
    assert(strcmp(black_follow_corner_name(follow.corner_phase),"SEEK")==0);
    g_right=650U;  /* Inside edge alone is ordinary correction. */
    for (uint32_t t=1110U; t<=1150U; t+=20U) {cycle(&owner,t,false,false,&follow);}
    assert(strcmp(black_follow_corner_name(follow.corner_phase),"SEEK")==0);
    g_left=600U; g_right=CENTRE_RIGHT;
    for (uint32_t t=1170U; t<=1210U; t+=20U) {cycle(&owner,t,false,false,&follow);}
    assert(strcmp(black_follow_corner_name(follow.corner_phase),"TURN_1")==0);
    assert(g_applied_left==-30 && g_applied_right==9);

    g_left=CENTRE_LEFT;
    for (uint32_t t=1230U; t<=1350U; t+=20U) {cycle(&owner,t,false,false,&follow);}
    /* Reusing one RGB sample cannot confirm the diagonal. */
    assert(strcmp(black_follow_corner_name(follow.corner_phase),"TURN_1")==0);
    ++g_rgb_sequence;
    cycle(&owner,1370U,false,false,&follow);
    assert(strcmp(black_follow_corner_name(follow.corner_phase),"DIAGONAL")==0);
    assert(g_applied_left==-30 && g_applied_right==30);
    cycle(&owner,1390U,false,false,&follow);
    assert(strcmp(black_follow_corner_name(follow.corner_phase),"DIAGONAL")==0);

    g_left=600U;
    for (uint32_t t=1410U; t<=1450U; t+=20U) {cycle(&owner,t,false,false,&follow);}
    assert(strcmp(black_follow_corner_name(follow.corner_phase),"TURN_2")==0);
    assert(g_applied_left==-30 && g_applied_right==9);
    g_left=CENTRE_LEFT;
    for (uint32_t t=1470U; t<=1590U; t+=20U) {cycle(&owner,t,false,false,&follow);}
    ++g_rgb_sequence;
    cycle(&owner,1610U,false,false,&follow);
    assert(strcmp(black_follow_corner_name(follow.corner_phase),"EXIT")==0);
    for (uint32_t t=1630U; t<=1970U; t+=20U) {cycle(&owner,t,false,false,&follow);}
    assert(!g_active && follow.reason==FOLLOW_COMPLETE);

    begin(&owner,3000U,&follow);
    g_left=680U;
    for (uint32_t t=3110U; t<=3490U; t+=20U) {cycle(&owner,t,false,false,&follow);}
    assert(g_active && follow.lease_limit==FOLLOW_CORNER_TIMEOUT &&
           strcmp(black_follow_corner_name(follow.corner_phase),"TURN_1")==0);
    for (uint32_t t=3510U; t<=3570U; t+=20U) {cycle(&owner,t,false,false,&follow);}
    assert(!g_active && follow.reason==FOLLOW_CORNER_TIMEOUT);

    begin(&owner,4000U,&follow);
    g_join_reading=&g_trial_join; ++g_rgb_sequence;
    cycle(&owner,4110U,false,false,&follow);
    assert(g_active && follow.join_active && follow.turn_pct==0);
    assert(strcmp(black_follow_corner_name(follow.corner_phase),"SEEK")==0);
    g_join_reading=&g_dark_black; ++g_rgb_sequence;
    cycle(&owner,4130U,false,false,&follow);
    assert(!g_active && follow.reason==FOLLOW_RGB);
    puts("30% clipped bend: independent turn cues, two RGB confirmations, exit and cutoff PASS");
}
#endif

#if !(FOLLOW_RED_SECOND_SLOW) && !(FOLLOW_TWO_TURN) && (FOLLOW_CLIPPED_LOOP) && FOLLOW_BLUE_BEND_RIGHT
/* v0.21 run 2 drove straight on this blue-bend sample. */
static void check_blue_bend_right(void)
{
    black_follow_owner_t owner;
    black_follow_status_t follow;
    /* v0.21 run 2 drove straight on this blue-bend sample. */
    const rgb_reading_t bend_exit = {242U, 262U, 274U, 580U};
    begin(&owner,1300U,&follow);
    g_left=217U; g_right=237U;
    cycle(&owner,1410U,false,false,&follow);
    assert(g_applied_left==-25 && g_applied_right==25);
    g_join_reading=&bend_exit; ++g_rgb_sequence;
    cycle(&owner,1430U,false,false,&follow);
    assert(g_active && follow.join_active && follow.turn_pct==25);
    assert(g_applied_left==-25 && g_applied_right==0);
    g_join_reading=NULL; ++g_rgb_sequence;
    cycle(&owner,1450U,false,false,&follow);
    assert(g_active && follow.join_active && g_applied_left==-25 && g_applied_right==25);
    ++g_rgb_sequence;
    cycle(&owner,1470U,false,false,&follow);
    assert(g_active && !follow.join_active && follow.join_count==0U);

    g_lane_colour=RGB_CLASS_YELLOW; ++g_rgb_sequence;
    cycle(&owner,1490U,false,false,&follow);
    g_join_reading=&bend_exit; ++g_rgb_sequence;
    cycle(&owner,1510U,false,false,&follow);
    assert(g_active && follow.join_active && follow.turn_pct==0);
    assert(g_applied_left==-25 && g_applied_right==25);
}
#endif

#if !(FOLLOW_RED_SECOND_SLOW) && !(FOLLOW_TWO_TURN) && (FOLLOW_CLIPPED_LOOP) && FOLLOW_EARLY_BEND
/* Blue run 2 saw the outside edge while RGB still matched blue. */
static void check_early_bend(void)
{
    black_follow_owner_t owner;
    black_follow_status_t follow;
    begin(&owner,1900U,&follow);
    /* Blue run 2 saw the outside edge while RGB still matched blue. */
    g_left=1059U; g_right=233U;
    cycle(&owner,2010U,false,false,&follow);
    assert(g_active && follow.ready && !follow.join_active);
#if FOLLOW_BLUE_STEER_RAMP && FOLLOW_BLUE_RAMP_START == 5
    assert(follow.turn_pct==5 && g_applied_left==-EXPECT_CLIPPED_DUTY(35) &&
           g_applied_right==EXPECT_CLIPPED_DUTY(25));
#elif FOLLOW_BLUE_STEER_RAMP
    assert(follow.turn_pct==8 && g_applied_left==-19 && g_applied_right==11);
#endif
    cycle(&owner,2030U,false,false,&follow);
#if FOLLOW_BLUE_STEER_RAMP && FOLLOW_BLUE_RAMP_START == 5
    assert(follow.turn_pct==6);
#elif FOLLOW_BLUE_STEER_RAMP
    assert(follow.turn_pct==9);
#endif
    for (uint32_t t=2050U; t<=2130U; t+=20U)
    {
        cycle(&owner,t,false,false,&follow);
    }
    assert(follow.turn_pct==14);
    assert(g_applied_left==-EXPECT_CLIPPED_DUTY(44) && g_applied_right==EXPECT_CLIPPED_DUTY(16));

    /* Yellow run 1 saw the same cue before the later loss of yellow. */
    g_lane_colour=RGB_CLASS_YELLOW; ++g_rgb_sequence;
    g_left=720U; g_right=217U;
    cycle(&owner,2150U,false,false,&follow);
    assert(g_active && follow.ready && !follow.join_active && follow.turn_pct==14);
    assert(g_applied_left==-EXPECT_CLIPPED_DUTY(44) && g_applied_right==EXPECT_CLIPPED_DUTY(16));
    g_left=255U; g_right=236U;
    cycle(&owner,2170U,false,false,&follow);
    assert(follow.turn_pct==0 && g_applied_left==-EXPECT_STRAIGHT &&
           g_applied_right==EXPECT_STRAIGHT);
}
#endif

#if !(FOLLOW_RED_SECOND_SLOW) && !(FOLLOW_TWO_TURN) && (FOLLOW_CLIPPED_LOOP) && FOLLOW_HOME_TRACK
static void check_home_track_lane_centres(void)
{
    black_follow_owner_t owner;
    black_follow_status_t follow;
    const rgb_reading_t new_blue_straight = {197U, 225U, 243U, 543U};
    begin(&owner,2100U,&follow);
    g_left=339U; g_right=313U; g_join_reading=&new_blue_straight;
    for (uint32_t t=2210U; t<=2810U; t+=20U)
    {
        g_rgb_age_ms=(t-2210U)%100U;
        if (g_rgb_age_ms==0U) {++g_rgb_sequence;}
        cycle(&owner,t,false,false,&follow);
        assert(g_active && follow.ready && !follow.join_active);
        assert(follow.turn_pct==0 && g_applied_left==-EXPECT_STRAIGHT &&
               g_applied_right==EXPECT_STRAIGHT);
    }
    g_rgb_age_ms=0U;
    g_join_reading=NULL; g_lane_colour=RGB_CLASS_YELLOW; ++g_rgb_sequence;
    g_left=245U; g_right=278U;
    cycle(&owner,2830U,false,false,&follow);
    assert(g_active && follow.ready && follow.right_norm==0U && follow.turn_pct==0);
    g_right=632U;
    cycle(&owner,2850U,false,false,&follow);
    assert(g_active && follow.turn_pct==-14 && g_applied_left==-EXPECT_CLIPPED_DUTY(28) &&
           g_applied_right==EXPECT_STRAIGHT);
    g_lane_colour=RGB_CLASS_RED; ++g_rgb_sequence;
    g_right=278U;
    cycle(&owner,2870U,false,false,&follow);
    assert(g_active && follow.right_norm==159U && follow.turn_pct==-1);
}
#endif

#if !(FOLLOW_RED_SECOND_SLOW) && !(FOLLOW_TWO_TURN) && (FOLLOW_CLIPPED_LOOP) && FOLLOW_YELLOW_EXIT_CUE
static void check_yellow_exit_cue(void)
{
    black_follow_owner_t owner;
    black_follow_status_t follow;
    const rgb_reading_t straight_join = {320U, 310U, 266U, 777U};
    const rgb_reading_t yellow_exit[] = {
        {345U, 327U, 283U, 724U}, {347U, 328U, 282U, 730U},
        {360U, 338U, 289U, 752U},
    };
    for (size_t i=0U; i<sizeof(yellow_exit)/sizeof(yellow_exit[0]); ++i)
    {
        begin(&owner,12000U,&follow);
        g_lane_colour=RGB_CLASS_YELLOW; ++g_rgb_sequence;
        g_left=245U; g_right=278U;
        cycle(&owner,12110U,false,false,&follow);
        g_join_reading=&straight_join; ++g_rgb_sequence;
        cycle(&owner,12130U,false,false,&follow);
        assert(g_active && follow.join_active && follow.turn_pct==0);
        g_join_reading=&yellow_exit[i]; ++g_rgb_sequence;
        g_right=650U;
        cycle(&owner,12150U,false,false,&follow);
        assert(g_active && follow.turn_pct==-14);
        g_right=278U;
        cycle(&owner,12170U,false,false,&follow);
        assert(g_active && follow.join_active && follow.turn_pct==14);
        assert(follow.left_half_pct==-53 && follow.right_half_pct==23);
        g_join_reading=&yellow_loss; ++g_rgb_sequence;
        cycle(&owner,12190U,false,false,&follow);
        assert(g_active && follow.turn_pct==25);
        assert(follow.left_half_pct==-55 && follow.right_half_pct==0);
    }
}
#endif

#if !(FOLLOW_RED_SECOND_SLOW) && !(FOLLOW_TWO_TURN) && (FOLLOW_CLIPPED_LOOP) && FOLLOW_YELLOW_LEAD_CUE
static void check_yellow_lead_cue(void)
{
    black_follow_owner_t owner;
    black_follow_status_t follow;
    const rgb_reading_t late_yellow = {368U, 336U, 274U, 781U};
    const rgb_reading_t first_unknown = {361U, 334U, 281U, 755U};
    const rgb_reading_t join_yellow = {460U, 408U, 310U, 994U};
    begin(&owner,13000U,&follow);
    g_lane_colour=RGB_CLASS_YELLOW; ++g_rgb_sequence;
    g_left=242U; g_right=219U; g_join_reading=&join_yellow;
    cycle(&owner,13110U,false,false,&follow);
    assert(g_active && follow.ready && follow.turn_pct==0);
    g_join_reading=&late_yellow; ++g_rgb_sequence;
    cycle(&owner,13130U,false,false,&follow);
    assert(g_active && follow.ready && !follow.join_active && follow.turn_pct==10);
    assert(follow.yellow_lead_used);
    assert(follow.left_half_pct==-49 && follow.right_half_pct==27);
    for (uint32_t t=13150U; t<=13330U; t+=20U) {cycle(&owner,t,false,false,&follow);}
    assert(g_active && follow.turn_pct==0 && follow.yellow_lead_used);
    g_join_reading=&first_unknown; ++g_rgb_sequence;
    cycle(&owner,13350U,false,false,&follow);
    assert(g_active && follow.join_active && follow.turn_pct==14);
}
#endif

#if !(FOLLOW_RED_SECOND_SLOW) && !(FOLLOW_TWO_TURN) && (FOLLOW_CLIPPED_LOOP) && FOLLOW_BLUE_REVERSAL_DAMPING
static void check_blue_reversal_damping(void)
{
    black_follow_owner_t owner;
    black_follow_status_t follow;
    begin(&owner,14000U,&follow);
    g_left=753U; g_right=294U;
    for (uint32_t t=14110U; t<=14230U; t+=20U) {cycle(&owner,t,false,false,&follow);}
    assert(g_active && follow.turn_pct==14);
    g_left=CENTRE_LEFT; g_right=CENTRE_RIGHT;
    cycle(&owner,14250U,false,false,&follow);
    assert(follow.turn_pct==0);
    g_left=250U; g_right=1302U;
    cycle(&owner,14270U,false,false,&follow);
    assert(follow.turn_pct==-5);
    assert(follow.blue_reversal_damped);
    for (uint32_t t=14290U; t<=14390U; t+=20U) {cycle(&owner,t,false,false,&follow);}
    assert(g_active && follow.ready && follow.turn_pct==-7);
    for (uint32_t t=14410U; t<=14670U; t+=20U) {cycle(&owner,t,false,false,&follow);}
    assert(g_active && follow.turn_pct==-14);
    g_left=217U; g_right=217U; g_join_reading=&blue_loss; ++g_rgb_sequence;
    cycle(&owner,14690U,false,false,&follow);
    assert(g_active && follow.join_active && follow.turn_pct==25);
}
#endif

#if !(FOLLOW_RED_SECOND_SLOW) && !(FOLLOW_TWO_TURN) && (FOLLOW_CLIPPED_LOOP) && FOLLOW_BLUE_STEER_WIDE_RANGE
static void check_blue_steer_wide_range(void)
{
    black_follow_owner_t owner;
    black_follow_status_t follow;
    begin(&owner,15000U,&follow);
    g_left=525U; g_right=281U;
    for (uint32_t t=15110U; t<=15530U; t+=20U) {cycle(&owner,t,false,false,&follow);}
    assert(g_active && follow.ready && follow.left_norm==1000U && follow.turn_pct==4);
    assert(follow.left_half_pct==-37 && follow.right_half_pct==28);
    g_left=234U; g_right=557U;
    for (uint32_t t=15550U; t<=15970U; t+=20U) {cycle(&owner,t,false,false,&follow);}
    assert(g_active && follow.ready && follow.right_norm==1000U && follow.turn_pct==-5);
    g_left=525U;
    cycle(&owner,15990U,false,false,&follow);
    assert(!g_active && follow.reason==FOLLOW_AMBIGUOUS);
}
#endif

#if !(FOLLOW_RED_SECOND_SLOW) && !(FOLLOW_TWO_TURN) && (FOLLOW_CLIPPED_LOOP) && FOLLOW_STOP_TRACE
static void check_stop_trace(void)
{
    black_follow_owner_t owner;
    black_follow_status_t follow;
    begin(&owner,16000U,&follow);
    g_left=216U; g_right=222U; g_join_reading=&g_trial_join;
    ++g_rgb_sequence;
    for (uint32_t t=16110U; t<=16590U; t+=20U) {cycle(&owner,t,false,false,&follow);}
    g_join_reading=NULL; ++g_rgb_sequence;
    g_left=230U; g_right=401U;
    cycle(&owner,16610U,false,false,&follow);
    assert(!g_active && follow.reason==FOLLOW_JOIN_TIMEOUT);
    assert(follow.stop_trace.valid && follow.stop_trace.captured_ms==16610U);
    assert(follow.stop_trace.rgb_result==RGB_CLASS_BLUE && follow.stop_trace.rgb_ms==16610U);
    assert(follow.stop_trace.join_active && follow.stop_trace.join_age_ms==500U);
    assert(follow.stop_trace.left_raw==230U && follow.stop_trace.right_raw==401U);
    g_lane_colour=RGB_CLASS_YELLOW; ++g_rgb_sequence;
    g_left=255U; g_right=278U;
    cycle(&owner,16630U,false,false,&follow);
    assert(follow.stop_trace.rgb_result==RGB_CLASS_BLUE && follow.stop_trace.right_raw==401U);
}
#endif

#if !(FOLLOW_RED_SECOND_SLOW) && !(FOLLOW_TWO_TURN) && (FOLLOW_CLIPPED_LOOP) && FOLLOW_BLUE_RED_HANDOFF
/* UNKNOWN RGB observations from the two hand-guided blue/red joins. */
static void check_blue_red_handoff(void)
{
    black_follow_owner_t owner;
    black_follow_status_t follow;
    /* UNKNOWN RGB observations from the two hand-guided blue/red joins. */
    begin(&owner,17000U,&follow);
    g_left=230U; g_right=220U;
    for (size_t i = 0U; i < sizeof(seam) / sizeof(seam[0]); ++i)
    {
        g_join_reading = &seam[i];
        ++g_rgb_sequence;
        cycle(&owner,17110U+(uint32_t)i*40U,false,false,&follow);
        assert(g_active && follow.join_active && follow.turn_pct==0);
        assert(follow.left_half_pct==-55 && follow.right_half_pct==55);
        assert(follow.blue_red_join_used);
    }
    g_join_reading=NULL; g_lane_colour=RGB_CLASS_RED; ++g_rgb_sequence;
    cycle(&owner,17390U,false,false,&follow);
    assert(g_active && follow.join_active);
    ++g_rgb_sequence;
    cycle(&owner,17410U,false,false,&follow);
    assert(g_active && !follow.join_active && follow.join_count==1U && follow.blue_red_join_used);

    /* Keep the turn response to the two previously missing blue readings. */
    const rgb_reading_t missed_bend[] = {{253U, 271U, 281U, 588U}, {261U, 271U, 274U, 595U}};
    begin(&owner,18000U,&follow);
    assert(!follow.blue_red_join_used);
    g_left=214U; g_right=240U;
    for (size_t i = 0U; i < sizeof(missed_bend) / sizeof(missed_bend[0]); ++i)
    {
        g_join_reading = &missed_bend[i];
        ++g_rgb_sequence;
        cycle(&owner,18110U+(uint32_t)i*20U,false,false,&follow);
        assert(g_active && follow.join_active && follow.turn_pct==25);
    }
    g_join_reading=&seam[0]; ++g_rgb_sequence;
    g_left=700U;
    cycle(&owner,18150U,false,false,&follow);
    assert(g_active && follow.join_active && follow.turn_pct==14 && !follow.blue_red_join_used);
}
#endif

#if !(FOLLOW_RED_SECOND_SLOW) && !(FOLLOW_TWO_TURN) && (FOLLOW_CLIPPED_LOOP) && FOLLOW_BLUE_SECOND_BOOST
/* Normal blue tracking still uses the ramp on a strong outside edge. */
static void check_blue_second_boost(void)
{
    black_follow_owner_t owner;
    black_follow_status_t follow;
    /* Normal blue tracking still uses the ramp on a strong outside edge. */
    begin(&owner,19000U,&follow);
    g_left=724U; g_right=226U;
    cycle(&owner,19110U,false,false,&follow);
    assert(g_active && follow.turn_pct==5 && !follow.blue_second_used);

    /* Blue1 lost blue, began rightward recovery, and then reacquired blue.
     * Use the recorded strong outside edge from Blue2 for the next cue. */
    const rgb_reading_t first_blue_loss = {246U, 265U, 276U, 583U};
    g_left=209U; g_right=223U;
    g_join_reading=&first_blue_loss; ++g_rgb_sequence;
    cycle(&owner,19200U,false,false,&follow);
    assert(g_active && follow.join_active && follow.turn_pct==25);
    const rgb_reading_t first_blue_reacquired = {214U, 237U, 252U, 554U};
    g_join_reading=&first_blue_reacquired; ++g_rgb_sequence;
    cycle(&owner,19240U,false,false,&follow);
    assert(g_active && follow.join_active && follow.turn_pct==8);
    ++g_rgb_sequence;
    cycle(&owner,19280U,false,false,&follow);
    assert(g_active && !follow.join_active && follow.turn_pct==0);
    g_left=724U; g_right=226U;
    cycle(&owner,19340U,false,false,&follow);
    assert(g_active && follow.turn_pct==12 && follow.blue_second_used);
    assert(follow.left_half_pct==-46 && follow.right_half_pct==19);
    g_left=213U; g_right=662U;
    cycle(&owner,19380U,false,false,&follow);
    assert(g_active && follow.turn_pct<0);
    g_left=CENTRE_LEFT; g_right=CENTRE_RIGHT;
    for (uint32_t now = 19420U; now < 19780U; now += 40U)
    {
        cycle(&owner,now,false,false,&follow);
        assert(g_active && follow.turn_pct==0);
    }
    g_left=724U; g_right=226U;
    cycle(&owner,19780U,false,false,&follow);
    assert(g_active && follow.turn_pct==5 && follow.blue_second_used);
}
#endif

#if !(FOLLOW_RED_SECOND_SLOW) && !(FOLLOW_TWO_TURN) && (FOLLOW_CLIPPED_LOOP) && FOLLOW_BLUE_OUTER_EDGE
/* Blue still matched when the outside IR rose in both powered runs. */
static void check_blue_outer_edge(void)
{
    black_follow_owner_t owner;
    black_follow_status_t follow;
    /* Blue still matched when the outside IR rose in both powered runs. */
    const rgb_reading_t outer_blue[] = {{202U, 227U, 242U, 540U}, {209U, 234U, 250U, 552U}};
    const uint16_t outside_ir[] = {565U, 1189U};
    const uint16_t inside_ir[] = {277U, 248U};
    for (size_t run = 0U; run < sizeof(outside_ir) / sizeof(outside_ir[0]); ++run)
    {
        begin(&owner,20000U,&follow);
        g_join_reading=&outer_blue[run]; ++g_rgb_sequence;
        g_left=outside_ir[run]; g_right=inside_ir[run];
        cycle(&owner,20110U,false,false,&follow);
        assert(g_active && follow.turn_pct<9 && !follow.blue_outer_used);
        cycle(&owner,20130U,false,false,&follow);
        assert(g_active && follow.turn_pct<9 && !follow.blue_outer_used);
        cycle(&owner,20150U,false,false,&follow);
        assert(g_active && follow.turn_pct==9);
        assert(follow.blue_outer_used);
        assert(follow.left_half_pct==-42 && follow.right_half_pct==23);

        g_left=241U; g_right=840U;
        cycle(&owner,20170U,false,false,&follow);
        assert(g_active && follow.turn_pct<0);
        g_left=outside_ir[run]; g_right=inside_ir[run];
        cycle(&owner,20190U,false,false,&follow);
        assert(g_active && follow.turn_pct<9);
    }
}
#endif

#if !(FOLLOW_RED_SECOND_SLOW) && !(FOLLOW_TWO_TURN) && (FOLLOW_CLIPPED_LOOP) && FOLLOW_BLUE_OUTER_RECOVERY
/* Blue2: the outside edge returned after a rightward recovery began. */
static void check_blue_outer_recovery(void)
{
    black_follow_owner_t owner;
    black_follow_status_t follow;
    /* Blue2: the outside edge returned after a rightward recovery began. */
    const rgb_reading_t blue2_loss = {241U, 261U, 272U, 575U};
    const rgb_reading_t blue2_outside = {228U, 248U, 257U, 558U};
    begin(&owner,21000U,&follow);
    g_left=211U; g_right=233U;
    g_join_reading=&blue2_loss; ++g_rgb_sequence;
    cycle(&owner,21110U,false,false,&follow);
    assert(g_active && follow.join_active && follow.turn_pct==25);
    g_left=1082U; g_right=259U;
    g_join_reading=&blue2_outside; ++g_rgb_sequence;
    cycle(&owner,21150U,false,false,&follow);
    assert(g_active && follow.join_active && follow.turn_pct==18);
    assert(follow.left_half_pct==-55 && follow.right_half_pct==17);
    assert(follow.blue_outer_recovery_used);
    g_join_reading=NULL; ++g_rgb_sequence;
    cycle(&owner,21190U,false,false,&follow);
    assert(g_active && follow.join_active && follow.turn_pct==14);
    ++g_rgb_sequence;
    cycle(&owner,21230U,false,false,&follow);
    assert(g_active && !follow.join_active && follow.turn_pct<14);
    assert(follow.blue_outer_recovery_used);

    /* A strong edge on the first lost sample, alone, is normal IR steering. */
    begin(&owner,22000U,&follow);
    g_left=1082U; g_right=259U;
    g_join_reading=&blue2_outside; ++g_rgb_sequence;
    cycle(&owner,22110U,false,false,&follow);
    assert(g_active && follow.join_active && follow.turn_pct==14);
    assert(!follow.blue_outer_recovery_used);
}
#endif

#if !(FOLLOW_RED_SECOND_SLOW) && !(FOLLOW_TWO_TURN) && (FOLLOW_CLIPPED_LOOP) && FOLLOW_BLUE_BEND_SLOW
/* A measured blue/red blend still crosses at the ordinary duty. */
static void check_blue_bend_slow(void)
{
    black_follow_owner_t owner;
    black_follow_status_t follow;
    const rgb_reading_t blue1_at_edge = {208U, 233U, 249U, 549U};
    begin(&owner,23000U,&follow);
    g_join_reading=&blue1_at_edge; ++g_rgb_sequence;
    g_left=1086U; g_right=249U;
    cycle(&owner,23110U,false,false,&follow);
    cycle(&owner,23130U,false,false,&follow);
    cycle(&owner,23150U,false,false,&follow);
    assert(g_active && follow.turn_pct==9 && !follow.blue_bend_slow_used);
    g_left=250U; g_right=246U;
    for (uint32_t now = 23170U; now < 23450U; now += 40U)
    {
        cycle(&owner,now,false,false,&follow);
        assert(g_active && !follow.join_active && follow.turn_pct==0);
        assert(follow.left_half_pct==-44 && follow.right_half_pct==44);
        assert(follow.blue_bend_slow_used);
    }
    cycle(&owner,23450U,false,false,&follow);
    assert(g_active && follow.left_half_pct==-55 && follow.right_half_pct==55);

    /* A measured blue/red blend still crosses at the ordinary duty. */
    g_left=1086U; g_right=249U;
    cycle(&owner,23470U,false,false,&follow);
    cycle(&owner,23510U,false,false,&follow);
    g_left=230U; g_right=220U;
    g_join_reading=&seam[0]; ++g_rgb_sequence;
    cycle(&owner,23530U,false,false,&follow);
    assert(g_active && follow.join_active && follow.turn_pct==0);
    assert(follow.left_half_pct==-55 && follow.right_half_pct==55);
}
#endif

#if !(FOLLOW_RED_SECOND_SLOW) && !(FOLLOW_TWO_TURN) && (FOLLOW_CLIPPED_LOOP) && FOLLOW_BLUE_CONFIRM_GRACE
static void check_blue_confirm_grace(void)
{
    black_follow_owner_t owner;
    black_follow_status_t follow;
    const rgb_reading_t blue3_loss = {270U, 277U, 277U, 607U};
    const rgb_reading_t blue3_return = {206U, 231U, 246U, 544U};
    for (unsigned int run = 0U; run < 2U; ++run)
    {
        begin(&owner,24000U,&follow);
        g_left=250U; g_right=219U;
        g_join_reading=&blue3_loss;
        for (uint32_t now = 24110U; now < 24510U; now += 40U)
        {
            g_rgb_age_ms=(now-24100U)%100U; ++g_rgb_sequence;
            cycle(&owner,now,false,false,&follow);
            assert(g_active && follow.join_active);
        }
        g_join_reading=&blue3_return; ++g_rgb_sequence;
        for (uint32_t now = 24510U; now <= 24600U; now += 30U)
        {
            g_rgb_age_ms=now-24500U;
            cycle(&owner,now,false,false,&follow);
            assert(g_active && follow.join_active && !follow.blue_late_confirmed);
        }
        follow_join_t pending = owner.join;
        rgb_snapshot_t too_late = owner.rgb;
        too_late.sampled_ms=24720U; ++too_late.sequence;
        assert(update_join(&pending, &too_late, 24720U, RGB_CLASS_BLUE, true)==FOLLOW_JOIN_TIMEOUT);
        g_rgb_age_ms=10U; ++g_rgb_sequence;
        if (run == 1U)
        {
            g_join_reading=&blue3_loss;
        }
        cycle(&owner,24610U,false,false,&follow);
        if (run == 0U)
        {
            assert(g_active && !follow.join_active && follow.join_count==0U && follow.blue_late_confirmed);
        }
        else
        {
            assert(!g_active && follow.reason==FOLLOW_JOIN_TIMEOUT && !follow.blue_late_confirmed);
        }
    }
}
#endif

#if !(FOLLOW_RED_SECOND_SLOW) && !(FOLLOW_TWO_TURN) && (FOLLOW_CLIPPED_LOOP) && FOLLOW_BEND_TRACE
static void check_bend_trace(void)
{
    black_follow_owner_t owner;
    black_follow_status_t follow;
    const rgb_reading_t approaching_bend = {211U, 237U, 252U, 556U};
    begin(&owner,25000U,&follow);
    g_left=760U; g_right=213U;
    g_join_reading=&approaching_bend; ++g_rgb_sequence;
    g_rgb_age_ms=40U;
    cycle(&owner,25110U,false,false,&follow);
    assert(g_active && follow.turn_pct==5);
    assert(follow.bend_trace.captured_ms==25110U && follow.bend_trace.ir_ms==25110U);
    assert(follow.bend_trace.rgb_ms==25070U && follow.bend_trace.rgb_result==RGB_CLASS_BLUE);
    assert(follow.bend_trace.left_raw==760U && follow.bend_trace.right_raw==213U);
    assert(follow.bend_trace.red==211U && follow.bend_trace.green==237U);
    assert(follow.bend_trace.blue==252U && follow.bend_trace.clear==556U);
}
#endif

#if !(FOLLOW_RED_SECOND_SLOW) && !(FOLLOW_TWO_TURN) && (FOLLOW_CLIPPED_LOOP) && FOLLOW_BLUE_EXIT_LEAD
/* Fresh outside-border confirmation precedes the recorded fading blue. */
static void check_blue_exit_lead(void)
{
    black_follow_owner_t owner;
    black_follow_status_t follow;
    const struct {
        rgb_reading_t reading;
        uint16_t left;
        uint16_t right;
    } blue_exit[] = {
        {{205U,228U,241U,540U}, 241U, 430U},
        {{202U,223U,232U,532U}, 224U, 1102U},
        {{208U,231U,243U,545U}, 270U, 268U},
        {{203U,224U,234U,536U}, 242U, 855U},
    };
    const rgb_reading_t after_blue_exit = {213U,231U,239U,544U};
    for (size_t run = 0U; run < sizeof(blue_exit)/sizeof(blue_exit[0]); ++run)
    {
        begin(&owner,26000U,&follow);
        g_left=blue_exit[run].left; g_right=blue_exit[run].right;
        g_join_reading=&blue_exit[run].reading; ++g_rgb_sequence;
        cycle(&owner,26110U,false,false,&follow);
        assert(g_active && follow.turn_pct<=0 && !follow.blue_exit_lead_used);

        /* Fresh outside-border confirmation precedes the recorded fading blue. */
        g_left=1113U; g_right=256U;
        g_join_reading=NULL; ++g_rgb_sequence;
        cycle(&owner,26150U,false,false,&follow);
        cycle(&owner,26190U,false,false,&follow);
        assert(g_active && follow.turn_pct==9);
        g_left=blue_exit[run].left; g_right=blue_exit[run].right;
        g_join_reading=&blue_exit[run].reading; ++g_rgb_sequence;
        cycle(&owner,26230U,false,false,&follow);
        assert(g_active && !follow.join_active && follow.turn_pct==expected_blue_lead &&
               follow.blue_exit_lead_used);
#if FOLLOW_BLUE_RED_BEND_CUES
        assert(follow.left_half_pct==-55 && follow.right_half_pct==0);
#elif FOLLOW_BLUE_EXIT_TURN == 22
        assert(follow.left_half_pct==-55 && follow.right_half_pct==12);
#elif FOLLOW_BLUE_EXIT_TURN == 18
        assert(follow.left_half_pct==-55 && follow.right_half_pct==17);
#else
        assert(follow.left_half_pct==-51 && follow.right_half_pct==25);
#endif

        g_left=197U; g_right=832U;
        g_join_reading=&after_blue_exit; ++g_rgb_sequence;
        cycle(&owner,26290U,false,false,&follow);
        assert(g_active && follow.join_active && follow.turn_pct==expected_blue_lead);

        if (run == 0U)
        {
            /* The measured blue/red seam cancels the bend cue immediately. */
            g_left=230U; g_right=220U;
            g_join_reading=&seam[0]; ++g_rgb_sequence;
            cycle(&owner,26330U,false,false,&follow);
            assert(g_active && follow.join_active && follow.turn_pct==0 && follow.blue_red_join_used);
        }
        else
        {
            cycle(&owner,26330U,false,false,&follow);
            assert(g_active && follow.turn_pct==expected_blue_lead);
            cycle(&owner,26380U,false,false,&follow);
#if FOLLOW_BLUE_EXIT_TURN == 22
            assert(g_active && follow.join_active && follow.turn_pct==expected_blue_lead);
#else
            assert(g_active && follow.join_active && follow.turn_pct==-14);
#endif
        }
    }
}
#endif

#if !(FOLLOW_RED_SECOND_SLOW) && !(FOLLOW_TWO_TURN) && (FOLLOW_CLIPPED_LOOP) && FOLLOW_BLUE_RED_CONFIRM_GRACE
static void check_blue_red_confirm_grace(void)
{
    black_follow_owner_t owner;
    black_follow_status_t follow;
    const rgb_reading_t red_approach[] = {{281U,266U,258U,619U}, {301U,278U,274U,629U}};
    const rgb_reading_t red_found[] = {{351U,280U,279U,665U}, {346U,258U,257U,658U}};
    for (size_t run = 0U; run < sizeof(red_approach)/sizeof(red_approach[0]); ++run)
    {
        begin(&owner,28000U,&follow);
        g_left=1023U; g_right=260U;
        g_join_reading=&before_exit; ++g_rgb_sequence;
        for (uint32_t now = 28110U; now <= 28230U; now += 40U)
        {
            cycle(&owner,now,false,false,&follow);
        }
        assert(g_active && follow.blue_exit_lead_used);

        g_left=224U; g_right=207U;
        g_join_reading=&off_blue;
        for (uint32_t now = 28270U; now < 28670U; now += 40U)
        {
            ++g_rgb_sequence;
            cycle(&owner,now,false,false,&follow);
            assert(g_active && follow.join_active);
        }
        g_join_reading=&red_approach[run]; ++g_rgb_sequence;
        cycle(&owner,28670U,false,false,&follow);
        cycle(&owner,28730U,false,false,&follow);
        cycle(&owner,28770U,false,false,&follow);
        assert(g_active && follow.join_active && follow.turn_pct==10 && follow.red_approach_grace_used);
        assert(follow.left_half_pct==-44 && follow.right_half_pct==22);

        follow_join_t pending_red = owner.join;
        rgb_snapshot_t late_rgb = owner.rgb;
        late_rgb.sampled_ms=28770U;
        assert(update_join(&pending_red, &late_rgb, 28770U, RGB_CLASS_BACKGROUND, false)==FOLLOW_JOIN_TIMEOUT);
        late_rgb.sampled_ms=29190U;
        assert(update_join(&pending_red, &late_rgb, 29190U, RGB_CLASS_RED, true)==FOLLOW_JOIN_TIMEOUT);

        g_join_reading=&red_found[run]; ++g_rgb_sequence;
        cycle(&owner,28830U,false,false,&follow);
        assert(g_active && follow.join_active && follow.turn_pct==0);
        assert(follow.left_half_pct==-33 && follow.right_half_pct==33);
        cycle(&owner,28890U,false,false,&follow);
        assert(g_active && follow.join_active);
        ++g_rgb_sequence;
        cycle(&owner,28930U,false,false,&follow);
        assert(g_active && !follow.join_active && follow.join_count==1U);
    }
}
#endif

#if !(FOLLOW_RED_SECOND_SLOW) && !(FOLLOW_TWO_TURN) && (FOLLOW_CLIPPED_LOOP) && FOLLOW_BLUE_EXIT_AFTER_RECOVERY
static void check_blue_exit_after_recovery(void)
{
    black_follow_owner_t owner;
    black_follow_status_t follow;
    const struct {
        rgb_reading_t reading;
        uint16_t left;
        uint16_t right;
    } fading_after_recovery[] = {
        {{218U,237U,246U,555U}, 218U, 1014U},
        {{211U,234U,246U,549U}, 233U, 253U},
        {{217U,239U,251U,556U}, 223U, 892U},
    };
    for (size_t run = 0U; run < sizeof(fading_after_recovery)/sizeof(fading_after_recovery[0]); ++run)
    {
        begin(&owner,29000U,&follow);
        g_left=fading_after_recovery[run].left; g_right=fading_after_recovery[run].right;
        g_join_reading=&fading_after_recovery[run].reading; ++g_rgb_sequence;
        cycle(&owner,29110U,false,false,&follow);
        assert(g_active && follow.turn_pct<=0 && !follow.blue_exit_lead_used);

        g_left=224U; g_right=220U;
        g_join_reading=&off_blue; ++g_rgb_sequence;
        cycle(&owner,29150U,false,false,&follow);
        assert(g_active && follow.join_active && follow.turn_pct==25);
        g_join_reading=NULL; ++g_rgb_sequence;
        cycle(&owner,29190U,false,false,&follow);
        assert(g_active && follow.join_active && follow.turn_pct==8);
        ++g_rgb_sequence;
        cycle(&owner,29230U,false,false,&follow);
        assert(g_active && !follow.join_active);

        g_left=fading_after_recovery[run].left; g_right=fading_after_recovery[run].right;
        g_join_reading=&fading_after_recovery[run].reading; ++g_rgb_sequence;
        cycle(&owner,29250U,false,false,&follow);
        assert(g_active && follow.turn_pct<=0 && !follow.blue_exit_lead_used);
        cycle(&owner,29270U,false,false,&follow);
        assert(g_active && !follow.join_active && follow.turn_pct==expected_blue_lead && follow.blue_exit_lead_used);
        assert(follow.blue_recovered_exit_used);
        assert(follow.left_half_pct==-55 && follow.right_half_pct==(FOLLOW_BLUE_RED_BEND_CUES ? 0 : 12));
    }
}
#endif

#if !(FOLLOW_RED_SECOND_SLOW) && !(FOLLOW_TWO_TURN) && (FOLLOW_CLIPPED_LOOP) && FOLLOW_RED_EXIT_LEAD
static void check_red_exit_lead(void)
{
    black_follow_owner_t owner;
    black_follow_status_t follow;
    const int8_t expected_red_inside_duty = FOLLOW_BLUE_RED_BEND_TRIAL ? 0 : 12;
    const rgb_reading_t red_on_border = {304U,242U,239U,607U};
    begin(&owner,30000U,&follow);
    g_lane_colour=RGB_CLASS_RED;
    g_join_reading=&fading_red; ++g_rgb_sequence;
    g_left=373U; g_right=220U;
    cycle(&owner,30110U,false,false,&follow);
    assert(g_active && follow.turn_pct==3 && !follow.red_exit_lead_used);

    g_join_reading=&solid_red; ++g_rgb_sequence;
    g_left=700U; g_right=232U;
    cycle(&owner,30150U,false,false,&follow);
    g_left=762U; g_right=220U;
    cycle(&owner,30190U,false,false,&follow);
    assert(g_active && follow.turn_pct==14);

    g_left=236U; g_right=202U;
    cycle(&owner,30230U,false,false,&follow);
    assert(g_active && follow.turn_pct==0);
    g_left=373U; g_right=220U;
    g_join_reading=&fading_red; ++g_rgb_sequence;
    cycle(&owner,30290U,false,false,&follow);
    assert(g_active && follow.turn_pct==expected_red_lead && follow.red_exit_lead_used);
    assert(follow.left_half_pct==-55 && follow.right_half_pct==expected_red_inside_duty);
    g_left=203U; g_right=729U;
    g_join_reading=&red_on_border; ++g_rgb_sequence;
    cycle(&owner,30350U,false,false,&follow);
    assert(g_active && follow.turn_pct==expected_red_lead);
    g_left=206U; g_right=206U;
    g_join_reading=&red_background; ++g_rgb_sequence;
    cycle(&owner,30410U,false,false,&follow);
    assert(g_active && follow.join_active && follow.turn_pct==25);
    g_join_reading=NULL; g_lane_colour=RGB_CLASS_YELLOW; ++g_rgb_sequence;
    cycle(&owner,30450U,false,false,&follow);
    assert(g_active && follow.join_active && follow.turn_pct==0);
    ++g_rgb_sequence;
    cycle(&owner,30490U,false,false,&follow);
    assert(g_active && !follow.join_active && follow.join_count==1U);
}
#endif

#if !(FOLLOW_RED_SECOND_SLOW) && !(FOLLOW_TWO_TURN) && (FOLLOW_CLIPPED_LOOP) && FOLLOW_RED_EXIT_EARLY
static void check_red_exit_early(void)
{
    black_follow_owner_t owner;
    black_follow_status_t follow;
    const rgb_reading_t early_red[] = {{324U,249U,248U,632U}, {337U,259U,258U,647U}};
    const rgb_reading_t stronger_red = {314U,241U,239U,622U};
    for (size_t run = 0U; run < sizeof(early_red)/sizeof(early_red[0]); ++run)
    {
        begin(&owner,31000U,&follow);
        g_lane_colour=RGB_CLASS_RED;
        g_join_reading=&early_red[run]; ++g_rgb_sequence;
        g_left=284U; g_right=238U;
        cycle(&owner,31110U,false,false,&follow);
        assert(g_active && !follow.red_exit_lead_used);
        g_left=754U; g_right=214U;
        g_join_reading=&solid_red; ++g_rgb_sequence;
        cycle(&owner,31150U,false,false,&follow);
        cycle(&owner,31190U,false,false,&follow);
        assert(g_active && follow.turn_pct==14);
        g_left=265U; g_right=245U;
        g_join_reading=FOLLOW_BLUE_RED_BEND_CUES ? &solid_red : &stronger_red; ++g_rgb_sequence;
        cycle(&owner,31230U,false,false,&follow);
        assert(g_active && !follow.red_exit_lead_used);
        g_left=284U; g_right=238U;
        g_join_reading=&early_red[run]; ++g_rgb_sequence;
        cycle(&owner,31290U,false,false,&follow);
        assert(g_active && follow.turn_pct==expected_red_lead && follow.red_exit_lead_used);
        g_left=201U; g_right=921U;
        g_join_reading=&red_background; ++g_rgb_sequence;
        for (uint32_t now = 31330U; now < 31650U; now += 40U)
        {
            cycle(&owner,now,false,false,&follow);
            assert(g_active && follow.join_active && follow.turn_pct==expected_red_lead);
        }
        cycle(&owner,31650U,false,false,&follow);
        assert(g_active && follow.join_active && follow.turn_pct==-14);
    }
}
#endif

#if !(FOLLOW_RED_SECOND_SLOW) && !(FOLLOW_TWO_TURN) && (FOLLOW_CLIPPED_LOOP) && FOLLOW_BLUE_RED_BEND_TRIAL
static void check_blue_red_bend_trial(void)
{
    black_follow_owner_t owner;
    black_follow_status_t follow;
    const rgb_reading_t faded_blue_outside = {211U,232U,243U,552U};
    const rgb_reading_t faded_blue_recovered = FOLLOW_BLUE_RED_BEND_CUES ?
        (rgb_reading_t){235U,257U,273U,584U} : (rgb_reading_t){217U,240U,254U,562U};
    begin(&owner,32000U,&follow);
    g_left=1058U; g_right=251U;
    cycle(&owner,32110U,false,false,&follow);
    cycle(&owner,32150U,false,false,&follow);
    g_left=310U; g_right=298U;
    g_join_reading=&faded_blue_outside; ++g_rgb_sequence;
    cycle(&owner,32190U,false,false,&follow);
    assert(g_active && follow.turn_pct==expected_blue_lead && follow.blue_exit_lead_used);
    assert(!follow.blue_recovered_exit_used);

    begin(&owner,33000U,&follow);
    g_left=224U; g_right=220U;
    g_join_reading=&off_blue; ++g_rgb_sequence;
    cycle(&owner,33110U,false,false,&follow);
    assert(g_active && follow.join_active && follow.turn_pct==25);
    g_join_reading=NULL; ++g_rgb_sequence;
    cycle(&owner,33150U,false,false,&follow);
    ++g_rgb_sequence;
    cycle(&owner,33190U,false,false,&follow);
    assert(g_active && !follow.join_active);
    g_left=362U; g_right=229U;
    g_join_reading=&faded_blue_recovered; ++g_rgb_sequence;
    cycle(&owner,33230U,false,false,&follow);
    assert(g_active && follow.turn_pct==expected_blue_lead && follow.blue_recovered_exit_used);
}
#endif

#if !(FOLLOW_RED_SECOND_SLOW) && !(FOLLOW_TWO_TURN) && (FOLLOW_CLIPPED_LOOP) && FOLLOW_BLUE_RED_BEND_CUES
static void check_blue_red_bend_cues(void)
{
    black_follow_owner_t owner;
    black_follow_status_t follow;
    const rgb_reading_t brighter_blue_exit[] = {{221U,244U,258U,567U}, {215U,236U,248U,556U}};
    const uint16_t falling_left[] = {334U, 441U};
    for (size_t run = 0U; run < sizeof(falling_left)/sizeof(falling_left[0]); ++run)
    {
        begin(&owner,34000U,&follow);
        g_left=1026U; g_right=257U;
        cycle(&owner,34110U,false,false,&follow);
        cycle(&owner,34150U,false,false,&follow);
        g_left=falling_left[run]; g_right=274U;
        g_join_reading=&brighter_blue_exit[run]; ++g_rgb_sequence;
        cycle(&owner,34190U,false,false,&follow);
        assert(g_active && follow.turn_pct==25 && follow.blue_exit_lead_used);
        assert(follow.left_half_pct==-55 && follow.right_half_pct==0);
    }

    const rgb_reading_t red_fade_before_edge = {351U,267U,266U,665U};
    const rgb_reading_t red_fade_on_edge = {336U,256U,255U,649U};
    begin(&owner,35000U,&follow);
    g_lane_colour=RGB_CLASS_RED;
    g_join_reading=&red_fade_before_edge; ++g_rgb_sequence;
    g_left=248U; g_right=211U;
    cycle(&owner,35110U,false,false,&follow);
    assert(g_active && !follow.red_exit_lead_used);
    g_left=569U; g_right=222U;
    g_join_reading=&red_fade_on_edge; ++g_rgb_sequence;
    cycle(&owner,35150U,false,false,&follow);
    assert(g_active && !follow.red_exit_lead_used);
    g_left=817U; g_right=196U;
    cycle(&owner,35190U,false,false,&follow);
    assert(g_active && follow.turn_pct==25 && follow.red_exit_lead_used);
    assert(follow.left_half_pct==-55 && follow.right_half_pct==0);
    g_left=248U; g_right=211U;
    g_join_reading=&solid_red; ++g_rgb_sequence;
    cycle(&owner,35230U,false,false,&follow);
    assert(g_active && follow.turn_pct==0);
    g_join_reading=&red_fade_before_edge; ++g_rgb_sequence;
    cycle(&owner,35270U,false,false,&follow);
    assert(g_active && follow.turn_pct==25);
}
#endif

#if !(FOLLOW_RED_SECOND_SLOW) && !(FOLLOW_TWO_TURN) && (FOLLOW_CLIPPED_LOOP) && FOLLOW_RED_SECOND_PIVOT
static void check_red_second_pivot(void)
{
    black_follow_owner_t owner;
    black_follow_status_t follow;
    const rgb_reading_t red_return[] = {{355U,269U,269U,668U}, {329U,249U,248U,641U}};
    const rgb_reading_t red_second_fade[] = {{350U,270U,270U,660U}, {352U,277U,277U,662U}};
    for (size_t run = 0U; run < sizeof(red_return)/sizeof(red_return[0]); ++run)
    {
        begin(&owner,36000U,&follow);
        g_lane_colour=RGB_CLASS_RED;
        g_left=702U; g_right=212U;
        g_join_reading=&solid_red; ++g_rgb_sequence;
        cycle(&owner,36110U,false,false,&follow);
        cycle(&owner,36150U,false,false,&follow);
        g_left=270U; g_right=228U;
        g_join_reading=&fading_red; ++g_rgb_sequence;
        cycle(&owner,36190U,false,false,&follow);
        assert(g_active && follow.left_half_pct==-55 && follow.right_half_pct==0);
        cycle(&owner,36230U,false,false,&follow);
        g_left=229U; g_right=222U;
        g_join_reading=&red_return[run]; ++g_rgb_sequence;
        cycle(&owner,36290U,false,false,&follow);
        assert(g_active && follow.turn_pct==0);

        g_left=242U; g_right=196U;
        g_join_reading=&red_second_fade[run]; ++g_rgb_sequence;
        cycle(&owner,36330U,false,false,&follow);
        assert(g_active && follow.left_half_pct==-55 && follow.right_half_pct==-22 &&
               follow.red_second_pivot_used);
        cycle(&owner,36390U,false,false,&follow);
        if (run == 1U)
        {
            g_join_reading=NULL; g_lane_colour=RGB_CLASS_YELLOW; ++g_rgb_sequence;
            cycle(&owner,36420U,false,false,&follow);
            assert(g_active && follow.right_half_pct==55);
            continue;
        }
        cycle(&owner,36450U,false,false,&follow);
        assert(g_active && follow.right_half_pct==-22 && g_lease==50U);
        cycle(&owner,36480U,false,false,&follow);
        assert(g_active && follow.right_half_pct==-22 && g_lease==20U);
        cycle(&owner,36490U,false,false,&follow);
        assert(g_active && follow.left_half_pct==-55 && follow.right_half_pct==0);

        g_join_reading=NULL; g_lane_colour=RGB_CLASS_YELLOW; ++g_rgb_sequence;
        cycle(&owner,36530U,false,false,&follow);
        assert(g_active && follow.right_half_pct==55);
    }
}
#endif

#if !(FOLLOW_RED_SECOND_SLOW) && !(FOLLOW_TWO_TURN) && (FOLLOW_CLIPPED_LOOP)
/* Track profile: clipped loop. */
static void check_clipped_loop_profile(void)
{
    black_follow_owner_t owner;
    black_follow_status_t follow;
    begin(&owner,1000U,&follow);
    unsigned int armed = g_arm_calls;
    g_left=FOLLOW_HOME_TRACK ? 406U : 400U;
    cycle(&owner,1110U,false,false,&follow);
#if FOLLOW_BLUE_STEER_WIDE_RANGE
    assert(follow.turn_pct==1 && g_applied_left==-EXPECT_STRAIGHT &&
           g_applied_right==EXPECT_CLIPPED_DUTY(48));
#else
    assert(follow.turn_pct==3 && g_applied_left==-EXPECT_STRAIGHT &&
           g_applied_right==EXPECT_CLIPPED_DUTY(44));
#endif
    g_left=FOLLOW_BLUE_STEER_WIDE_RANGE ? 700U : 500U;
    cycle(&owner,1130U,false,false,&follow);
#if FOLLOW_BLUE_STEER_RAMP && FOLLOW_BLUE_RAMP_START == 5
    assert(follow.turn_pct==5 && g_applied_left==-EXPECT_CLIPPED_DUTY(35) &&
           g_applied_right==EXPECT_CLIPPED_DUTY(25));
#if FOLLOW_DUTY_HALF_STEP
    assert(follow.left_half_pct==-38 && follow.right_half_pct==27);
#endif
#elif FOLLOW_BLUE_STEER_RAMP
    assert(follow.turn_pct==8 && g_applied_left==-19 && g_applied_right==11);
#elif FOLLOW_EARLY_BEND
    assert(follow.turn_pct==14 && g_applied_left==-22 && g_applied_right==8);
#else
    assert(follow.turn_pct==14 && g_applied_left==-25 && g_applied_right==14);
#endif
    g_left=CENTRE_LEFT; g_right=FOLLOW_BLUE_STEER_WIDE_RANGE ? 700U : 500U;
    cycle(&owner,1150U,false,false,&follow);
#if FOLLOW_BLUE_STEER_RAMP && FOLLOW_BLUE_RAMP_START == 5
    assert(follow.turn_pct==-5 && g_applied_left==-EXPECT_CLIPPED_DUTY(40) &&
           g_applied_right==EXPECT_STRAIGHT);
#elif FOLLOW_BLUE_STEER_RAMP
    assert(follow.turn_pct==-8 && g_applied_left==-18 && g_applied_right==25);
#else
    assert(follow.turn_pct==-14 && g_applied_left==-14 && g_applied_right==25);
#endif
    g_right=CENTRE_RIGHT;
    cycle(&owner,1170U,false,false,&follow);
    assert(follow.turn_pct==0 && g_applied_left==-EXPECT_STRAIGHT &&
           g_applied_right==EXPECT_STRAIGHT);

    g_join_reading=&g_trial_join; ++g_rgb_sequence;
    cycle(&owner,1190U,false,false,&follow);
    assert(g_active && follow.join_active && follow.join_count==0U);
    g_left=500U;
    cycle(&owner,1210U,false,false,&follow);
    assert(follow.turn_pct==14 && g_applied_left==-EXPECT_STRAIGHT &&
           g_applied_right==EXPECT_CLIPPED_DUTY(28));
    g_join_reading=NULL; g_lane_colour=RGB_CLASS_RED; ++g_rgb_sequence;
    g_left=255U; g_right=236U;
    cycle(&owner,1230U,false,false,&follow);
    assert(g_active && follow.join_active);
    ++g_rgb_sequence;
    cycle(&owner,1250U,false,false,&follow);
    assert(g_active && !follow.join_active && follow.join_count==1U && g_arm_calls==armed);
    g_left=500U; g_right=500U;
    cycle(&owner,1270U,false,false,&follow);
    assert(!g_active && follow.reason==FOLLOW_AMBIGUOUS);

#if FOLLOW_BLUE_BEND_RIGHT
    check_blue_bend_right();
#endif

#if FOLLOW_LOOP_RECOVERY_DIRECTION != 0
    const int8_t tight_left = turn_direction > 0 ? -EXPECT_STRAIGHT : 0;
    const int8_t tight_right = turn_direction > 0 ? 0 : EXPECT_STRAIGHT;
    const int8_t gentle_left = -EXPECT_CLIPPED_DUTY(turn_direction > 0 ? 38 : 22);
    const int8_t gentle_right = EXPECT_CLIPPED_DUTY(turn_direction > 0 ? 22 : 38);
    /* The first blue run lost the lane, saw blue once, then lost it again. */
    const rgb_reading_t blue_again = {208U, 233U, 247U, 546U};
    begin(&owner,1600U,&follow);
    g_left=188U; g_right=237U; g_join_reading=&blue_loss; ++g_rgb_sequence;
    cycle(&owner,1710U,false,false,&follow);
    assert(g_active && follow.join_active && follow.turn_pct==25*turn_direction);
    assert(g_applied_left==tight_left && g_applied_right==tight_right);
    g_left=314U; g_right=245U; g_join_reading=&blue_again; ++g_rgb_sequence;
    cycle(&owner,1730U,false,false,&follow);
    assert(g_active && follow.join_active && follow.turn_pct==8*turn_direction);
    assert(g_applied_left==gentle_left && g_applied_right==gentle_right);
    cycle(&owner,1750U,false,false,&follow);
    assert(g_active && follow.join_active && follow.turn_pct==8*turn_direction);
    ++g_rgb_sequence;
    cycle(&owner,1770U,false,false,&follow);
    assert(g_active && !follow.join_active && follow.turn_pct==0);
    assert(g_applied_left==-EXPECT_STRAIGHT && g_applied_right==EXPECT_STRAIGHT);

    /* A straight black seam should not start recovery. The measured yellow
     * bend's background reading should, with the same low IR signals. */
    const rgb_reading_t black_seam = {323U, 326U, 313U, 781U};
    g_lane_colour=RGB_CLASS_YELLOW; g_join_reading=NULL; ++g_rgb_sequence;
    g_left=232U; g_right=216U;
    cycle(&owner,1790U,false,false,&follow);
    g_join_reading=&black_seam; ++g_rgb_sequence;
    cycle(&owner,1810U,false,false,&follow);
    assert(g_active && follow.join_active && follow.turn_pct==0);
    g_join_reading=&yellow_loss; ++g_rgb_sequence;
    cycle(&owner,1830U,false,false,&follow);
    assert(g_active && follow.join_active && follow.turn_pct==25*turn_direction);
    assert(g_applied_left==tight_left && g_applied_right==tight_right);
    g_lane_colour=RGB_CLASS_RED; g_join_reading=NULL; ++g_rgb_sequence;
    cycle(&owner,1850U,false,false,&follow);
    assert(g_active && follow.join_active && follow.turn_pct==0);
    ++g_rgb_sequence;
    cycle(&owner,1870U,false,false,&follow);
    assert(g_active && !follow.join_active && follow.join_count==1U);

    /* The reverse blue run supplied a background cue and low IR readings. */
    const rgb_reading_t reverse_loss = {311U, 295U, 280U, 676U};
    g_lane_colour=RGB_CLASS_BLUE; ++g_rgb_sequence;
    cycle(&owner,1880U,false,false,&follow);
    g_join_reading=&reverse_loss; ++g_rgb_sequence;
    g_left=217U; g_right=217U;
    cycle(&owner,1890U,false,false,&follow);
    assert(g_active && follow.join_active && follow.turn_pct==25*turn_direction);
    assert(g_applied_left==tight_left && g_applied_right==tight_right);
#endif

#if FOLLOW_EARLY_BEND
    check_early_bend();
#endif

#if FOLLOW_HOME_TRACK
    check_home_track_lane_centres();
#endif

    begin(&owner,2000U,&follow);
    g_join_reading=&g_dark_black;
    cycle(&owner,2110U,false,false,&follow);
    assert(!g_active && follow.reason==FOLLOW_RGB);

    begin(&owner,3000U,&follow);
    g_right=FOLLOW_BLUE_STEER_WIDE_RANGE ? 700U : 500U;
    cycle(&owner,3110U,false,false,&follow);
#if FOLLOW_BLUE_STEER_RAMP && FOLLOW_BLUE_RAMP_START == 5
    assert(g_active && follow.turn_pct==-5);
#elif FOLLOW_BLUE_STEER_RAMP
    assert(g_active && follow.turn_pct==-8);
#else
    assert(g_active && follow.turn_pct==-14);
#endif
    cycle(&owner,3130U,false,true,&follow);
    assert(!g_active && follow.reason==FOLLOW_BUTTON);
#if FOLLOW_DUTY_HALF_STEP
    assert(follow.left_half_pct==0 && follow.right_half_pct==0);
#endif

    begin(&owner,4000U,&follow);
    for (uint32_t t=4110U; t<=10070U; t+=20U)
    {cycle(&owner,t,false,false,&follow);}
    assert(g_active && follow.elapsed_ms==5980U && g_lease==10U);
    cycle(&owner,10080U,false,false,&follow);
    assert(!g_active && follow.reason==FOLLOW_COMPLETE);
    cycle(&owner,10100U,false,false,&follow);
    assert(!g_active);

    begin(&owner,11000U,&follow);
    g_join_reading=&g_trial_join; ++g_rgb_sequence;
    for (uint32_t t=11110U; t<=11570U; t+=20U)
    {cycle(&owner,t,false,false,&follow);}
    cycle(&owner,11580U,false,false,&follow);
    assert(g_active && g_lease==20U && follow.lease_limit==FOLLOW_JOIN_TIMEOUT);
#if FOLLOW_BLUE_BEND_RIGHT
    assert(follow.turn_pct==25 && g_applied_left==-25 && g_applied_right==0);
#endif
#if FOLLOW_LOOP_RECOVERY_DIRECTION != 0
    assert(follow.turn_pct==25*turn_direction);
    assert(g_applied_left==tight_left && g_applied_right==tight_right);
#endif
    assert(follow.last_lease_ms==20U && !follow.lease_alarm);
    unsigned int drives=g_drive_calls;
    g_active=false; ++g_cutoff_count; /* Timer cutoff before the next owner cycle. */
    cycle(&owner,11600U,false,false,&follow);
    assert(!g_active && g_drive_calls==drives && follow.reason==FOLLOW_DRIVER);
    assert(follow.lease_alarm && follow.lease_limit==FOLLOW_JOIN_TIMEOUT);
    assert(follow.last_lease_ms==20U);
    g_join_reading=NULL;
    cycle(&owner,11620U,false,false,&follow);
    assert(follow.lease_alarm && follow.lease_limit==FOLLOW_JOIN_TIMEOUT);
    cycle(&owner,11650U,false,false,&follow);
    cycle(&owner,11660U,true,false,&follow);
    cycle(&owner,11700U,true,false,&follow);
    /* Fresh IR and command TTL both offer 100 ms; the IR bound wins the tie. */
    assert(g_active && !follow.lease_alarm && follow.lease_limit==FOLLOW_IR);
    g_active=false; /* Unexplained inactive output; no alarm evidence. */
    cycle(&owner,11720U,false,false,&follow);
    assert(!g_active && follow.reason==FOLLOW_DRIVER && !follow.lease_alarm);
#if FOLLOW_DUTY_LIMIT_PCT == 30
    /* The v0.27 follower can start while blue is matched and one edge is
     * in view, and that edge requests a gentle correction on the straight. */
    g_now=12000U;
    black_follow_owner_init(&owner);
    g_left=430U; g_right=CENTRE_RIGHT;
    cycle(&owner,12000U,false,false,&follow);
    cycle(&owner,12040U,false,false,&follow);
    assert(follow.ready && !g_active);
    cycle(&owner,12050U,true,false,&follow);
    cycle(&owner,12090U,true,false,&follow);
    assert(g_active && follow.phase==FOLLOW_RUNNING && follow.turn_pct==4);
    assert(g_applied_left==-29 && g_applied_right==24);
#endif
#if FOLLOW_YELLOW_EXIT_CUE
    check_yellow_exit_cue();
#endif
#if FOLLOW_YELLOW_LEAD_CUE
    check_yellow_lead_cue();
#endif
#if FOLLOW_BLUE_REVERSAL_DAMPING
    check_blue_reversal_damping();
#endif
#if FOLLOW_BLUE_STEER_WIDE_RANGE
    check_blue_steer_wide_range();
#endif
#if FOLLOW_STOP_TRACE
    check_stop_trace();
#endif
#if FOLLOW_BLUE_RED_HANDOFF
    check_blue_red_handoff();
#endif
#if FOLLOW_BLUE_SECOND_BOOST
    check_blue_second_boost();
#endif
#if FOLLOW_BLUE_OUTER_EDGE
    check_blue_outer_edge();
#endif
#if FOLLOW_BLUE_OUTER_RECOVERY
    check_blue_outer_recovery();
#endif
#if FOLLOW_BLUE_BEND_SLOW
    check_blue_bend_slow();
#endif
#if FOLLOW_BLUE_RED_JOIN_EXTEND
    {
        /* v0.82: exact test5 stop frame, then a cardboard and a blue/red mix frame. */
        follow_join_t join = {.started_ms=17800U, .lane=RGB_CLASS_BLUE,
            .candidate=RGB_CLASS_UNKNOWN, .active=true, .blue_red_extend=true};
        rgb_snapshot_t red_edge = {.red=321U, .green=258U, .blue=260U, .clear=638U,
            .sampled_ms=18200U, .sequence=182U, .device_id=0x44U, .ready=true,
            .has_sample=true, .valid=true, .status=RGB_OK};
        assert(join_window_ms(&join, &red_edge, RGB_CLASS_UNKNOWN) == FOLLOW_JOIN_MAX_MS + 250U);
        assert(update_join(&join, &red_edge, 18290U, RGB_CLASS_UNKNOWN, false) == FOLLOW_NONE);
        assert(update_join(&join, &red_edge, 18550U, RGB_CLASS_UNKNOWN, false) == FOLLOW_JOIN_TIMEOUT);
        rgb_snapshot_t cardboard = red_edge;
        cardboard.red=300U; cardboard.green=250U; cardboard.blue=240U;
        assert(join_window_ms(&join, &cardboard, RGB_CLASS_BACKGROUND) < FOLLOW_JOIN_MAX_MS + 250U);
        rgb_snapshot_t mix = red_edge;
        mix.red=277U; mix.green=276U; mix.blue=283U;
        assert(join_window_ms(&join, &mix, RGB_CLASS_UNKNOWN) < FOLLOW_JOIN_MAX_MS + 250U);
        join.blue_red_extend = false;
        assert(join_window_ms(&join, &red_edge, RGB_CLASS_UNKNOWN) < FOLLOW_JOIN_MAX_MS + 250U);
    }
#endif
#if FOLLOW_RED_PIVOT_INSIDE_RELEASE
    {
        /* v0.83 latch: v0.82 test2 had an expired cue window but a used exit pivot. */
        black_follow_owner_t latch_owner = {0};
        assert(!blue_red_extend_latch(&latch_owner, true, RGB_CLASS_BLUE, false));
        latch_owner.policy.status.blue_exit_lead_used = true;
        assert(blue_red_extend_latch(&latch_owner, true, RGB_CLASS_BLUE, false));
        assert(!blue_red_extend_latch(&latch_owner, false, RGB_CLASS_BLUE, false));
        assert(!blue_red_extend_latch(&latch_owner, true, RGB_CLASS_RED, false));
        assert(!blue_red_extend_latch(&latch_owner, true, RGB_CLASS_YELLOW, true));

        /* v0.83 release: v0.82 test1 frame at 3.48 s, 240 ms into the second
         * pivot, IR 189/794, red_clear_pivot never set. */
        black_follow_t pivot = {0};
        pivot.red_second_active = true;
        pivot.status.red_second_pivot_used = true;
        pivot.status.red_clear_pivot_used = false;
        pivot.red_second_pivot_ms = 3240U;
        pivot.status.left_norm = 0U;
        black_follow_input_t edge = {.now_ms=3480U, .ir_ms=3470U, .left_raw=189U,
            .right_raw=794U, .has_ir=true, .ir_valid=true, .red_surface=true};
        assert(black_follow_red_inside_release_due(&pivot, &edge));
        edge.right_raw = 520U;
        assert(!black_follow_red_inside_release_due(&pivot, &edge));
        edge.right_raw = 794U;
        pivot.status.left_norm = 400U;
        assert(!black_follow_red_inside_release_due(&pivot, &edge));
        pivot.status.left_norm = 0U;
        edge.now_ms = 3280U;
        assert(!black_follow_red_inside_release_due(&pivot, &edge));
        edge.now_ms = 3800U;
        assert(!black_follow_red_inside_release_due(&pivot, &edge));
        edge.now_ms = 3480U;
        pivot.status.red_second_release_used = true;
        assert(!black_follow_red_inside_release_due(&pivot, &edge));
    }
#endif
#if FOLLOW_RED_OUTSIDE_RECOVERY
    {
        /* v0.84: v0.83 run B frames. 2.90 s left IR on the outer border,
         * 3.00 s right IR on the same border, 3.10 s both on cardboard. */
        black_follow_t out = {0};
        out.status.red_first_pivot_used = true;
        black_follow_input_t in = {.red_surface=true, .rgb_target_matched=true};
        in.now_ms = 2900U; out.status.left_norm = 559U; out.status.right_norm = 0U;
        assert(!black_follow_red_outside_update(&out, &in, false));
        in.now_ms = 3000U; out.status.left_norm = 0U; out.status.right_norm = 1000U;
        assert(black_follow_red_outside_update(&out, &in, false));
        assert(out.status.red_outside_used);
        in.now_ms = 3100U; out.status.right_norm = 0U;
        in.rgb_target_matched = false; in.rgb_join_active = true;
        assert(black_follow_red_outside_update(&out, &in, false));
        in.now_ms = 3200U; in.rgb_target_matched = true; in.rgb_join_active = false;
        assert(!black_follow_red_outside_update(&out, &in, false));

        /* Held at most 350 ms when red does not return. */
        black_follow_t held = {0};
        held.status.red_first_pivot_used = true;
        black_follow_input_t lost = {.red_surface=true};
        lost.now_ms = 3080U; held.status.left_norm = 942U;
        assert(!black_follow_red_outside_update(&held, &lost, false));
        lost.now_ms = 3320U; held.status.left_norm = 0U; held.status.right_norm = 723U;
        assert(black_follow_red_outside_update(&held, &lost, false));
        lost.now_ms = 3600U; held.status.right_norm = 0U;
        assert(black_follow_red_outside_update(&held, &lost, false));
        lost.now_ms = 3670U;
        assert(!black_follow_red_outside_update(&held, &lost, false));

        /* Not triggered: right edge alone, too late, during a pivot,
         * before the first red pivot, or once the final arc has begun. */
        black_follow_t neg = {0};
        neg.status.red_first_pivot_used = true;
        black_follow_input_t n = {.red_surface=true, .rgb_target_matched=true};
        n.now_ms = 1000U; neg.status.right_norm = 1000U;
        assert(!black_follow_red_outside_update(&neg, &n, false));
        n.now_ms = 1100U; neg.status.right_norm = 0U; neg.status.left_norm = 800U;
        assert(!black_follow_red_outside_update(&neg, &n, false));
        n.now_ms = 1400U; neg.status.left_norm = 0U; neg.status.right_norm = 800U;
        assert(!black_follow_red_outside_update(&neg, &n, false));
        n.now_ms = 1500U; neg.status.left_norm = 800U; neg.status.right_norm = 0U;
        assert(!black_follow_red_outside_update(&neg, &n, false));
        n.now_ms = 1600U; neg.status.left_norm = 0U; neg.status.right_norm = 800U;
        assert(!black_follow_red_outside_update(&neg, &n, true));
        black_follow_t early = {0};
        black_follow_input_t e = {.red_surface=true};
        e.now_ms = 1000U; early.status.left_norm = 800U;
        assert(!black_follow_red_outside_update(&early, &e, false));
        e.now_ms = 1100U; early.status.left_norm = 0U; early.status.right_norm = 800U;
        assert(!black_follow_red_outside_update(&early, &e, false));
        black_follow_t final_arc = {0};
        final_arc.status.red_first_pivot_used = true;
        final_arc.status.red_exit_turn_used = true;
        e.now_ms = 1000U; final_arc.status.left_norm = 800U;
        assert(!black_follow_red_outside_update(&final_arc, &e, false));
        e.now_ms = 1100U; final_arc.status.left_norm = 0U; final_arc.status.right_norm = 800U;
        assert(!black_follow_red_outside_update(&final_arc, &e, false));
    }
#endif
#if FOLLOW_RED_RIGHT_EDGE_OUTSIDE
    {
        /* v0.85: v0.84 frames. test1 4.02 s IR 203/549 (norm 0/1000), test4
         * 3.86 s 194/403 (norm 0/633), test2 2.92 s 219/1098, all after the
         * red handoff alignment, all answered with a left turn on the car. */
        black_follow_t r = {0};
        r.status.red_alignment_used = true;
        r.status.red_first_pivot_used = true;
        black_follow_input_t ri = {.red_surface=true, .now_ms=2920U};
        r.status.left_norm = 0U; r.status.right_norm = 1000U;
        assert(black_follow_red_right_edge_outside(&r, &ri));
        r.status.red_exit_turn_used = true; r.red_exit_turn_started_ms = 3760U;
        ri.now_ms = 3860U; r.status.right_norm = 633U;
        assert(black_follow_red_right_edge_outside(&r, &ri));
        r.red_exit_turn_started_ms = 3980U; ri.now_ms = 4020U; r.status.right_norm = 1000U;
        assert(black_follow_red_right_edge_outside(&r, &ri));
        /* Normal steering again on the red straight after the final arc. */
        ri.now_ms = 4479U;
        assert(black_follow_red_right_edge_outside(&r, &ri));
        ri.now_ms = 4480U;
        assert(!black_follow_red_right_edge_outside(&r, &ri));
        /* Not for weak edges, a left edge, other colours or before the handoff. */
        r.status.red_exit_turn_used = false; ri.now_ms = 3000U;
        r.status.right_norm = 360U;
        assert(!black_follow_red_right_edge_outside(&r, &ri));
        r.status.right_norm = 800U; r.status.left_norm = 300U;
        assert(!black_follow_red_right_edge_outside(&r, &ri));
        r.status.left_norm = 0U; ri.red_surface = false;
        assert(!black_follow_red_right_edge_outside(&r, &ri));
        ri.red_surface = true; r.status.red_alignment_used = false;
        r.status.red_first_pivot_used = false;
        assert(!black_follow_red_right_edge_outside(&r, &ri));
    }
#endif
#if FOLLOW_RED_CORNER_REFINE
    {
        /* v0.86 (1): v0.85 test4 2.46-2.72 s, right IR 451..1113 after the
         * handoff but before the first red pivot: normal steering, no spin. */
        black_follow_t h = {0};
        h.status.red_alignment_used = true;
        h.status.left_norm = 0U; h.status.right_norm = 1000U;
        black_follow_input_t hi = {.red_surface=true, .now_ms=2520U};
        assert(!black_follow_red_right_edge_outside(&h, &hi));
        h.status.red_first_pivot_used = true;
        assert(black_follow_red_right_edge_outside(&h, &hi));

        /* v0.86 (2): v0.85 test1 red rejoin. RED on a red-lane join gets
         * 150 ms more; UNKNOWN/BACKGROUND and other lanes do not. */
        follow_join_t rj = {.started_ms=3040U, .lane=RGB_CLASS_RED,
            .candidate=RGB_CLASS_UNKNOWN, .active=true};
        rgb_snapshot_t red_back = {.red=322U, .green=252U, .blue=249U, .clear=633U,
            .sampled_ms=3480U, .sequence=348U, .device_id=0x44U, .ready=true,
            .has_sample=true, .valid=true, .status=RGB_OK};
        assert(join_window_ms(&rj, &red_back, RGB_CLASS_RED) == FOLLOW_JOIN_MAX_MS + 150U);
        assert(update_join(&rj, &red_back, 3540U, RGB_CLASS_RED, true) == FOLLOW_NONE);
        assert(join_window_ms(&rj, &red_back, RGB_CLASS_UNKNOWN) == FOLLOW_JOIN_MAX_MS);
        assert(join_window_ms(&rj, &red_back, RGB_CLASS_BACKGROUND) == FOLLOW_JOIN_MAX_MS);
        follow_join_t yj = rj;
        yj.lane = RGB_CLASS_YELLOW;
        assert(join_window_ms(&yj, &red_back, RGB_CLASS_RED) == FOLLOW_JOIN_MAX_MS);

        /* v0.86 (3): v0.85 test2 final arc. At 300 ms the left IR read the
         * outer border (606, norm 1000): keep holding to 530 ms. */
        assert(black_follow_red_final_hold_ok(100U, true, true, 300U));
        assert(!black_follow_red_final_hold_ok(100U, true, true, 380U));
        assert(black_follow_red_final_hold_ok(1000U, true, true, 300U));
        assert(black_follow_red_final_hold_ok(1000U, true, true, 529U));
        assert(!black_follow_red_final_hold_ok(1000U, true, true, 530U));
        assert(!black_follow_red_final_hold_ok(1000U, false, true, 300U));
        assert(!black_follow_red_final_hold_ok(1000U, true, false, 300U));
    }
#endif
#if FOLLOW_RED_SPIN_HOLD
    {
        /* v0.87 fix: feed the join window a REAL classifier result. The v0.86
         * test4 stop frame 315/253/254/631 is packed UNKNOWN/NO_MATCH (0x17). */
        follow_join_t bj = {.started_ms=1880U, .lane=RGB_CLASS_BLUE,
            .candidate=RGB_CLASS_UNKNOWN, .active=true, .blue_red_extend=true};
        rgb_snapshot_t stop_frame = {.red=315U, .green=253U, .blue=254U, .clear=631U,
            .sampled_ms=2280U, .sequence=228U, .device_id=0x44U, .ready=true,
            .has_sample=true, .valid=true, .status=RGB_OK};
        rgb_classification_t packed = rgb_classify(rgb_default_calibration(), &stop_frame,
                                                   2290U, RGB_GAIN, RGB_INTEGRATION_US);
        assert(rgb_classification_class(packed) == RGB_CLASS_UNKNOWN);
        assert(rgb_classification_reason(packed) == RGB_CLASS_NO_MATCH);
        assert(packed != RGB_CLASS_UNKNOWN);
        assert(join_window_ms(&bj, &stop_frame, packed) == FOLLOW_JOIN_MAX_MS + 250U);
        assert(update_join(&bj, &stop_frame, 2370U, packed, false) == FOLLOW_NONE);

        /* v0.87 hold: v0.86 test2 3.26 s, spin starts on a still-RED frame. */
        black_follow_t hs = {0};
        hs.status.red_first_pivot_used = true;
        black_follow_input_t hin = {.red_surface=true, .rgb_target_matched=true,
            .rgb_sequence=326U, .now_ms=3260U};
        black_follow_red_spin_start(&hs, &hin);
        hin.now_ms = 3300U;
        assert(black_follow_red_outside_update(&hs, &hin, true));      /* same RED frame */
        hin.now_ms = 3320U; hin.rgb_sequence = 332U; hin.rgb_target_matched = false;
        assert(black_follow_red_outside_update(&hs, &hin, true));      /* new UNKNOWN */
        hin.now_ms = 3420U; hin.rgb_sequence = 342U; hin.rgb_target_matched = true;
        assert(!black_follow_red_outside_update(&hs, &hin, true));     /* new RED: stop */

        /* Bounded by 350 ms, and ended by the left IR on the border. */
        black_follow_t ht = {0};
        ht.status.red_first_pivot_used = true;
        black_follow_input_t tin = {.red_surface=true, .rgb_sequence=10U, .now_ms=1000U};
        black_follow_red_spin_start(&ht, &tin);
        tin.now_ms = 1349U;
        assert(black_follow_red_outside_update(&ht, &tin, true));
        tin.now_ms = 1350U;
        assert(!black_follow_red_outside_update(&ht, &tin, true));
        black_follow_red_spin_start(&ht, &tin);
        ht.status.left_norm = 600U; tin.now_ms = 1400U;
        assert(!black_follow_red_outside_update(&ht, &tin, true));

        /* Kept during the first 500 ms of the final arc, not after. */
        black_follow_t hf = {0};
        hf.status.red_first_pivot_used = true;
        hf.status.red_exit_turn_used = true;
        hf.red_exit_turn_started_ms = 3800U;
        black_follow_input_t fin = {.red_surface=true, .rgb_sequence=40U, .now_ms=4100U};
        black_follow_red_spin_start(&hf, &fin);
        fin.now_ms = 4299U;
        assert(black_follow_red_outside_update(&hf, &fin, true));
        fin.now_ms = 4300U;
        assert(!black_follow_red_outside_update(&hf, &fin, true));
    }
#endif
#if FOLLOW_BLUE_CONFIRM_GRACE
    check_blue_confirm_grace();
#endif
#if FOLLOW_BEND_TRACE
    check_bend_trace();
#endif
#if FOLLOW_BLUE_EXIT_LEAD
    check_blue_exit_lead();
#endif
#if FOLLOW_BLUE_EXIT_TURN == 22
    /* v0.42: fading blue appears while the outside edge is still strong. */
    begin(&owner,27000U,&follow);
    g_left=1023U; g_right=260U;
    g_join_reading=&before_exit; ++g_rgb_sequence;
    for (uint32_t now = 27110U; now < 27230U; now += 40U)
    {
        cycle(&owner,now,false,false,&follow);
        assert(g_active && !follow.blue_exit_lead_used);
    }
    cycle(&owner,27230U,false,false,&follow);
    assert(g_active && !follow.join_active && follow.turn_pct==expected_blue_lead && follow.blue_exit_lead_used);
    assert(follow.left_half_pct==-55 && follow.right_half_pct==(FOLLOW_BLUE_RED_BEND_CUES ? 0 : 12));
    g_left=227U; g_right=1041U;
    g_join_reading=&off_blue;
    for (uint32_t now = 27270U; now < 27590U; now += 40U)
    {
        ++g_rgb_sequence;
        cycle(&owner,now,false,false,&follow);
        assert(g_active && follow.join_active && follow.turn_pct==expected_blue_lead);
    }
    cycle(&owner,27590U,false,false,&follow);
    assert(g_active && follow.join_active && follow.turn_pct==-14);
#endif
#if FOLLOW_BLUE_RED_CONFIRM_GRACE
    check_blue_red_confirm_grace();
#endif
#if FOLLOW_BLUE_EXIT_AFTER_RECOVERY
    check_blue_exit_after_recovery();
#endif
#if FOLLOW_RED_EXIT_LEAD
    check_red_exit_lead();
#endif
#if FOLLOW_RED_EXIT_EARLY
    check_red_exit_early();
#endif
#if FOLLOW_BLUE_RED_BEND_TRIAL
    check_blue_red_bend_trial();
#endif
#if FOLLOW_BLUE_RED_BEND_CUES
    check_blue_red_bend_cues();
#endif
#if FOLLOW_RED_SECOND_PIVOT
    check_red_second_pivot();
#endif
    puts("Clipped loop: T14 steering, joins, guards, cutoff and alarm attribution PASS");
}
#endif

#if !(FOLLOW_RED_SECOND_SLOW) && !(FOLLOW_TWO_TURN) && !(FOLLOW_CLIPPED_LOOP) && (FOLLOW_CORNER_DIRECTION != 0)
/* Track profile: corner direction. */
static void check_corner_direction_profile(void)
{
    black_follow_owner_t owner;
    black_follow_status_t follow;
    const int8_t pivot = FOLLOW_CORNER_DIRECTION == 1 ? -25 : 25;
    begin(&owner,1000U,&follow);
    unsigned int armed = g_arm_calls;
    g_left=500U; g_right=500U;
    cycle(&owner,1100U,false,false,&follow);
    assert(g_active && follow.corner_phase==FOLLOW_CORNER_SEEK);
    assert(g_applied_left==-12 && g_applied_right==12);
    g_right=CENTRE_RIGHT;
    cycle(&owner,1120U,false,false,&follow);
    g_lane_colour=RGB_CLASS_YELLOW;
    g_right=500U;
    cycle(&owner,1140U,false,false,&follow);
    cycle(&owner,1160U,false,false,&follow);
    assert(follow.corner_phase==FOLLOW_CORNER_SEEK);
    cycle(&owner,1180U,false,false,&follow);
    assert(follow.corner_phase==FOLLOW_CORNER_TURN);
    assert(g_applied_left==pivot && g_applied_right==pivot && g_arm_calls==armed);
    g_join_reading=&g_dark_black;
    for (uint32_t t=1200U; t<1400U; t+=20U)
    {cycle(&owner,t,false,false,&follow);}
    assert(g_active && follow.corner_phase==FOLLOW_CORNER_TURN);
    g_left=CENTRE_LEFT; g_right=CENTRE_RIGHT;
    g_join_reading=NULL; g_lane_colour=RGB_CLASS_RED; ++g_rgb_sequence;
    for (uint32_t t=1400U; t<=1500U; t+=20U)
    {cycle(&owner,t,false,false,&follow);}
    assert(g_active && follow.corner_phase==FOLLOW_CORNER_TURN);
    ++g_rgb_sequence;
    cycle(&owner,1520U,false,false,&follow);
    assert(g_active && follow.corner_phase==FOLLOW_CORNER_EXIT);
    assert(g_applied_left==-25 && g_applied_right==25);
    for (uint32_t t=1540U; t<=1880U; t+=20U)
    {cycle(&owner,t,false,false,&follow);}
    assert(!g_active && follow.reason==FOLLOW_COMPLETE);
    assert(follow.corner_phase==FOLLOW_CORNER_EXIT && g_arm_calls==armed);

    begin(&owner,2000U,&follow);
    for (uint32_t t=2110U; t<=2850U; t+=20U)
    {cycle(&owner,t,false,false,&follow);}
    assert(!g_active && follow.reason==FOLLOW_CORNER_NOT_SEEN);

    begin(&owner,3000U,&follow);
    g_left=500U; g_right=500U;
    for (uint32_t t=3100U; t<=3960U; t+=20U)
    {cycle(&owner,t,false,false,&follow);}
    cycle(&owner,3970U,false,false,&follow);
    assert(g_active && follow.corner_phase==FOLLOW_CORNER_TURN && g_lease==10U);
    cycle(&owner,3980U,false,false,&follow);
    assert(!g_active && follow.reason==FOLLOW_CORNER_TIMEOUT);
    cycle(&owner,4020U,false,false,&follow);
    assert(!g_active);

    begin(&owner,5000U,&follow);
    g_left=500U; g_right=500U;
    for (uint32_t t=5100U; t<=5140U; t+=20U)
    {cycle(&owner,t,false,false,&follow);}
    cycle(&owner,5160U,false,true,&follow);
    assert(!g_active && follow.reason==FOLLOW_BUTTON);

    begin(&owner,6000U,&follow);
    g_left=500U; g_right=500U;
    for (uint32_t t=6100U; t<=6140U; t+=20U)
    {cycle(&owner,t,false,false,&follow);}
    g_rgb_valid=false;
    cycle(&owner,6160U,false,false,&follow);
    assert(!g_active && follow.reason==FOLLOW_RGB);
    puts("Corner owner: cue, signed pivot, reacquisition, deadlines and STOP PASS");
}
#endif

#if !(FOLLOW_RED_SECOND_SLOW) && !(FOLLOW_TWO_TURN) && !(FOLLOW_CLIPPED_LOOP) && !(FOLLOW_CORNER_DIRECTION != 0) && FOLLOW_MULTICOLOUR_CORRIDOR
/* Check both the normalized IR and the resulting wheel output. */
static void check_multicolour_corridor(void)
{
    black_follow_owner_t owner;
    black_follow_status_t follow;
    begin(&owner,12000U,&follow);
    unsigned int armed=g_arm_calls;
    g_lane_colour=RGB_CLASS_YELLOW;
    /* Check both the normalized IR and the resulting wheel output. */
    g_left=355U; g_right=236U;
    cycle(&owner,12110U,false,false,&follow);
    assert(g_active && follow.phase==FOLLOW_RUNNING && follow.left_norm==408U);
    assert(g_applied_left==-EXPECT_STRAIGHT && g_applied_right==22);
    g_lane_colour=RGB_CLASS_RED; g_left=255U; g_right=500U;
    cycle(&owner,12130U,false,false,&follow);
    assert(g_active && g_applied_left==-EXPECT_INNER && g_applied_right==EXPECT_STRAIGHT);
    g_lane_colour=RGB_CLASS_BLUE; g_left=355U; g_right=315U;
    cycle(&owner,12150U,false,false,&follow);
    assert(g_active && follow.left_norm==142U && follow.right_norm==0U);
    assert(g_arm_calls==armed && owner.policy.run_started_ms==12090U);
    g_lane_colour=RGB_CLASS_UNKNOWN;
    cycle(&owner,12170U,false,false,&follow);
    assert(!g_active && follow.reason==FOLLOW_RGB);
    g_lane_colour=RGB_CLASS_RED;
    cycle(&owner,12190U,false,false,&follow);
    assert(!g_active && follow.reason==FOLLOW_RGB);

    begin(&owner,13000U,&follow);
    armed=g_arm_calls;
    g_lane_colour=RGB_CLASS_YELLOW; g_left=255U; g_right=236U;
    cycle(&owner,13100U,false,false,&follow);
    g_join_reading=&g_join_unknown; ++g_rgb_sequence;
    cycle(&owner,13110U,false,false,&follow);
    assert(g_active && follow.join_active && !follow.ready && follow.join_count==0U);
    g_join_reading=&g_join_background; ++g_rgb_sequence;
    cycle(&owner,13190U,false,false,&follow);
    assert(g_active && follow.join_active);
    g_join_reading=&g_join_ambiguous; ++g_rgb_sequence;
    cycle(&owner,13260U,false,false,&follow);
    assert(g_active && follow.join_active);
    g_join_reading=&g_join_black; ++g_rgb_sequence;
    cycle(&owner,13330U,false,false,&follow);
    assert(g_active && follow.join_active);
    g_join_reading=NULL; g_lane_colour=RGB_CLASS_BLUE; ++g_rgb_sequence;
    g_left=331U; g_right=315U;
    cycle(&owner,13400U,false,false,&follow);
    assert(g_active && follow.join_active && follow.join_count==0U && follow.left_norm==310U);
    cycle(&owner,13420U,false,false,&follow); /* Same acquisition cannot confirm. */
    assert(g_active && follow.join_active && follow.join_count==0U);
    ++g_rgb_sequence;
    cycle(&owner,13480U,false,false,&follow);
    assert(g_active && !follow.join_active && follow.join_count==1U && follow.left_norm==0U);
    assert(g_arm_calls==armed && owner.policy.run_started_ms==13090U);

    begin(&owner,14000U,&follow);
    g_join_reading=&g_join_unknown; ++g_rgb_sequence;
    g_rgb_age_ms=20U;
    cycle(&owner,14120U,false,false,&follow);
    g_rgb_age_ms=0U;
    for (uint32_t t=14140U; t<14400U; t+=20U)
    {cycle(&owner,t,false,false,&follow);}
    g_join_reading=NULL; g_lane_colour=RGB_CLASS_YELLOW; ++g_rgb_sequence;
    cycle(&owner,14400U,false,false,&follow); /* One good sample is insufficient. */
    g_join_reading=&g_join_black; ++g_rgb_sequence;
    for (uint32_t t=14420U; t<14580U; t+=20U)
    {cycle(&owner,t,false,false,&follow);}
    cycle(&owner,14580U,false,false,&follow);
    assert(g_active && follow.join_active && g_lease==10U);
    cycle(&owner,14590U,false,false,&follow);
    assert(!g_active && follow.reason==FOLLOW_JOIN_TIMEOUT && !follow.join_active);
    g_join_reading=NULL;
    cycle(&owner,14610U,false,false,&follow);
    assert(!g_active && follow.reason==FOLLOW_JOIN_TIMEOUT);

    begin(&owner,15000U,&follow);
    g_join_reading=&g_dark_black;
    cycle(&owner,15100U,false,false,&follow);
    assert(!g_active && follow.reason==FOLLOW_RGB && !follow.join_active);

    begin(&owner,16000U,&follow);
    g_join_reading=&g_join_black;
    cycle(&owner,16100U,false,false,&follow);
    assert(g_active && follow.join_active);
    cycle(&owner,16120U,false,true,&follow);
    assert(!g_active && follow.reason==FOLLOW_BUTTON);

    begin(&owner,17000U,&follow);
    g_join_reading=&g_join_background;
    cycle(&owner,17100U,false,false,&follow);
    g_left=500U; g_right=500U;
    cycle(&owner,17120U,false,false,&follow);
    assert(!g_active && follow.reason==FOLLOW_AMBIGUOUS);

    begin(&owner,18000U,&follow);
    g_join_reading=&g_join_unknown;
    cycle(&owner,18100U,false,false,&follow);
    g_rgb_valid=false;
    cycle(&owner,18120U,false,false,&follow);
    assert(!g_active && follow.reason==FOLLOW_RGB);

    begin(&owner,19000U,&follow);
    g_join_reading=&g_join_unknown;
    cycle(&owner,19100U,false,false,&follow);
    g_rgb_busy=true;
    for (uint32_t t=19120U; t<=19270U; t+=20U)
    {cycle(&owner,t,false,false,&follow);}
    cycle(&owner,19291U,false,false,&follow);
    assert(!g_active && follow.reason==FOLLOW_RGB);

    begin(&owner,20000U,&follow);
    for (uint32_t t=20110U; t<21070U; t+=20U)
    {cycle(&owner,t,false,false,&follow);}
    g_join_reading=&g_join_black;
    cycle(&owner,21070U,false,false,&follow);
    assert(g_active && follow.join_active && g_lease==10U);
    cycle(&owner,21090U,false,false,&follow);
    assert(!g_active && follow.reason==FOLLOW_COMPLETE);

    g_join_reading=&g_join_black;
    cycle(&owner,21120U,false,false,&follow);
    cycle(&owner,21150U,false,false,&follow);
    cycle(&owner,21160U,true,false,&follow);
    cycle(&owner,21200U,true,false,&follow);
    assert(!g_active && !follow.ready && follow.phase==FOLLOW_STOPPED);
}
#endif

#if !(FOLLOW_RED_SECOND_SLOW) && !(FOLLOW_TWO_TURN) && !(FOLLOW_CLIPPED_LOOP) && !(FOLLOW_CORNER_DIRECTION != 0)
/* Track profile: single line. */
static void check_single_line_profile(void)
{
    black_follow_owner_t owner;
    black_follow_status_t follow;
    begin(&owner,100U,&follow);
#if FOLLOW_BORDERED_CORRIDOR
    g_left=500U;
#else
    g_right=1444U;
#endif
    cycle(&owner,200U,false,false,&follow);
    assert(g_applied_left==-EXPECT_STRAIGHT && g_applied_right==EXPECT_INNER);
#if FOLLOW_BORDERED_CORRIDOR
    g_left=CENTRE_LEFT; g_right=500U;
#else
    g_right=CENTRE_RIGHT; g_left=1368U;
#endif
    cycle(&owner,220U,false,false,&follow);
    assert(g_applied_left==-EXPECT_INNER && g_applied_right==EXPECT_STRAIGHT);
    cycle(&owner,240U,false,true,&follow);
    assert(!g_active && follow.reason==FOLLOW_BUTTON);
    begin(&owner,1000U,&follow);
    g_ir_busy=true; cycle(&owner,1150U,false,false,&follow);
    assert(g_active); /* Last successful snapshot retains its original age. */
    assert(g_lease==30U); /* IR remainder40 less10ms clock-quantization guard. */
    cycle(&owner,1191U,false,false,&follow);
    assert(!g_active && follow.reason==FOLLOW_IR);
    begin(&owner,2000U,&follow);
    g_rgb_busy=true;
    for (uint32_t t=2110U; t<=2270U; t+=20U)
    {cycle(&owner,t,false,false,&follow);}
    assert(g_active);
    assert(g_lease==10U); /* RGB remainder20 less10ms clock guard. */
    cycle(&owner,2291U,false,false,&follow);
    assert(!g_active && follow.reason==FOLLOW_RGB);
    begin(&owner,3000U,&follow);
    g_rgb_valid=false; cycle(&owner,3100U,false,false,&follow);
    assert(!g_active && follow.reason==FOLLOW_RGB);
    begin(&owner,4000U,&follow);
    g_active=false; /* Independent cutoff; never silently resume. */
    unsigned int calls=g_drive_calls;
    cycle(&owner,4100U,false,false,&follow);
    assert(!g_active && g_drive_calls==calls && follow.reason==FOLLOW_DRIVER);
    begin(&owner,5000U,&follow);
    for (uint32_t t=5110U; t<=6050U; t+=20U)
    {cycle(&owner,t,false,false,&follow);}
    cycle(&owner,6070U,false,false,&follow);
    assert(g_active && g_lease==10U); /* Conservative run deadline guard. */
    cycle(&owner,6090U,false,false,&follow);
    assert(!g_active && follow.reason==FOLLOW_COMPLETE);
    begin(&owner,7000U,&follow);
    g_drive_error=-50; cycle(&owner,7100U,false,false,&follow);
    assert(!g_active && follow.reason==FOLLOW_DRIVER);
    begin(&owner,8000U,&follow);
    g_apply_delay_ms=150U;
    cycle(&owner,8100U,false,false,&follow);
    assert(!g_active && follow.reason==FOLLOW_DRIVER); /* Delayed renewal rejected. */
    g_now=9000U; g_apply_delay_ms=0U; g_active=false;
    black_follow_owner_init(&owner);
    cycle(&owner,9000U,false,false,&follow);
    cycle(&owner,9040U,false,false,&follow);
    cycle(&owner,9050U,true,false,&follow);
    g_apply_delay_ms=150U;
    cycle(&owner,9090U,true,false,&follow);
    assert(!g_active && follow.reason==FOLLOW_DRIVER); /* Delayed first apply. */
    begin(&owner,10000U,&follow);
    g_wrong_target=true; cycle(&owner,10100U,false,false,&follow);
    assert(!g_active && follow.reason==FOLLOW_RGB);
    g_wrong_target=false; cycle(&owner,10120U,false,false,&follow);
    assert(!g_active); /* Valid target recovery must not restart. */
#if FOLLOW_BLUE_CORRIDOR || FOLLOW_MULTICOLOUR_CORRIDOR
    begin(&owner,11000U,&follow);
    g_left=500U; g_right=500U;
    cycle(&owner,11100U,false,false,&follow);
    assert(!g_active && follow.reason==FOLLOW_AMBIGUOUS);
#endif
#if FOLLOW_MULTICOLOUR_CORRIDOR
    check_multicolour_corridor();
#endif
    assert(g_arm_calls>0U);
    puts("Follow owner: real steering/polarity, cached freshness, cutoff/deadline PASS");
}
#endif

int main(void)
{
#if FOLLOW_RED_SECOND_SLOW
    check_home_track_profile();
#elif FOLLOW_TWO_TURN
    check_two_turn_profile();
#elif FOLLOW_CLIPPED_LOOP
    check_clipped_loop_profile();
#elif FOLLOW_CORNER_DIRECTION != 0
    check_corner_direction_profile();
#else
    check_single_line_profile();
#endif
    return 0;
}
