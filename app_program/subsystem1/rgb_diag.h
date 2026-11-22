#ifndef AAI05_RGB_DIAG_H
#define AAI05_RGB_DIAG_H
#include "rgb_sensor.h"
int32_t rgb_diag_init(void);
int32_t rgb_diag_start(void);
bool rgb_diag_snapshot(rgb_snapshot_t *p_snapshot);
void rgb_diag_report(void);
#endif
