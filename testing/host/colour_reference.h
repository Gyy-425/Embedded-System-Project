/* Equivalent byte-aligned test reference; never installed on the robot. */
#ifndef AAI05_COLOUR_REFERENCE_H
#define AAI05_COLOUR_REFERENCE_H

#include "colour_classifier.h"

typedef struct
{
    uint16_t lower;
    uint16_t upper;
} test_colour_range_t;

typedef struct
{
    test_colour_range_t ranges[2][7];
    uint16_t max_age_ms;
    uint8_t present[2];
} test_colour_native_model_t;

typedef struct
{
    uint8_t colour;
    uint8_t reason;
} test_colour_native_result_t;

_Static_assert(sizeof(colour_calibration_t) == 46U, "Packed model size");
_Static_assert(sizeof(test_colour_native_model_t) == 60U, "Reference model");
_Static_assert(sizeof(colour_result_t) == 1U, "Packed result size");
_Static_assert(sizeof(test_colour_native_result_t) == 2U, "Reference result");

#endif
