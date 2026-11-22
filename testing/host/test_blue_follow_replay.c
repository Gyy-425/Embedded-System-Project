#include "black_follow.h"
#include "rgb_classifier.h"
#include <assert.h>
#include <stdio.h>

typedef struct
{
    uint32_t now_ms;
    uint32_t ir_ms;
    uint16_t left_raw;
    uint16_t right_raw;
    unsigned int position;
    rgb_snapshot_t rgb;
} follow_vector_t;

#include "blue_follow_vectors.h"

int main(void)
{
    const int8_t turns[] = {0, 8, -8};
    unsigned int counts[3] = {0U};
    for (size_t i = 0U; i < sizeof(g_vectors) / sizeof(g_vectors[0]); ++i)
    {
        const follow_vector_t *p_vector = &g_vectors[i];
        rgb_classification_t result = rgb_classify(rgb_default_calibration(),
            &p_vector->rgb, p_vector->now_ms, 4U, 50400U);
        assert(result == RGB_CLASS_BLUE);

        black_follow_t state;
        black_follow_init(&state, p_vector->now_ms - 100U);
        state.status.phase = FOLLOW_RUNNING;
        state.run_started_ms = p_vector->now_ms - 100U;
        black_follow_input_t input = {
            .now_ms = p_vector->now_ms, .ir_ms = p_vector->ir_ms,
            .left_raw = p_vector->left_raw, .right_raw = p_vector->right_raw,
            .has_ir = true, .ir_valid = true, .rgb_target_matched = true,
            .hardware_ready = true
        };
        black_follow_output_t output = black_follow_step(&state, &input);
        assert(p_vector->position < 3U);
        assert(output.drive && output.turn_pct == turns[p_vector->position]);
        ++counts[p_vector->position];
    }
    assert(counts[0] == 57U && counts[1] == 62U && counts[2] == 77U);
    printf("Measured blue corridor replay: centre=%u robot-left=%u robot-right=%u PASS\n",
           counts[0], counts[1], counts[2]);
    return 0;
}
