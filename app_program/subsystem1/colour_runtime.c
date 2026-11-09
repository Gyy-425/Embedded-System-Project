#include "colour_runtime.h"
#include <stddef.h>

colour_model_status_t colour_runtime_init(colour_runtime_t *p_runtime,
                                          const colour_calibration_t *p_model)
{
    if ((p_runtime == NULL) || (p_model == NULL))
    {
        return COLOUR_MODEL_BAD_ARGUMENT;
    }
    *p_runtime = (colour_runtime_t){.model = *p_model};
    colour_model_status_t left = colour_calibration_validate(p_model, 0U, NULL);
    colour_model_status_t right = colour_calibration_validate(p_model, 1U, NULL);
    colour_model_status_t status = COLOUR_MODEL_OK;
    if ((left == COLOUR_MODEL_MISSING) || (right == COLOUR_MODEL_MISSING))
    {
        status = ((left == COLOUR_MODEL_MISSING) &&
                  (right == COLOUR_MODEL_MISSING) &&
                  (p_model->present[0] == 0U) &&
                  (p_model->present[1] == 0U)) ?
            COLOUR_MODEL_MISSING : COLOUR_MODEL_BAD_CONFIG;
    }
    else if (left != COLOUR_MODEL_OK)
    {
        status = left;
    }
    else
    {
        status = right;
    }
    p_runtime->model_status = status;
    return status;
}

static colour_result_t colour_runtime_classify(const colour_runtime_t *p_runtime,
                                                uint8_t sensor, uint32_t now_ms)
{
    if ((p_runtime->model_status != COLOUR_MODEL_OK) &&
        (p_runtime->model_status != COLOUR_MODEL_MISSING))
    {
        return (colour_result_t)((COLOUR_REASON_INVALID_MODEL << 3U) |
                                  COLOUR_UNKNOWN);
    }
    colour_sample_t sample = {
        .sampled_ms = p_runtime->latest.sampled_ms,
        .raw = p_runtime->latest.raw[sensor],
        .is_valid = p_runtime->sample_valid};
    return colour_classify(&p_runtime->model, sensor, &sample, now_ms);
}

void colour_runtime_update(colour_runtime_t *p_runtime, uint16_t left,
                            uint16_t right, bool is_valid,
                            uint32_t sampled_ms, uint32_t now_ms)
{
    if (p_runtime == NULL)
    {
        return;
    }
    colour_result_t previous_left = p_runtime->latest.result[0];
    colour_result_t previous_right = p_runtime->latest.result[1];
    p_runtime->sample_valid = is_valid && (left <= COLOUR_ADC_MAX) &&
                              (right <= COLOUR_ADC_MAX);
    p_runtime->latest.sampled_ms = sampled_ms;
    /* Invalid ADC inputs carry INVALID_SAMPLE and an encodable zero placeholder. */
    p_runtime->latest.raw[0] = (left <= COLOUR_ADC_MAX) ? left : 0U;
    p_runtime->latest.raw[1] = (right <= COLOUR_ADC_MAX) ? right : 0U;
    p_runtime->latest.result[0] = colour_runtime_classify(p_runtime, 0U, now_ms);
    p_runtime->latest.result[1] = colour_runtime_classify(p_runtime, 1U, now_ms);
    bool changed = !p_runtime->has_sample ||
        (previous_left != p_runtime->latest.result[0]) ||
        (previous_right != p_runtime->latest.result[1]);
    p_runtime->has_sample = true;
    if (!changed)
    {
        return;
    }
    ++p_runtime->latest.sequence;
    if (p_runtime->count == COLOUR_EVENT_CAPACITY)
    {
        p_runtime->head = (uint8_t)((p_runtime->head + 1U) % COLOUR_EVENT_CAPACITY);
        --p_runtime->count;
        if (p_runtime->dropped != UINT32_MAX)
        {
            ++p_runtime->dropped;
        }
    }
    uint8_t index = (uint8_t)((p_runtime->head + p_runtime->count) %
                               COLOUR_EVENT_CAPACITY);
    p_runtime->events[index] = p_runtime->latest;
    ++p_runtime->count;
}
bool colour_runtime_latest(const colour_runtime_t *p_runtime, uint32_t now_ms,
                            colour_report_t *p_report)
{
    if ((p_runtime == NULL) || (p_report == NULL) || !p_runtime->has_sample)
    {
        return false;
    }
    p_report->observation = p_runtime->latest;
    p_report->dropped = p_runtime->dropped;
    p_report->observation.result[0] = colour_runtime_classify(p_runtime, 0U, now_ms);
    p_report->observation.result[1] = colour_runtime_classify(p_runtime, 1U, now_ms);
    return true;
}
bool colour_runtime_pop(colour_runtime_t *p_runtime, colour_report_t *p_report)
{
    if ((p_runtime == NULL) || (p_report == NULL) || (p_runtime->count == 0U))
    {
        return false;
    }
    p_report->observation = p_runtime->events[p_runtime->head];
    p_report->dropped = p_runtime->dropped;
    p_runtime->head = (uint8_t)((p_runtime->head + 1U) % COLOUR_EVENT_CAPACITY);
    --p_runtime->count;
    return true;
}
void colour_runtime_reset_events(colour_runtime_t *p_runtime)
{
    if (p_runtime != NULL)
    {
        p_runtime->head = 0U;
        p_runtime->count = 0U;
    }
}
