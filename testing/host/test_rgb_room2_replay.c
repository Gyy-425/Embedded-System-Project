/* ROOM2 lighting replay: stationary blue/cardboard captures from 22-Nov must
 * classify under the ROOM2 home-track profile. Old-room fixtures are replayed
 * separately against the unchanged home profile. */
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
#include "rgb_room2_replay_vectors.h"

int main(void)
{
    unsigned int failed = 0U;
    unsigned int blue = 0U;
    unsigned int background = 0U;
    assert(strcmp(rgb_calibration_name(), "BWYRB_20261122_HOME_ROOM2") == 0);
    for (size_t i = 0U; i < sizeof(g_room2_vectors) / sizeof(g_room2_vectors[0]); ++i)
    {
        const replay_vector_t *p_vector = &g_room2_vectors[i];
        rgb_classification_t result = rgb_classify(rgb_default_calibration(),
            &p_vector->sample, p_vector->now_ms, p_vector->gain,
            p_vector->integration_us);
        if ((rgb_classification_class(result) != p_vector->expected) ||
            (rgb_classification_reason(result) != p_vector->expected_reason))
        {
            ++failed;
            printf("row %zu %s expected=%u got=%s/%s\n", i + 1U, p_vector->p_split,
                   (unsigned int)p_vector->expected,
                   rgb_class_name(rgb_classification_class(result)),
                   rgb_reason_name(rgb_classification_reason(result)));
        }
        if (strcmp(p_vector->p_split, "room2-blue") == 0)
        {
            ++blue;
        }
        else if (strcmp(p_vector->p_split, "room2-background") == 0)
        {
            ++background;
        }
        else
        {
            assert(false);
        }
    }
    assert(blue == 24U && background == 21U);
    printf("ROOM2 replay: blue=%u cardboard=%u failed=%u\n", blue, background, failed);
    return failed == 0U ? 0 : 1;
}
