/* Synthetic intervals test correctness, not physical tape separability. */
#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "colour_reference.h"

static const test_colour_range_t g_test_ranges[2][7] =
{
    {{100U,199U}, {400U,499U}, {800U,899U}, {1200U,1299U},
     {1600U,1699U}, {2000U,2099U}, {3000U,3099U}},
    {{200U,299U}, {600U,699U}, {1000U,1099U}, {1400U,1499U},
     {1800U,1899U}, {2200U,2299U}, {3200U,3299U}}
};
static uint32_t g_test_comparisons;

static void test_fill(colour_calibration_t *p_model,
                       const test_colour_native_model_t *p_reference)
{
    assert(colour_calibration_init(p_model, p_reference->max_age_ms) ==
           COLOUR_MODEL_OK);
    for (uint8_t sensor = 0U; sensor < 2U; ++sensor)
    {
        for (uint8_t colour = 0U; colour < 7U; ++colour)
        {
            const test_colour_range_t *p_range =
                &p_reference->ranges[sensor][colour];
            assert(colour_calibration_set_range(p_model, sensor, colour,
                p_range->lower, p_range->upper) == COLOUR_MODEL_OK);
        }
    }
}

static test_colour_native_model_t test_reference(void)
{
    test_colour_native_model_t reference =
    {
        .max_age_ms = 100U, .present = {127U, 127U}
    };
    memcpy(reference.ranges, g_test_ranges, sizeof(g_test_ranges));
    return reference;
}

static colour_result_t test_read(const colour_calibration_t *p_model,
                                 uint8_t sensor, uint16_t raw)
{
    const colour_sample_t sample = {1000U, raw, true};
    return colour_classify(p_model, sensor, &sample, 1000U);
}

static void test_packing_and_missing(void)
{
    colour_calibration_t model;
    assert(colour_calibration_init(&model, 100U) == COLOUR_MODEL_OK);
    assert(model.present[0] == 0U && model.present[1] == 0U);
    assert(test_read(&model, 0U, 0U) == 0x0FU); /* unknown/uncalibrated */
    assert(colour_calibration_set_range(&model, 0U, 2U, 0x123U, 0xABCU) ==
           COLOUR_MODEL_OK);
    assert(model.bounds[0][2][0] == 0x12U);
    assert(model.bounds[0][2][1] == 0x3AU);
    assert(model.bounds[0][2][2] == 0xBCU);
    assert(model.present[0] == 4U && model.present[1] == 0U);
    assert(colour_calibration_validate(&model, 0U, NULL) ==
           COLOUR_MODEL_MISSING);
    assert(test_read(&model, 0U, 500U) == 0x0FU);
}

static void test_boundaries_and_channels(void)
{
    colour_calibration_t model;
    test_colour_native_model_t reference = test_reference();
    test_fill(&model, &reference);
    assert(colour_calibration_validate(&model, 0U, NULL) == COLOUR_MODEL_OK);
    assert(test_read(&model, 0U, 99U) == 0x17U); /* unknown/no match */
    assert(test_read(&model, 0U, 100U) == 0U);
    assert(test_read(&model, 0U, 199U) == 0U);
    assert(test_read(&model, 0U, 200U) == 0x17U);
    assert(test_read(&model, 1U, 200U) == 0U);
    assert(test_read(&model, 0U, 450U) == 1U);
    assert(test_read(&model, 0U, 3050U) == 6U);
    assert(colour_calibration_set_range(&model, 0U, 0U, 0U, 0U) ==
           COLOUR_MODEL_OK);
    assert(colour_calibration_set_range(&model, 0U, 1U, 4095U, 4095U) ==
           COLOUR_MODEL_OK);
    assert(test_read(&model, 0U, 0U) == 0U);
    assert(test_read(&model, 0U, 4095U) == 1U);
    model.present[1] = 0U;
    assert(test_read(&model, 0U, 0U) == 0U);
    assert(test_read(&model, 1U, 200U) == 0x0FU);
}

