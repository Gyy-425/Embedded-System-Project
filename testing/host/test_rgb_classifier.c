/* Behaviour tests: unsafe samples and unsupported colours must never match. */
#include "rgb_classifier.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static rgb_snapshot_t sample(uint16_t red, uint16_t green,
                             uint16_t blue, uint16_t clear)
{
    rgb_snapshot_t value = {.red=red, .green=green, .blue=blue, .clear=clear,
        .sampled_ms=100U, .device_id=0x44U, .status=RGB_OK,
        .ready=true, .has_sample=true, .valid=true};
    return value;
}

static rgb_classification_t classify(const rgb_classifier_model_t *p_model,
                                      const rgb_snapshot_t *p_value,
                                      uint32_t now)
{
    return rgb_classify(p_model, p_value, now, 4U, 50400U);
}

static void expect(rgb_classification_t result, uint8_t colour, uint8_t reason)
{
    assert(rgb_classification_class(result) == colour);
    assert(rgb_classification_reason(result) == reason);
    assert((result & 0xC0U) == 0U);
}

int main(void)
{
    const rgb_classifier_model_t *p_model = rgb_default_calibration();
    rgb_snapshot_t value = sample(274U, 276U, 252U, 662U);
    expect(classify(p_model, &value, 100U), RGB_CLASS_BLACK, RGB_CLASS_MATCH);
    /* Actual 35mm track, then an independently repositioned capture. */
    value = sample(202U, 211U, 193U, 528U);
    expect(classify(p_model, &value, 100U), RGB_CLASS_BLACK, RGB_CLASS_MATCH);
    value = sample(199U, 209U, 191U, 523U);
    expect(classify(p_model, &value, 100U), RGB_CLASS_BLACK, RGB_CLASS_MATCH);
    /* Latest known-black capture flickered on one-count proportion changes. */
    value = sample(200U, 211U, 195U, 522U);
    expect(classify(p_model, &value, 100U), RGB_CLASS_BLACK, RGB_CLASS_MATCH);
    value = sample(200U, 210U, 195U, 522U);
    expect(classify(p_model, &value, 100U), RGB_CLASS_BLACK, RGB_CLASS_MATCH);
    /* New assembled RED lane; local class4 fits the existing three class bits. */
    value = sample(360U, 259U, 250U, 706U);
    expect(classify(p_model, &value, 100U), RGB_CLASS_RED, RGB_CLASS_MATCH);
    value = sample(218U, 249U, 255U, 614U);
    rgb_classification_t blue = classify(p_model, &value, 100U);
    expect(blue, RGB_CLASS_BLUE, RGB_CLASS_MATCH);
    assert(blue == 0x05U && strcmp(rgb_class_name(RGB_CLASS_BLUE), "BLUE") == 0);
#if FOLLOW_HOME_TRACK
    /* All distinct readings from the 82 labelled new-track blue captures. */
    const uint16_t blue_readings[][4] = {
        {197U, 225U, 242U, 543U}, {197U, 225U, 243U, 543U},
        {197U, 225U, 243U, 544U}, {197U, 226U, 243U, 543U},
        {197U, 226U, 243U, 544U}, {198U, 227U, 244U, 545U},
        {196U, 224U, 241U, 542U},
    };
    for (size_t i = 0U; i < sizeof(blue_readings) / sizeof(blue_readings[0]); ++i)
    {
        const uint16_t *reading = blue_readings[i];
        value = sample(reading[0], reading[1], reading[2], reading[3]);
        expect(classify(p_model, &value, 100U), RGB_CLASS_BLUE, RGB_CLASS_MATCH);
    }
    value = sample(270U, 280U, 280U, 615U);
    expect(classify(p_model, &value, 100U), RGB_CLASS_UNKNOWN, RGB_CLASS_NO_MATCH);
#endif
    /* Existing real-profile overlap must still reject rather than force black. */
    value = sample(1420U, 1400U, 1275U, 700U);
    expect(classify(p_model, &value, 100U), RGB_CLASS_UNKNOWN, RGB_CLASS_AMBIGUOUS);
    /* One ADC count must not toggle this labelled cardboard to UNKNOWN. */
    value = sample(390U, 359U, 325U, 838U);
    expect(classify(p_model, &value, 100U), RGB_CLASS_BACKGROUND, RGB_CLASS_MATCH);
    value = sample(572U, 582U, 549U, 1435U);
    expect(classify(p_model, &value, 100U), RGB_CLASS_WHITE, RGB_CLASS_MATCH);
    value = sample(451U, 413U, 323U, 982U);
    expect(classify(p_model, &value, 100U), RGB_CLASS_YELLOW, RGB_CLASS_MATCH);
    /* Fresh cardboard brightness drift must not become BLACK. */
    value = sample(329U, 311U, 293U, 706U);
    expect(classify(p_model, &value, 100U), RGB_CLASS_BACKGROUND, RGB_CLASS_MATCH);
    /* Earlier red capture lies outside the new lane calibration; still UNKNOWN. */
    value = sample(328U, 275U, 272U, 643U);
    expect(classify(p_model, &value, 100U), RGB_CLASS_UNKNOWN, RGB_CLASS_NO_MATCH);
    value = sample(0U, 0U, 0U, 0U);
    expect(classify(p_model, &value, 100U), RGB_CLASS_UNKNOWN, RGB_CLASS_NO_MATCH);
    value = sample(274U, 276U, 252U, 662U);
    expect(classify(p_model, &value, 300U), RGB_CLASS_BLACK, RGB_CLASS_MATCH);
    expect(classify(p_model, &value, 301U), RGB_CLASS_UNKNOWN, RGB_CLASS_STALE);
    expect(classify(p_model, &value, 99U), RGB_CLASS_UNKNOWN, RGB_CLASS_STALE);
    value.sampled_ms = UINT32_MAX - 49U;
    expect(classify(p_model, &value, 50U), RGB_CLASS_BLACK, RGB_CLASS_MATCH);
    value.sampled_ms = 100U;
    value.sequence = 0U; /* Wrapped sequence is not a missing sample. */
    expect(classify(p_model, &value, 100U), RGB_CLASS_BLACK, RGB_CLASS_MATCH);
    value.valid = false;
    expect(classify(p_model, &value, 100U), RGB_CLASS_UNKNOWN, RGB_CLASS_INVALID);
    /* The diagnostic adapter clears valid when an otherwise healthy sample ages. */
    expect(classify(p_model, &value, 301U), RGB_CLASS_UNKNOWN, RGB_CLASS_STALE);
    value.valid = true; value.saturated = true;
    expect(classify(p_model, &value, 100U), RGB_CLASS_UNKNOWN, RGB_CLASS_INVALID);
    value.saturated = false; value.red = UINT16_MAX;
    expect(classify(p_model, &value, 100U), RGB_CLASS_UNKNOWN, RGB_CLASS_INVALID);
    value = sample(21503U, 21503U, 21503U, 21503U);
    expect(classify(p_model, &value, 100U), RGB_CLASS_UNKNOWN, RGB_CLASS_NO_MATCH);
    value.red = 21504U;
    expect(classify(p_model, &value, 100U), RGB_CLASS_UNKNOWN, RGB_CLASS_INVALID);
    value = sample(274U, 276U, 252U, 662U);
    value.device_id = 0x4DU;
    expect(classify(p_model, &value, 100U), RGB_CLASS_UNKNOWN, RGB_CLASS_INVALID);
    value.device_id = 0x44U; value.status = RGB_IO_ERROR;
    expect(classify(p_model, &value, 100U), RGB_CLASS_UNKNOWN, RGB_CLASS_INVALID);
    value.status = RGB_OK; value.ready = false;
    expect(classify(p_model, &value, 100U), RGB_CLASS_UNKNOWN, RGB_CLASS_INVALID);
    value.ready = true; value.has_sample = false;
    expect(classify(p_model, &value, 100U), RGB_CLASS_UNKNOWN, RGB_CLASS_INVALID);
    value.has_sample = true; value.errors = 1U; /* Historical error, recovered. */
    expect(classify(p_model, &value, 100U), RGB_CLASS_BLACK, RGB_CLASS_MATCH);
    expect(rgb_classify(p_model, &value, 100U, 16U, 50400U),
           RGB_CLASS_UNKNOWN, RGB_CLASS_EXPOSURE);
    expect(rgb_classify(p_model, &value, 100U, 4U, 24000U),
           RGB_CLASS_UNKNOWN, RGB_CLASS_EXPOSURE);
    expect(classify(NULL, &value, 100U), RGB_CLASS_UNKNOWN, RGB_CLASS_BAD_MODEL);
    expect(classify(p_model, NULL, 100U), RGB_CLASS_UNKNOWN, RGB_CLASS_INVALID);
    rgb_classifier_model_t model = *p_model;
    model.present_mask = 0U;
    expect(classify(&model, &value, 100U), RGB_CLASS_UNKNOWN, RGB_CLASS_UNCALIBRATED);
    model.present_mask = 0x80U;
    expect(classify(&model, &value, 100U), RGB_CLASS_UNKNOWN, RGB_CLASS_BAD_MODEL);
    model.present_mask = 0x40U;
    expect(classify(&model, &value, 100U), RGB_CLASS_UNKNOWN, RGB_CLASS_BAD_MODEL);
    model = *p_model; model.profiles[0].lower[0] = 4096U;
    expect(classify(&model, &value, 100U), RGB_CLASS_UNKNOWN, RGB_CLASS_BAD_MODEL);
    model = *p_model; model.max_age_ms = 0U;
    expect(classify(&model, &value, 100U), RGB_CLASS_UNKNOWN, RGB_CLASS_BAD_MODEL);
    model = *p_model; model.profiles[0].clear_lower = 1000U;
    model.profiles[0].clear_upper = 100U;
    expect(classify(&model, &value, 100U), RGB_CLASS_UNKNOWN, RGB_CLASS_BAD_MODEL);
    model = *p_model;
    model.present_mask = 3U;
    model.profiles[1] = model.profiles[0];
    expect(classify(&model, &value, 100U), RGB_CLASS_UNKNOWN, RGB_CLASS_AMBIGUOUS);
    model.present_mask = 1U;
    expect(classify(&model, &value, 100U), RGB_CLASS_BLACK, RGB_CLASS_MATCH);
    model = *p_model; model.gain = 0U;
    expect(classify(&model, &value, 100U), RGB_CLASS_UNKNOWN, RGB_CLASS_BAD_MODEL);
    model = *p_model; model.integration_us = 0U;
    expect(classify(&model, &value, 100U), RGB_CLASS_UNKNOWN, RGB_CLASS_BAD_MODEL);
    model = *p_model; model.max_age_ms = 201U;
    expect(classify(&model, &value, 100U), RGB_CLASS_UNKNOWN, RGB_CLASS_BAD_MODEL);
    model = *p_model; model.profiles[0].lower[1] = 1500U;
    expect(classify(&model, &value, 100U), RGB_CLASS_UNKNOWN, RGB_CLASS_BAD_MODEL);
    model = *p_model; model.profiles[0].clear_upper = 21504U;
    expect(classify(&model, &value, 100U), RGB_CLASS_UNKNOWN, RGB_CLASS_BAD_MODEL);
    value.green = 21504U;
    expect(classify(p_model, &value, 100U), RGB_CLASS_UNKNOWN, RGB_CLASS_INVALID);
    value = sample(274U, 276U, 21504U, 662U);
    expect(classify(p_model, &value, 100U), RGB_CLASS_UNKNOWN, RGB_CLASS_INVALID);
    value = sample(274U, 276U, 252U, 21504U);
    expect(classify(p_model, &value, 100U), RGB_CLASS_UNKNOWN, RGB_CLASS_INVALID);
    /* Equal channels are exactly 1365/4095 each: literal inclusive bounds. */
    model = *p_model; model.present_mask = 1U;
    model.profiles[0] = (rgb_profile_t){.lower={1365U,1365U,1365U},
        .upper={1365U,1365U,1365U}, .clear_lower=10U, .clear_upper=10U};
    value = sample(3U, 3U, 3U, 10U);
    expect(classify(&model, &value, 100U), RGB_CLASS_BLACK, RGB_CLASS_MATCH);
    value.clear = 9U;
    expect(classify(&model, &value, 100U), RGB_CLASS_UNKNOWN, RGB_CLASS_NO_MATCH);
    value.clear = 11U;
    expect(classify(&model, &value, 100U), RGB_CLASS_UNKNOWN, RGB_CLASS_NO_MATCH);
    value = sample(3U, 3U, 4U, 10U);
    expect(classify(&model, &value, 100U), RGB_CLASS_UNKNOWN, RGB_CLASS_NO_MATCH);
    printf("Native model bytes=%zu; classification result bytes=%zu (no wire ABI)\n",
           sizeof(model), sizeof(rgb_classification_t));
    puts("RGB classifier: profiles, rejection, health, exposure, wrap PASS");
    return 0;
}
