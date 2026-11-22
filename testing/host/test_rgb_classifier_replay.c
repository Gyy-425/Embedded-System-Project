/* Replay labels are recorded experimental labels, not classifier predictions. */
#include "rgb_classifier.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    const char *p_split;
    uint32_t now_ms;
    rgb_snapshot_t sample;
    uint8_t expected;
    uint8_t expected_reason;
    uint8_t gain;
    uint32_t integration_us;
} replay_vector_t;

/* Generated in a temporary directory by tools/check_rgb_diag.py from CSV. */
#if AAI05_RGB_HOME_PROFILE
#include "rgb_home_replay_vectors.h"
#else
#include "rgb_replay_vectors.h"
#endif

int main(void)
{
    unsigned int failed = 0U;
    const char *const splits[] = {
        "calibration", "validation", "unsupported-red", "fresh-cardboard",
        "track-black-initial", "track-black-repositioned", "track-cardboard", "black-flicker",
        "yellow-centre", "yellow-robot-left", "yellow-robot-right",
        "red-centre", "red-robot-left", "red-robot-right",
        "blue-centre", "blue-robot-left", "blue-robot-right",
#if AAI05_RGB_HOME_PROFILE
        "home-yellow", "home-blue", "home-red", "home-background",
#endif
    };
    unsigned int counts[sizeof(splits) / sizeof(splits[0])] = {0U};
    for (size_t i = 0U; i < sizeof(g_vectors) / sizeof(g_vectors[0]); ++i)
    {
        const replay_vector_t *p_vector = &g_vectors[i];
        rgb_classification_t result = rgb_classify(rgb_default_calibration(),
            &p_vector->sample, p_vector->now_ms, p_vector->gain,
            p_vector->integration_us);
        if ((rgb_classification_class(result) != p_vector->expected) ||
            (rgb_classification_reason(result) != p_vector->expected_reason))
        {
            ++failed;
            printf("row %zu %s expected=%u got=%s/%s\n", i + 1U,
                   p_vector->p_split, (unsigned int)p_vector->expected,
                   rgb_class_name(rgb_classification_class(result)),
                   rgb_reason_name(rgb_classification_reason(result)));
        }
        bool known_split = false;
        for (size_t j = 0U; j < sizeof(splits) / sizeof(splits[0]); ++j)
        {
            if (strcmp(p_vector->p_split, splits[j]) == 0)
            {
                ++counts[j];
                known_split = true;
            }
        }
        assert(known_split);
    }
    assert(counts[0] == 205U && counts[1] == 200U &&
           counts[2] == 61U && counts[3] == 23U && counts[4] == 26U &&
           counts[5] == 25U && counts[6] == 22U && counts[7] == 22U && counts[8] == 30U &&
           counts[9] == 40U && counts[10] == 39U && counts[11] == 39U &&
           counts[12] == 53U && counts[13] == 50U && counts[14] == 57U &&
           counts[15] == 62U && counts[16] == 77U);
#if AAI05_RGB_HOME_PROFILE
    assert(counts[17] == 34U && counts[18] == 37U &&
           counts[19] == 37U && counts[20] == 35U);
    printf("Home replay: yellow=%u blue=%u red=%u cardboard=%u\n",
           counts[17], counts[18], counts[19], counts[20]);
#endif
    printf("RGB replay: calibration=%u validation=%u red-rejection=%u "
           "fresh-background=%u track-black=%u/%u track-background=%u "
           "black-flicker=%u yellow-corridor=%u/%u/%u red-corridor=%u/%u/%u "
           "blue-corridor=%u/%u/%u failed=%u\n",
           counts[0], counts[1], counts[2], counts[3], counts[4], counts[5], counts[6],
           counts[7], counts[8], counts[9], counts[10], counts[11], counts[12],
           counts[13], counts[14], counts[15], counts[16], failed);
    return failed == 0U ? 0 : 1;
}
