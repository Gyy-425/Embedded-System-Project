#include "rgb_classifier.h"
#include <stddef.h>

static rgb_classification_t make_result(uint8_t colour, uint8_t reason)
{
    return (rgb_classification_t)(colour | (uint8_t)(reason << 3U));
}

static uint8_t validate_model(const rgb_classifier_model_t *p_model)
{
    if ((p_model == NULL) || ((p_model->present_mask & 0xC0U) != 0U) ||
        (p_model->max_age_ms == 0U) ||
        (p_model->max_age_ms > RGB_FRESH_MS) ||
        (p_model->integration_us != RGB_INTEGRATION_US) ||
        ((p_model->gain != 1U) && (p_model->gain != 4U) &&
         (p_model->gain != 16U) && (p_model->gain != 60U)))
    {
        return RGB_CLASS_BAD_MODEL;
    }
    if (p_model->present_mask == 0U)
    {
        return RGB_CLASS_UNCALIBRATED;
    }
    for (uint8_t colour = 0U; colour < RGB_PROFILE_COUNT; ++colour)
    {
        if ((p_model->present_mask & (1U << colour)) == 0U)
        {
            continue;
        }
        const rgb_profile_t *p_profile = &p_model->profiles[colour];
        if ((p_profile->clear_lower == 0U) ||
            (p_profile->clear_lower > p_profile->clear_upper) ||
            (p_profile->clear_upper >= RGB_SATURATION_COUNT))
        {
            return RGB_CLASS_BAD_MODEL;
        }
        for (uint8_t channel = 0U; channel < 3U; ++channel)
        {
            if ((p_profile->lower[channel] > p_profile->upper[channel]) ||
                (p_profile->upper[channel] > RGB_FRACTION_MAX))
            {
                return RGB_CLASS_BAD_MODEL;
            }
        }
    }
    return RGB_CLASS_MATCH;
}

rgb_classification_t rgb_classify(const rgb_classifier_model_t *p_model,
                                  const rgb_snapshot_t *p_sample,
                                  uint32_t now_ms, uint8_t gain,
                                  uint32_t integration_us)
{
    uint8_t reason = validate_model(p_model);
    if (reason != RGB_CLASS_MATCH)
    {
        return make_result(RGB_CLASS_UNKNOWN, reason);
    }
    if ((gain != p_model->gain) ||
        (integration_us != p_model->integration_us))
    {
        return make_result(RGB_CLASS_UNKNOWN, RGB_CLASS_EXPOSURE);
    }
    if ((p_sample == NULL) || !p_sample->ready || !p_sample->has_sample ||
        p_sample->saturated ||
        (p_sample->device_id != RGB_DEVICE_ID) || (p_sample->status != RGB_OK) ||
        (p_sample->red >= RGB_SATURATION_COUNT) ||
        (p_sample->green >= RGB_SATURATION_COUNT) ||
        (p_sample->blue >= RGB_SATURATION_COUNT) ||
        (p_sample->clear >= RGB_SATURATION_COUNT))
    {
        return make_result(RGB_CLASS_UNKNOWN, RGB_CLASS_INVALID);
    }
    if ((uint32_t)(now_ms - p_sample->sampled_ms) > p_model->max_age_ms)
    {
        return make_result(RGB_CLASS_UNKNOWN, RGB_CLASS_STALE);
    }
    /* The producer also clears valid on expiry; report STALE consistently
     * for otherwise healthy old samples, INVALID for fresh invalid data. */
    if (!p_sample->valid)
    {
        return make_result(RGB_CLASS_UNKNOWN, RGB_CLASS_INVALID);
    }
    uint32_t sum = (uint32_t)p_sample->red + p_sample->green + p_sample->blue;
    if (sum == 0U)
    {
        return make_result(RGB_CLASS_UNKNOWN, RGB_CLASS_NO_MATCH);
    }
    const uint16_t raw[3] = {p_sample->red, p_sample->green, p_sample->blue};
    uint16_t fractions[3];
    for (uint8_t channel = 0U; channel < 3U; ++channel)
    {
        /* Every raw channel <21504, so raw*4095 + sum/2 fits uint32_t. */
        fractions[channel] = (uint16_t)(((uint32_t)raw[channel] *
                              RGB_FRACTION_MAX + (sum / 2U)) / sum);
    }
    uint8_t match = RGB_CLASS_UNKNOWN;
    for (uint8_t colour = 0U; colour < RGB_PROFILE_COUNT; ++colour)
    {
        if ((p_model->present_mask & (1U << colour)) == 0U)
        {
            continue;
        }
        const rgb_profile_t *p_profile = &p_model->profiles[colour];
        bool matches = (p_sample->clear >= p_profile->clear_lower) &&
                       (p_sample->clear <= p_profile->clear_upper);
        for (uint8_t channel = 0U; channel < 3U; ++channel)
        {
            matches = matches && (fractions[channel] >= p_profile->lower[channel]) &&
                      (fractions[channel] <= p_profile->upper[channel]);
        }
        if (matches)
        {
            if (match != RGB_CLASS_UNKNOWN)
            {
                return make_result(RGB_CLASS_UNKNOWN, RGB_CLASS_AMBIGUOUS);
            }
            match = colour;
        }
    }
    return make_result(match, match == RGB_CLASS_UNKNOWN ? RGB_CLASS_NO_MATCH :
                                                        RGB_CLASS_MATCH);
}

uint8_t rgb_classification_class(rgb_classification_t result)
{
    return result & 7U;
}

uint8_t rgb_classification_reason(rgb_classification_t result)
{
    return (result >> 3U) & 7U;
}

const char *rgb_class_name(uint8_t colour)
{
    static const char *const names[RGB_PROFILE_COUNT] = {
        "BLACK", "WHITE", "YELLOW", "BACKGROUND", "RED", "BLUE"
    };
    return colour < RGB_PROFILE_COUNT ? names[colour] : "UNKNOWN";
}

const char *rgb_reason_name(uint8_t reason)
{
    static const char *const names[8] = {
        "MATCH", "UNCALIBRATED", "NO_MATCH", "AMBIGUOUS",
        "INVALID", "STALE", "BAD_MODEL", "EXPOSURE"
    };
    return reason < 8U ? names[reason] : "BAD_REASON";
}
