#include "encoder_capture.h"
#include <limits.h>

static encoder_capture_t capture;

static uint8_t read_ab(uint32_t levels, unsigned int pin_a, unsigned int pin_b)
{
    return (uint8_t)((((levels >> pin_a) & 1U) << 1U) | ((levels >> pin_b) & 1U));
}

static void increment_saturating(uint32_t *value)
{
    if (*value < UINT32_MAX) ++*value;
}

static void capture_wheel(encoder_state_t *state, uint32_t *overrun,
                          uint32_t edges, uint8_t ab, bool ambiguous)
{
    if (edges == 0U) return;
    if (ambiguous) {
        /* Edge latches do not retain order or multiplicity. Never guess a delta. */
        increment_saturating(overrun);
        state->ab = ab;
    } else {
        encoder_update(state, ab);
    }
}

void encoder_capture_reset(void)
{
    uint32_t saved = encoder_io_lock();
    uint32_t levels = encoder_io_levels();
    encoder_init(&capture.left, read_ab(levels, APP_ENCODER_LEFT_A, APP_ENCODER_LEFT_B));
    encoder_init(&capture.right, read_ab(levels, APP_ENCODER_RIGHT_A, APP_ENCODER_RIGHT_B));
    capture.irq_count = 0U;
    capture.left_overrun = 0U;
    capture.right_overrun = 0U;
    encoder_io_unlock(saved);
}

void encoder_capture_irq(void)
{
    uint32_t saved = encoder_io_lock();
    uint32_t left_edges = encoder_io_pending(ENCODER_LEFT_BANK) & ENCODER_LEFT_EDGES;
    uint32_t right_edges = encoder_io_pending(ENCODER_RIGHT_BANK) & ENCODER_RIGHT_EDGES;
    if ((left_edges | right_edges) != 0U) {
        uint32_t first_left = left_edges;
        uint32_t first_right = right_edges;
        /* Associate movement with levels sampled BEFORE acknowledgment. Recheck
           latches around the sample so later edges cannot invent a direction. */
        uint32_t levels = encoder_io_levels();
        left_edges |= encoder_io_pending(ENCODER_LEFT_BANK) & ENCODER_LEFT_EDGES;
        right_edges |= encoder_io_pending(ENCODER_RIGHT_BANK) & ENCODER_RIGHT_EDGES;
        bool left_ambiguous = (left_edges != first_left) || ((left_edges & (left_edges - 1U)) != 0U);
        bool right_ambiguous = (right_edges != first_right) || ((right_edges & (right_edges - 1U)) != 0U);
        if (left_edges != 0U) encoder_io_ack(ENCODER_LEFT_BANK, left_edges);
        if (right_edges != 0U) encoder_io_ack(ENCODER_RIGHT_BANK, right_edges);
        uint32_t resync_levels = (left_ambiguous || right_ambiguous) ? encoder_io_levels() : levels;
        capture_wheel(&capture.left, &capture.left_overrun, left_edges,
                       read_ab(left_ambiguous ? resync_levels : levels,
                               APP_ENCODER_LEFT_A, APP_ENCODER_LEFT_B), left_ambiguous);
        capture_wheel(&capture.right, &capture.right_overrun, right_edges,
                       read_ab(right_ambiguous ? resync_levels : levels,
                               APP_ENCODER_RIGHT_A, APP_ENCODER_RIGHT_B), right_ambiguous);
        increment_saturating(&capture.irq_count);
    }
    encoder_io_unlock(saved);
}

void encoder_capture_read(encoder_capture_t *snapshot)
{
    uint32_t saved = encoder_io_lock();
    *snapshot = capture;
    encoder_io_unlock(saved);
}
