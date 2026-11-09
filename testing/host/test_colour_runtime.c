/* Synthetic fixtures only. They are never linked as robot calibration. */
#include "colour_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static colour_calibration_t model(void)
{
    colour_calibration_t result;
    assert(colour_calibration_init(&result, 100U) == COLOUR_MODEL_OK);
    for (uint8_t sensor = 0U; sensor < 2U; ++sensor)
    {
        for (uint8_t id = 0U; id < 7U; ++id)
        {
            uint16_t lower = (uint16_t)((uint16_t)id * 500U + sensor * 50U);
            assert(colour_calibration_set_range(&result, sensor, id,
                                                lower, (uint16_t)(lower + 100U)) == 0U);
        }
    }
    return result;
}

int main(void)
{
    colour_runtime_t runtime;
    colour_report_t report;
    colour_calibration_t missing;
    assert(colour_calibration_init(&missing, 100U) == 0U);
    assert(colour_runtime_init(&runtime, &missing) == COLOUR_MODEL_MISSING);
    assert(!colour_runtime_latest(&runtime, 0U, &report));
    colour_runtime_update(&runtime, 123U, 456U, true, 10U, 10U);
    assert(colour_runtime_latest(&runtime, 10U, &report));
    assert(report.observation.result[0] == 15U && report.observation.result[1] == 15U);
    assert(report.observation.sequence == 1U && report.dropped == 0U);
    assert(colour_runtime_pop(&runtime, &report));
    assert(!colour_runtime_pop(&runtime, &report));

    colour_calibration_t calibrated = model();
    assert(colour_runtime_init(&runtime, &calibrated) == 0U);
    /* Copy/ownership: changing the source model must not affect the runtime. */
    memset(&calibrated, 0, sizeof(calibrated));
    for (uint8_t id = 0U; id < 7U; ++id)
    {
        colour_runtime_update(&runtime, (uint16_t)(id * 500U + 20U),
                               (uint16_t)(id * 500U + 70U), true, id, id);
        assert(colour_runtime_pop(&runtime, &report));
        assert(report.observation.result[0] == id && report.observation.result[1] == id);
    }
    /* A one-sample intermediate class is kept, with independent sensor IDs. */
    colour_runtime_update(&runtime, 20U, 70U, true, 20U, 20U);
    colour_runtime_update(&runtime, 520U, 1070U, true, 30U, 30U);
    colour_runtime_update(&runtime, 20U, 70U, true, 40U, 40U);
    assert(colour_runtime_pop(&runtime, &report) && report.observation.sequence == 8U);
    assert(colour_runtime_pop(&runtime, &report));
    assert(report.observation.result[0] == 1U && report.observation.result[1] == 2U);
    assert(report.observation.sampled_ms == 30U);
    assert(colour_runtime_pop(&runtime, &report) && report.observation.sequence == 10U);
    colour_runtime_update(&runtime, 21U, 71U, true, 50U, 50U);
    assert(!colour_runtime_pop(&runtime, &report));
    assert(colour_runtime_latest(&runtime, 149U, &report));
    assert(report.observation.result[0] == 0U && report.observation.raw[0] == 21U);
    assert(colour_runtime_latest(&runtime, 150U, &report));
    assert(report.observation.result[0] == 47U && report.observation.result[1] == 47U);
    colour_runtime_update(&runtime, 20U, 70U, false, 160U, 160U);
    assert(colour_runtime_pop(&runtime, &report) && report.observation.result[0] == 39U);
    colour_runtime_update(&runtime, 4096U, 70U, true, 170U, 170U);
    assert(colour_runtime_latest(&runtime, 170U, &report));
    assert(report.observation.result[0] == 39U && report.observation.raw[0] == 0U);

    calibrated = model();
    assert(colour_runtime_init(&runtime, &calibrated) == 0U);
    colour_runtime_update(&runtime, 20U, 70U, true, UINT32_MAX - 20U, UINT32_MAX - 20U);
    assert(colour_runtime_latest(&runtime, 10U, &report) && report.observation.result[0] == 0U);
    runtime.latest.sequence = UINT32_MAX;
    colour_runtime_update(&runtime, 520U, 570U, true, 20U, 20U);
    assert(runtime.latest.sequence == 0U);
    colour_runtime_reset_events(&runtime);
    assert(!colour_runtime_pop(&runtime, &report));
    for (uint32_t index = 0U; index < 20U; ++index)
    {
        colour_runtime_update(&runtime, (uint16_t)((index % 2U) * 500U + 20U),
                               70U, true, index + 30U, index + 30U);
    }
    assert(colour_runtime_pop(&runtime, &report));
    assert(report.dropped == 4U && report.observation.sampled_ms == 34U);
    runtime.dropped = UINT32_MAX;
    for (uint32_t index = 0U; index < 20U; ++index)
    {
        colour_runtime_update(&runtime, (uint16_t)((index % 2U) * 500U + 20U),
                               70U, true, index + 100U, index + 100U);
    }
    assert(colour_runtime_latest(&runtime, 119U, &report) && report.dropped == UINT32_MAX);

    calibrated = model();
    calibrated.present[1] = 0U;
    assert(colour_runtime_init(&runtime, &calibrated) == COLOUR_MODEL_BAD_CONFIG);
    colour_runtime_update(&runtime, 20U, 70U, true, 10U, 10U);
    assert(colour_runtime_latest(&runtime, 10U, &report) && report.observation.result[0] == 55U);
    calibrated = model();
    assert(colour_calibration_set_range(&calibrated, 0U, 1U, 50U, 200U) == 0U);
    assert(colour_runtime_init(&runtime, &calibrated) == COLOUR_MODEL_OVERLAP);
    colour_runtime_update(&runtime, 20U, 70U, true, 10U, 10U);
    assert(colour_runtime_latest(&runtime, 10U, &report) && report.observation.result[0] == 55U);
    assert(colour_runtime_init(NULL, &calibrated) == COLOUR_MODEL_BAD_ARGUMENT);
    assert(colour_runtime_init(&runtime, NULL) == COLOUR_MODEL_BAD_ARGUMENT);
    assert(!colour_runtime_latest(NULL, 0U, &report));
    assert(!colour_runtime_pop(&runtime, NULL));
    puts("PASS: runtime calibration, every-sample transitions, freshness, wrap and bounded loss");
    return 0;
}
