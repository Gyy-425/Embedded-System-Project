/* Portable calibrated ADC classifier. No real-tape defaults or runtime I/O. */
#ifndef AAI05_COLOUR_CLASSIFIER_H
#define AAI05_COLOUR_CLASSIFIER_H

#include <stdbool.h>
#include <stdint.h>

#define COLOUR_CLASS_COUNT 7U
#define COLOUR_SENSOR_COUNT 2U
#define COLOUR_SENSOR_LEFT 0U
#define COLOUR_SENSOR_RIGHT 1U
#define COLOUR_UNKNOWN 7U
#define COLOUR_ADC_MAX 4095U
#define COLOUR_PRESENT_ALL 0x7FU

#define COLOUR_MODEL_OK 0U
#define COLOUR_MODEL_MISSING 1U
#define COLOUR_MODEL_BAD_ARGUMENT 2U
#define COLOUR_MODEL_BAD_RANGE 3U
#define COLOUR_MODEL_BAD_CONFIG 4U
#define COLOUR_MODEL_OVERLAP 5U
typedef uint8_t colour_model_status_t;

#define COLOUR_REASON_MATCH 0U
#define COLOUR_REASON_UNCALIBRATED 1U
#define COLOUR_REASON_NO_MATCH 2U
#define COLOUR_REASON_AMBIGUOUS 3U
#define COLOUR_REASON_INVALID_SAMPLE 4U
#define COLOUR_REASON_STALE_SAMPLE 5U
#define COLOUR_REASON_INVALID_MODEL 6U
#define COLOUR_REASON_BAD_ARGUMENT 7U
typedef uint8_t colour_result_t;

/* Bounds: lower12 bits then upper12 bits, explicit three-byte encoding.
 * Presence bits0..6 correspond to black, white, colour1..5; bit7 is zero.
 * This native model is NOT a MQTT/file format (max_age_ms is native uint16).
 * Configure private copies; never mutate a model while readers use it. */
typedef struct
{
    uint8_t bounds[COLOUR_SENSOR_COUNT][COLOUR_CLASS_COUNT][3];
    uint16_t max_age_ms;
    uint8_t present[COLOUR_SENSOR_COUNT];
} colour_calibration_t;

typedef struct
{
    uint32_t sampled_ms;
    uint16_t raw;
    bool is_valid;
} colour_sample_t;

colour_model_status_t colour_calibration_init(colour_calibration_t *p_model,
                                             uint16_t max_age_ms);
colour_model_status_t colour_calibration_set_range(
    colour_calibration_t *p_model, uint8_t sensor, uint8_t colour,
    uint16_t lower, uint16_t upper);
/* Optional overlap output; must not alias model storage. Cleared on errors. */
colour_model_status_t colour_calibration_validate(
    const colour_calibration_t *p_model, uint8_t sensor,
    uint8_t *p_overlap_mask);
colour_result_t colour_classify(const colour_calibration_t *p_model,
                                uint8_t sensor,
                                const colour_sample_t *p_sample,
                                uint32_t now_ms);
/* Result: class bits2..0, reason bits5..3, reserved bits7..6 zero.
 * Accessors consume results produced by this core, not untrusted wire bytes. */
uint8_t colour_result_class(colour_result_t result);
uint8_t colour_result_reason(colour_result_t result);

#endif /* AAI05_COLOUR_CLASSIFIER_H */
