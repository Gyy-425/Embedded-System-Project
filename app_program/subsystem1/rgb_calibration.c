#include "rgb_classifier.h"
#include "../../config/rgb_calibration_config.h"

const rgb_classifier_model_t *rgb_default_calibration(void)
{
    return &g_rgb_calibration;
}

const char *rgb_calibration_name(void)
{
#if AAI05_RGB_ROOM2
    return "BWYRB_20261122_HOME_ROOM2";
#elif FOLLOW_HOME_TRACK
    return "BWYRB_20261120_HOME_R2";
#elif AAI05_RGB_HOME_PROFILE
    return "BWYRB_20261119_HOME_R1";
#else
    return "BWYRB_20261119_R5";
#endif
}
