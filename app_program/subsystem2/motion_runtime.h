#ifndef AAI05_MOTION_RUNTIME_H
#define AAI05_MOTION_RUNTIME_H
#include "motion_control.h"

/* Task-context API. Accepted means stored, not already physically applied.
 * One logical producer; STOP is latched outside any queue until reset. */
motion_result_t subsystem2_motion_submit(const motion_command_t *p_command);
motion_result_t subsystem2_motion_stop(void);

/* Internal startup/owner interfaces, not producer or ISR entry points. */
int32_t motion_runtime_init(void);
void motion_runtime_step(uint32_t now_ms, uint32_t buttons,
                          bool is_hardware_ready, bool is_driver_active,
                          motion_status_t *p_status);
#endif
