/* Public boot calibration, NOT measured yet. Never populate from test fixtures.
 * Keep all presence bits zero until actual tape captures justify all14 ranges.
 * Bounds use colour_classifier.h's explicit12+12-bit three-byte encoding.
 * Boot initialization rejects partial/overlapping/invalid deployment models.
 * Changes require rebuild/reboot; there is no live model mutation. */
#ifndef AAI05_COLOUR_CALIBRATION_CONFIG_H
#define AAI05_COLOUR_CALIBRATION_CONFIG_H

#include "../app_program/subsystem1/colour_classifier.h"

static const colour_calibration_t g_colour_boot_calibration = {
    .max_age_ms = 100U,
    .present = {0U, 0U}
};

#endif /* AAI05_COLOUR_CALIBRATION_CONFIG_H */
