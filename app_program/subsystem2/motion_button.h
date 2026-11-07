#ifndef AAI05_MOTION_BUTTON_H
#define AAI05_MOTION_BUTTON_H
#include "motion_control.h"

#define MOTION_BUTTON_DEBOUNCE_MS 40U
#define MOTION_BUTTON_RUN_MS 2000U

/* Local exercise producer only; replace with Mission for autonomous operation. */
typedef struct
{
    uint32_t changed_ms;
    uint32_t started_ms;
    uint16_t sequence;
    bool raw_pressed;
    bool stable_pressed;
    bool is_released;
    bool was_started;
    bool is_running;
} motion_button_t;

void motion_button_init(motion_button_t *p_button, uint32_t now_ms);
bool motion_button_update(motion_button_t *p_button, uint32_t now_ms,
                           bool is_pressed, bool is_controller_available,
                           motion_command_t *p_command);
#endif