static void test_overlap_and_touching(void)
{
    colour_calibration_t model;
    test_colour_native_model_t reference = test_reference();
    uint8_t overlap = 255U;
    test_fill(&model, &reference);
    assert(colour_calibration_set_range(&model, 0U, 1U, 150U, 249U) ==
           COLOUR_MODEL_OK);
    assert(colour_calibration_validate(&model, 0U, &overlap) ==
           COLOUR_MODEL_OVERLAP);
    assert(overlap == 3U);
    assert(test_read(&model, 0U, 175U) == 0x1FU); /* unknown/ambiguous */
    assert(test_read(&model, 0U, 125U) == 0U);
    assert(test_read(&model, 0U, 225U) == 1U);
    assert(colour_calibration_set_range(&model, 0U, 1U, 199U, 249U) ==
           COLOUR_MODEL_OK);
    assert(test_read(&model, 0U, 198U) == 0U);
    assert(test_read(&model, 0U, 199U) == 0x1FU);
    assert(test_read(&model, 0U, 200U) == 1U);
    assert(colour_calibration_validate(&model, 1U, &overlap) ==
           COLOUR_MODEL_OK);
    assert(overlap == 0U);
}

static void test_failure_atomicity_and_corruption(void)
{
    colour_calibration_t model;
    test_colour_native_model_t reference = test_reference();
    test_fill(&model, &reference);
    colour_calibration_t before = model;
    assert(colour_calibration_init(&model, 0U) == COLOUR_MODEL_BAD_CONFIG);
    assert(memcmp(&model, &before, sizeof(model)) == 0);
    assert(colour_calibration_set_range(&model, 2U, 0U, 0U, 10U) ==
           COLOUR_MODEL_BAD_ARGUMENT);
    assert(colour_calibration_set_range(&model, 0U, 7U, 0U, 10U) ==
           COLOUR_MODEL_BAD_ARGUMENT);
    assert(colour_calibration_set_range(&model, 0U, 0U, 200U, 100U) ==
           COLOUR_MODEL_BAD_RANGE);
    assert(colour_calibration_set_range(&model, 0U, 0U, 0U, 4096U) ==
           COLOUR_MODEL_BAD_RANGE);
    assert(memcmp(&model, &before, sizeof(model)) == 0);
    model.present[0] |= 0x80U;
    assert(colour_calibration_validate(&model, 0U, NULL) ==
           COLOUR_MODEL_BAD_CONFIG);
    assert(test_read(&model, 0U, 120U) == 0x37U);
    model = before;
    model.bounds[0][0][0] = 0xFFU;
    model.bounds[0][0][1] = 0xF0U;
    model.bounds[0][0][2] = 0U; /* lower4095 > upper0 */
    assert(colour_calibration_validate(&model, 0U, NULL) ==
           COLOUR_MODEL_BAD_RANGE);
    assert(test_read(&model, 0U, 120U) == 0x37U);
    model = before;
    model.max_age_ms = 0U;
    assert(test_read(&model, 0U, 120U) == 0x37U);
}

static void test_invalid_samples_arguments_and_time(void)
{
    colour_calibration_t model;
    test_colour_native_model_t reference = test_reference();
    test_fill(&model, &reference);
    colour_sample_t sample = {1000U, 120U, true};
    uint8_t overlap = 255U;
    assert(colour_classify(NULL, 0U, &sample, 1000U) == 0x3FU);
    assert(colour_classify(&model, 2U, &sample, 1000U) == 0x3FU);
    assert(colour_classify(&model, 0U, NULL, 1000U) == 0x3FU);
    assert(colour_calibration_validate(NULL, 0U, &overlap) ==
           COLOUR_MODEL_BAD_ARGUMENT && overlap == 0U);
    assert(colour_calibration_init(NULL, 100U) == COLOUR_MODEL_BAD_ARGUMENT);
    assert(colour_calibration_set_range(NULL, 0U, 0U, 0U, 1U) ==
           COLOUR_MODEL_BAD_ARGUMENT);
    sample.is_valid = false;
    assert(colour_classify(&model, 0U, &sample, 1000U) == 0x27U);
    sample.is_valid = true;
    sample.raw = 4096U;
    assert(colour_classify(&model, 0U, &sample, 1000U) == 0x27U);
    sample.raw = 120U;
    assert(colour_classify(&model, 0U, &sample, 1099U) == 0U);
    assert(colour_classify(&model, 0U, &sample, 1100U) == 0x2FU);
    assert(colour_classify(&model, 0U, &sample, 999U) == 0x2FU);
    sample.sampled_ms = UINT32_MAX - 10U;
    assert(colour_classify(&model, 0U, &sample, 5U) == 0U);
    assert(colour_classify(&model, 0U, &sample, 89U) == 0x2FU);
    assert(colour_calibration_init(&model, UINT16_MAX) == COLOUR_MODEL_OK);
    test_fill(&model, &reference);
    model.max_age_ms = UINT16_MAX;
    sample.sampled_ms = 0U;
    assert(colour_classify(&model, 0U, &sample, 65534U) == 0U);
    assert(colour_classify(&model, 0U, &sample, 65535U) == 0x2FU);
}

