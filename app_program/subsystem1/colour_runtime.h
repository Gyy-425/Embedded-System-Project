#ifndef AAI05_COLOUR_RUNTIME_H
#define AAI05_COLOUR_RUNTIME_H

#include "colour_classifier.h"

#define COLOUR_EVENT_CAPACITY 16U

typedef struct
{
    uint32_t sequence;
    uint32_t sampled_ms;
    uint16_t raw[COLOUR_SENSOR_COUNT];
    colour_result_t result[COLOUR_SENSOR_COUNT];
} colour_observation_t;

typedef struct
{
    colour_observation_t observation;
    uint32_t dropped;
} colour_report_t;

/* Caller serializes all access. Model copied once, never edited in place. */
typedef struct
{
    colour_calibration_t model;
    colour_observation_t latest;
    colour_observation_t events[COLOUR_EVENT_CAPACITY];
    uint32_t dropped;
    uint8_t head;
    uint8_t count;
    colour_model_status_t model_status;
    bool has_sample;
    bool sample_valid;
} colour_runtime_t;

colour_model_status_t colour_runtime_init(colour_runtime_t *p_runtime,
                                          const colour_calibration_t *p_model);
void colour_runtime_update(colour_runtime_t *p_runtime, uint16_t left,
                            uint16_t right, bool is_valid,
                            uint32_t sampled_ms, uint32_t now_ms);
bool colour_runtime_latest(const colour_runtime_t *p_runtime, uint32_t now_ms,
                            colour_report_t *p_report);
bool colour_runtime_pop(colour_runtime_t *p_runtime, colour_report_t *p_report);
void colour_runtime_reset_events(colour_runtime_t *p_runtime);

#endif /* AAI05_COLOUR_RUNTIME_H */
