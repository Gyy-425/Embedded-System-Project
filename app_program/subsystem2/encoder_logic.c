#include "encoder_logic.h"
#include <limits.h>

static void count_invalid(encoder_state_t *state)
{
    if (state->invalid_transitions < UINT32_MAX)
    {
        ++state->invalid_transitions;
    }
}

void encoder_init(encoder_state_t *state, uint8_t ab)
{
    state->count = 0;
    state->invalid_transitions = 0U;
    state->ab = ab & 3U;
    state->count_saturated = false;
}

void encoder_update(encoder_state_t *state, uint8_t ab)
{
    static const int8_t steps[16] = {
         0,  1, -1,  0,
        -1,  0,  0,  1,
         1,  0,  0, -1,
         0, -1,  1,  0
    };
    int8_t step;

    if ((ab > 3U) || (state->ab > 3U))
    {
        count_invalid(state);
        state->ab = ab & 3U;
        return;
    }
    if ((state->ab ^ ab) == 3U)
    {
        count_invalid(state);
        state->ab = ab;
        return;
    }
    step = steps[(state->ab << 2U) | ab];
    state->ab = ab;
    if (((step > 0) && (state->count == INT32_MAX)) ||
        ((step < 0) && (state->count == INT32_MIN))) {
        state->count_saturated = true;
        return;
    }
    state->count += step;
}