static uint8_t test_reference_classify(
    const test_colour_native_model_t *p_model, uint8_t sensor, uint16_t raw)
{
    uint8_t matches = 0U;
    uint8_t selected = 7U;
    for (uint8_t colour = 0U; colour < 7U; ++colour)
    {
        const test_colour_range_t *p_range = &p_model->ranges[sensor][colour];
        if ((raw >= p_range->lower) && (raw <= p_range->upper))
        {
            selected = colour;
            ++matches;
        }
    }
    if (matches == 0U)
    {
        return 0x17U;
    }
    return (matches > 1U) ? 0x1FU : selected;
}

static void test_full_adc_reference_equivalence(void)
{
    for (uint8_t mode = 0U; mode < 2U; ++mode)
    {
        colour_calibration_t model;
        test_colour_native_model_t reference = test_reference();
        if (mode == 1U)
        {
            reference.ranges[0][1] = (test_colour_range_t){150U, 900U};
            reference.ranges[1][4] = (test_colour_range_t){0U, 4095U};
        }
        test_fill(&model, &reference);
        for (uint8_t sensor = 0U; sensor < 2U; ++sensor)
        {
            for (uint16_t raw = 0U; raw <= 4095U; ++raw)
            {
                colour_result_t result = test_read(&model, sensor, raw);
                assert(result == test_reference_classify(&reference, sensor,
                                                          raw));
                assert((result & 0xC0U) == 0U);
                uint8_t reason = colour_result_reason(result);
                uint8_t colour = colour_result_class(result);
                assert(reason == ((result >> 3U) & 7U));
                assert((reason == 0U) ? (colour < 7U) : (colour == 7U));
                ++g_test_comparisons;
            }
        }
    }
}

static void test_no_temporal_or_global_state(void)
{
    colour_calibration_t first;
    colour_calibration_t second;
    test_colour_native_model_t reference = test_reference();
    test_fill(&first, &reference);
    assert(test_read(&first, 0U, 120U) == 0U);
    assert(test_read(&first, 0U, 450U) == 1U);
    assert(test_read(&first, 0U, 850U) == 2U);
    reference.ranges[0][0] = (test_colour_range_t){400U, 499U};
    reference.ranges[0][1] = (test_colour_range_t){100U, 199U};
    test_fill(&second, &reference);
    assert(test_read(&second, 0U, 120U) == 1U);
    assert(test_read(&first, 0U, 120U) == 0U);
}

int main(void)
{
    test_packing_and_missing();
    test_boundaries_and_channels();
    test_overlap_and_touching();
    test_failure_atomicity_and_corruption();
    test_invalid_samples_arguments_and_time();
    test_full_adc_reference_equivalence();
    test_no_temporal_or_global_state();
    printf("PASS:7 groups; reference comparisons=%lu\n",
           (unsigned long)g_test_comparisons);
    printf("SIZE: model=%zu native_model=%zu result=%zu native_result=%zu "
           "sample=%zu\n", sizeof(colour_calibration_t),
           sizeof(test_colour_native_model_t), sizeof(colour_result_t),
           sizeof(test_colour_native_result_t), sizeof(colour_sample_t));
    return 0;
}
