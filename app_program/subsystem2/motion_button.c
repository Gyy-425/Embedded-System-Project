#include "motion_button.h"

void motion_button_init(motion_button_t *p_button, uint32_t now_ms)
{
    *p_button = (motion_button_t){0};
    p_button->changed_ms = now_ms;
    p_button->raw_pressed = true;
    p_button->stable_pressed = true;
}

bool motion_button_update(motion_button_t *p_button, uint32_t now_ms,
                           bool is_pressed, bool is_controller_available,
                           motion_command_t *p_command)
{
    bool is_new_press = false;

    if (is_pressed != p_button->raw_pressed)
    {
        p_button->raw_pressed = is_pressed;
        p_button->changed_ms = now_ms;
    }
    if ((uint32_t)(now_ms - p_button->changed_ms) >=
        MOTION_BUTTON_DEBOUNCE_MS)
    {
        if (!p_button->raw_pressed)
        {
            p_button->stable_pressed = false;
            p_button->is_released = true;
        }
        else if (!p_button->stable_pressed)
        {
            p_button->stable_pressed = true;
            is_new_press = p_button->is_released;
            p_button->is_released = false;
        }
    }
    if (is_new_press && !p_button->was_started && is_controller_available)
    {
        p_button->started_ms = now_ms;
        p_button->was_started = true;
        p_button->is_running = true;
    }
    if (!is_controller_available ||
        (p_button->is_running &&
         ((uint32_t)(now_ms - p_button->started_ms) >= MOTION_BUTTON_RUN_MS)))
    {
        p_button->is_running = false;
    }
    if (!p_button->is_running)
    {
        return false;
    }
    ++p_button->sequence;
    *p_command = (motion_command_t){now_ms, p_button->sequence, 50, 0};
    return true;
}
