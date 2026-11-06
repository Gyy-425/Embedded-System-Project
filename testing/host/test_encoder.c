#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include "encoder_logic.h"

static void test_forward_reverse(void)
{
    encoder_state_t state;
    const uint8_t forward[] = {1U, 3U, 2U, 0U};
    const uint8_t reverse[] = {2U, 3U, 1U, 0U};
    encoder_init(&state, 0U);
    for (unsigned int repeat = 0U; repeat < 25U; ++repeat) {
        for (unsigned int i = 0U; i < 4U; ++i) {
            encoder_update(&state, forward[i]);
        }
    }
    assert(state.count == 100);
    for (unsigned int repeat = 0U; repeat < 25U; ++repeat) {
        for (unsigned int i = 0U; i < 4U; ++i) {
            encoder_update(&state, reverse[i]);
        }
    }
    assert(state.count == 0);
    assert(state.invalid_transitions == 0U);
}

static void test_bounce_and_no_change(void)
{
    encoder_state_t state;
    encoder_init(&state, 0U);
    encoder_update(&state, 0U);
    encoder_update(&state, 1U);
    encoder_update(&state, 0U);
    encoder_update(&state, 0U);
    assert(state.count == 0);
    assert(state.invalid_transitions == 0U);
}

static void test_diagonal_and_resynchronise(void)
{
    encoder_state_t state;
    encoder_init(&state, 0U);
    encoder_update(&state, 3U);
    assert(state.count == 0);
    assert(state.invalid_transitions == 1U);
    encoder_update(&state, 2U);
    assert(state.count == 1);
    assert(state.ab == 2U);
}

static void test_saturation(void)
{
    encoder_state_t state;
    encoder_init(&state, 0U);
    state.count = INT32_MAX;
    encoder_update(&state, 1U);
    assert(state.count == INT32_MAX);
    assert(state.count_saturated);
    encoder_init(&state, 0U);
    state.count = INT32_MIN;
    encoder_update(&state, 2U);
    assert(state.count == INT32_MIN);
    assert(state.count_saturated);
    state.invalid_transitions = UINT32_MAX;
    encoder_update(&state, 1U);
    assert(state.invalid_transitions == UINT32_MAX);
}

static void test_independent_wheels(void)
{
    encoder_state_t left;
    encoder_state_t right;
    encoder_init(&left, 0U);
    encoder_init(&right, 0U);
    encoder_update(&left, 1U);
    encoder_update(&right, 2U);
    assert(left.count == 1);
    assert(right.count == -1);
    assert(!left.count_saturated && !right.count_saturated);
}

int main(void)
{
    test_forward_reverse();
    test_bounce_and_no_change();
    test_diagonal_and_resynchronise();
    test_saturation();
    test_independent_wheels();
    puts("PASS: 5 encoder diagnostic test groups");
    return 0;
}
