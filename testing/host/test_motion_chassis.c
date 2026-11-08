/* Breaks caught: reversed steering/polarity, unbounded output, metadata loss,
 * invalid input mutation and asymmetric clipping/rounding. */
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "motion_chassis.h"

static void test_vectors(void)
{
    static const struct
    {
        uint8_t forward;
        int8_t turn;
        int8_t left;
        int8_t right;
    } cases[] = {
        {0U, 0, 0, 0}, {0U, 50, 0, 0}, {0U, -50, 0, 0},
        {1U, 1, -2, 0}, {10U, 50, -20, 0}, {10U, -50, 0, 20},
        {30U, 10, -40, 20}, {40U, 0, -40, 40},
        {40U, 10, -50, 30}, {40U, -10, -30, 50},
        {40U, 20, -50, 16}, {40U, -20, -16, 50},
        {50U, 1, -50, 48}, {50U, 0, -50, 50},
        {50U, 50, -50, 0}, {50U, -50, 0, 50}
    };
    for (size_t index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index)
    {
        motion_chassis_command_t request = {UINT32_MAX, UINT16_MAX,
                                             cases[index].forward,
                                             cases[index].turn};
        motion_command_t raw;
        assert(motion_chassis_convert(&request, &raw) == MOTION_ACCEPTED);
        assert(raw.left_pct == cases[index].left);
        assert(raw.right_pct == cases[index].right);
        assert(raw.issued_ms == UINT32_MAX && raw.sequence == UINT16_MAX);
    }
}

static void test_invalid_preserves_output(void)
{
    motion_chassis_command_t request = {0U, 0U, 51U, 0};
    motion_command_t raw = {123U, 456U, -17, 23};
    assert(motion_chassis_convert(NULL, &raw) == MOTION_BAD_COMMAND);
    assert(motion_chassis_convert(&request, NULL) == MOTION_BAD_COMMAND);
    for (unsigned int forward = 0U; forward <= UINT8_MAX; ++forward)
    {
        for (int turn = INT8_MIN; turn <= INT8_MAX; ++turn)
        {
            request.forward_pct = (uint8_t)forward;
            request.turn_pct = (int8_t)turn;
            if ((forward > 50U) || (turn < -50) || (turn > 50))
            {
                assert(motion_chassis_convert(&request, &raw) == MOTION_BAD_COMMAND);
                assert(raw.issued_ms == 123U && raw.sequence == 456U);
                assert(raw.left_pct == -17 && raw.right_pct == 23);
            }
        }
    }
}

static void test_domain(bool emit_vectors)
{
    for (unsigned int forward = 0U; forward <= 50U; ++forward)
    {
        for (int turn = -50; turn <= 50; ++turn)
        {
            motion_chassis_command_t request = {0U, 0U, (uint8_t)forward,
                                                 (int8_t)turn};
            motion_command_t raw;
            motion_command_t mirror;
            assert(motion_chassis_convert(&request, &raw) == MOTION_ACCEPTED);
            request.turn_pct = (int8_t)-turn;
            assert(motion_chassis_convert(&request, &mirror) == MOTION_ACCEPTED);
            assert(raw.left_pct >= -50 && raw.left_pct <= 0);
            assert(raw.right_pct >= 0 && raw.right_pct <= 50);
            assert(-raw.left_pct == mirror.right_pct);
            assert(raw.right_pct == -mirror.left_pct);
            if (forward == 0U)
            {
                assert(raw.left_pct == 0 && raw.right_pct == 0);
            }
            else
            {
                assert(raw.left_pct != 0 || raw.right_pct != 0);
            }
            if (turn > 0)
            {
                assert(-raw.left_pct >= raw.right_pct);
            }
            if (turn < 0)
            {
                assert(-raw.left_pct <= raw.right_pct);
            }
            if (emit_vectors)
            {
                printf("CHASSIS %u %d %d %d\n", forward, turn,
                       (int)raw.left_pct, (int)raw.right_pct);
            }
        }
    }
}

int main(int argc, char **argv)
{
    bool emit_vectors = (argc == 2) && (strcmp(argv[1], "--vectors") == 0);
    test_vectors();
    test_invalid_preserves_output();
    test_domain(emit_vectors);
    puts("PASS: chassis literals, full byte-domain rejection, 5151 valid pairs, "
         "symmetry, duty bounds, forward-only output and metadata preservation");
    return 0;
}
