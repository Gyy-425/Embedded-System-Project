#include "colour_classifier.h"
#include <stddef.h>

static uint16_t colour_bounds_lower(const uint8_t *p_bounds)
{
    return (uint16_t)(((uint32_t)p_bounds[0] << 4U) |
                      ((uint32_t)p_bounds[1] >> 4U));
}

static uint16_t colour_bounds_upper(const uint8_t *p_bounds)
{
    return (uint16_t)((((uint32_t)p_bounds[1] & 0x0FU) << 8U) |
                      p_bounds[2]);
}

static bool colour_config_is_valid(const colour_calibration_t *p_model)
{
    return (p_model->max_age_ms != 0U) &&
           ((((uint32_t)p_model->present[0] | p_model->present[1]) &
             0x80U) == 0U);
}

static colour_result_t colour_pack_result(uint8_t colour, uint8_t reason)
{
    return (colour_result_t)(((uint32_t)reason << 3U) | colour);
}

colour_model_status_t colour_calibration_init(colour_calibration_t *p_model,
                                             uint16_t max_age_ms)
{
    if (p_model == NULL)
    {
        return COLOUR_MODEL_BAD_ARGUMENT;
    }
    if (max_age_ms == 0U)
    {
        return COLOUR_MODEL_BAD_CONFIG;
    }
    *p_model = (colour_calibration_t){.max_age_ms = max_age_ms};
    return COLOUR_MODEL_OK;
}

colour_model_status_t colour_calibration_set_range(
    colour_calibration_t *p_model, uint8_t sensor, uint8_t colour,
    uint16_t lower, uint16_t upper)
{
    if ((p_model == NULL) || (sensor >= COLOUR_SENSOR_COUNT) ||
        (colour >= COLOUR_CLASS_COUNT))
    {
        return COLOUR_MODEL_BAD_ARGUMENT;
    }
    if ((lower > COLOUR_ADC_MAX) || (upper > COLOUR_ADC_MAX) ||
        (lower > upper))
    {
        return COLOUR_MODEL_BAD_RANGE;
    }
    if (!colour_config_is_valid(p_model))
    {
        return COLOUR_MODEL_BAD_CONFIG;
    }
    uint8_t *p_bounds = p_model->bounds[sensor][colour];
    p_bounds[0] = (uint8_t)((uint32_t)lower >> 4U);
    p_bounds[1] = (uint8_t)((((uint32_t)lower << 4U) & 0xF0U) |
                            ((uint32_t)upper >> 8U));
    p_bounds[2] = (uint8_t)((uint32_t)upper & 0xFFU);
    p_model->present[sensor] = (uint8_t)((uint32_t)p_model->present[sensor] |
                                       (UINT32_C(1) << colour));
    return COLOUR_MODEL_OK;
}

colour_model_status_t colour_calibration_validate(
    const colour_calibration_t *p_model, uint8_t sensor,
    uint8_t *p_overlap_mask)
{
    uint16_t lower[COLOUR_CLASS_COUNT];
    uint16_t upper[COLOUR_CLASS_COUNT];
    uint8_t overlap = 0U;

    if (p_overlap_mask != NULL)
    {
        *p_overlap_mask = 0U;
    }
    if ((p_model == NULL) || (sensor >= COLOUR_SENSOR_COUNT))
    {
        return COLOUR_MODEL_BAD_ARGUMENT;
    }
    if (!colour_config_is_valid(p_model))
    {
        return COLOUR_MODEL_BAD_CONFIG;
    }
    if (p_model->present[sensor] != COLOUR_PRESENT_ALL)
    {
        return COLOUR_MODEL_MISSING;
    }
    for (uint8_t colour = 0U; colour < COLOUR_CLASS_COUNT; ++colour)
    {
        lower[colour] = colour_bounds_lower(p_model->bounds[sensor][colour]);
        upper[colour] = colour_bounds_upper(p_model->bounds[sensor][colour]);
        if (lower[colour] > upper[colour])
        {
            return COLOUR_MODEL_BAD_RANGE;
        }
    }
    for (uint8_t left = 0U; left < COLOUR_CLASS_COUNT; ++left)
    {
        for (uint8_t right = (uint8_t)(left + 1U);
             right < COLOUR_CLASS_COUNT; ++right)
        {
            if ((lower[left] <= upper[right]) &&
                (lower[right] <= upper[left]))
            {
                overlap = (uint8_t)((uint32_t)overlap |
                    (UINT32_C(1) << left) | (UINT32_C(1) << right));
            }
        }
    }
    if (p_overlap_mask != NULL)
    {
        *p_overlap_mask = overlap;
    }
    return (overlap == 0U) ? COLOUR_MODEL_OK : COLOUR_MODEL_OVERLAP;
}

colour_result_t colour_classify(const colour_calibration_t *p_model,
                                uint8_t sensor,
                                const colour_sample_t *p_sample,
                                uint32_t now_ms)
{
    if ((p_model == NULL) || (p_sample == NULL) ||
        (sensor >= COLOUR_SENSOR_COUNT))
    {
        return colour_pack_result(COLOUR_UNKNOWN, COLOUR_REASON_BAD_ARGUMENT);
    }
    if (!p_sample->is_valid || (p_sample->raw > COLOUR_ADC_MAX))
    {
        return colour_pack_result(COLOUR_UNKNOWN,
                                  COLOUR_REASON_INVALID_SAMPLE);
    }
    colour_model_status_t validation =
        colour_calibration_validate(p_model, sensor, NULL);
    if (validation == COLOUR_MODEL_MISSING)
    {
        return colour_pack_result(COLOUR_UNKNOWN,
                                  COLOUR_REASON_UNCALIBRATED);
    }
    if ((validation != COLOUR_MODEL_OK) &&
        (validation != COLOUR_MODEL_OVERLAP))
    {
        return colour_pack_result(COLOUR_UNKNOWN, COLOUR_REASON_INVALID_MODEL);
    }
    uint32_t age_ms = now_ms - p_sample->sampled_ms;
    if (age_ms >= p_model->max_age_ms)
    {
        return colour_pack_result(COLOUR_UNKNOWN, COLOUR_REASON_STALE_SAMPLE);
    }
    uint8_t selected = COLOUR_UNKNOWN;
    uint8_t matches = 0U;
    for (uint8_t colour = 0U; colour < COLOUR_CLASS_COUNT; ++colour)
    {
        const uint8_t *p_bounds = p_model->bounds[sensor][colour];
        if ((p_sample->raw >= colour_bounds_lower(p_bounds)) &&
            (p_sample->raw <= colour_bounds_upper(p_bounds)))
        {
            selected = colour;
            ++matches;
        }
    }
    if (matches == 0U)
    {
        return colour_pack_result(COLOUR_UNKNOWN, COLOUR_REASON_NO_MATCH);
    }
    if (matches > 1U)
    {
        return colour_pack_result(COLOUR_UNKNOWN, COLOUR_REASON_AMBIGUOUS);
    }
    return colour_pack_result(selected, COLOUR_REASON_MATCH);
}

uint8_t colour_result_class(colour_result_t result)
{
    return (uint8_t)((uint32_t)result & 7U);
}

uint8_t colour_result_reason(colour_result_t result)
{
    return (uint8_t)(((uint32_t)result >> 3U) & 7U);
}
