/* Portable fixed-exposure RGB classification; no I/O or mutable state. */
#ifndef AAI05_RGB_CLASSIFIER_H
#define AAI05_RGB_CLASSIFIER_H

#include "rgb_sensor.h"

#define RGB_CLASS_BLACK 0U
#define RGB_CLASS_WHITE 1U
#define RGB_CLASS_YELLOW 2U
#define RGB_CLASS_BACKGROUND 3U
#define RGB_CLASS_RED 4U
#define RGB_CLASS_BLUE 5U
#define RGB_CLASS_UNKNOWN 7U
#define RGB_PROFILE_COUNT 6U
#define RGB_FRACTION_MAX 4095U

#define RGB_CLASS_MATCH 0U
#define RGB_CLASS_UNCALIBRATED 1U
#define RGB_CLASS_NO_MATCH 2U
#define RGB_CLASS_AMBIGUOUS 3U
#define RGB_CLASS_INVALID 4U
#define RGB_CLASS_STALE 5U
#define RGB_CLASS_BAD_MODEL 6U
#define RGB_CLASS_EXPOSURE 7U

/* Local result encoding, not the existing IR/MQTT protocol:
 * class bits 0..2, reason bits 3..5, reserved bits 6..7 zero. */
typedef uint8_t rgb_classification_t;

typedef struct
{
    uint16_t lower[3]; /* R, G, B fractions of (R+G+B), scaled 0..4095. */
    uint16_t upper[3];
    uint16_t clear_lower;
    uint16_t clear_upper;
} rgb_profile_t;

/* Native configuration, not a serialized structure. Caller provides an
 * immutable model or a private copy; never update shared bounds in place.
 * Only bits 0..5 of present_mask are defined, indexed by RGB_CLASS_* IDs. */
typedef struct
{
    rgb_profile_t profiles[RGB_PROFILE_COUNT];
    uint32_t integration_us;
    uint16_t max_age_ms; /* 1..RGB_FRESH_MS, unsigned wrap-safe sample age. */
    uint8_t gain;
    uint8_t present_mask;
} rgb_classifier_model_t;

/* Sample and model must be coherent private/immutable copies. The adapter
 * supplies the exposure actually used to acquire the snapshot. Model/exposure
 * errors precede sample errors. A recovered historical errors counter does
 * not invalidate a fresh, currently healthy sample. No smoothing is applied. */
rgb_classification_t rgb_classify(const rgb_classifier_model_t *p_model,
                                  const rgb_snapshot_t *p_sample,
                                  uint32_t now_ms, uint8_t gain,
                                  uint32_t integration_us);
uint8_t rgb_classification_class(rgb_classification_t result);
uint8_t rgb_classification_reason(rgb_classification_t result);
const char *rgb_class_name(uint8_t colour);
const char *rgb_reason_name(uint8_t reason);
const rgb_classifier_model_t *rgb_default_calibration(void);
const char *rgb_calibration_name(void);

#endif /* AAI05_RGB_CLASSIFIER_H */
