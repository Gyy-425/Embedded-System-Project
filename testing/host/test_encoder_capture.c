#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "encoder_capture.h"

/* Hardware boundary model; the production portable capture service runs unchanged. */
static uint32_t levels;
static uint32_t pending[4];
static unsigned int lock_depth;
static int late_edge_injection;

static void inject_two_late_edges(void)
{
    /* After B-rise: 01 -> 11 -> 10, two more forward edges. */
    levels = (UINT32_C(1) << APP_ENCODER_LEFT_A);
    pending[0] |= (UINT32_C(8) << (APP_ENCODER_LEFT_A * 4U)) |
                  (UINT32_C(4) << (APP_ENCODER_LEFT_B * 4U));
    late_edge_injection = 0;
}

uint32_t encoder_io_lock(void)
{
    assert(lock_depth == 0U);
    lock_depth = 1U;
    return 17U;
}
void encoder_io_unlock(uint32_t saved)
{
    assert(saved == 17U && lock_depth == 1U);
    lock_depth = 0U;
}
uint32_t encoder_io_levels(void)
{
    assert(lock_depth == 1U);
    if (late_edge_injection == 2) inject_two_late_edges();
    return levels;
}
uint32_t encoder_io_pending(unsigned int bank)
{
    assert(lock_depth == 1U && bank < 4U);
    return pending[bank];
}
void encoder_io_ack(unsigned int bank, uint32_t edges)
{
    assert(lock_depth == 1U && bank < 4U);
    assert((edges & ~(bank == ENCODER_LEFT_BANK ? ENCODER_LEFT_EDGES : ENCODER_RIGHT_EDGES)) == 0U);
    pending[bank] &= ~edges;
    if (late_edge_injection == 1) inject_two_late_edges();
}

static uint32_t wheel_levels(uint8_t ab, unsigned int pin_a, unsigned int pin_b)
{
    return (((uint32_t)ab >> 1U) << pin_a) | (((uint32_t)ab & 1U) << pin_b);
}

/* Regression adapter models v0.1: no interrupt capture, update at task read only. */
#ifdef AAI05_LEGACY_POLL_TEST
static encoder_capture_t legacy;
void encoder_capture_reset(void)
{
    memset(&legacy, 0, sizeof(legacy));
    encoder_init(&legacy.left, (uint8_t)(((levels >> 4U) & 1U) * 2U + ((levels >> 5U) & 1U)));
    encoder_init(&legacy.right, (uint8_t)(((levels >> 16U) & 1U) * 2U + ((levels >> 17U) & 1U)));
}
void encoder_capture_irq(void) { /* v0.1 has no edge handler. */ }
void encoder_capture_read(encoder_capture_t *snapshot)
{
    encoder_update(&legacy.left, (uint8_t)(((levels >> 4U) & 1U) * 2U + ((levels >> 5U) & 1U)));
    encoder_update(&legacy.right, (uint8_t)(((levels >> 16U) & 1U) * 2U + ((levels >> 17U) & 1U)));
    *snapshot = legacy;
}
#endif

static void reset_capture(uint8_t left, uint8_t right)
{
    assert(lock_depth == 0U);
    levels = wheel_levels(left, APP_ENCODER_LEFT_A, APP_ENCODER_LEFT_B) |
             wheel_levels(right, APP_ENCODER_RIGHT_A, APP_ENCODER_RIGHT_B);
    memset(pending, 0, sizeof(pending));
    late_edge_injection = 0;
    encoder_capture_reset();
}

static void move_to(uint8_t left, uint8_t right, int service)
{
    const unsigned int pins[] = {APP_ENCODER_LEFT_A, APP_ENCODER_LEFT_B,
                                 APP_ENCODER_RIGHT_A, APP_ENCODER_RIGHT_B};
    uint32_t next = wheel_levels(left, pins[0], pins[1]) | wheel_levels(right, pins[2], pins[3]);
    for (unsigned int i = 0U; i < 4U; ++i) {
        unsigned int pin = pins[i];
        if (((levels ^ next) & (UINT32_C(1) << pin)) != 0U) {
            uint32_t event = ((next >> pin) & 1U) != 0U ? 8U : 4U;
            pending[pin / 8U] |= event << ((pin % 8U) * 4U);
        }
    }
    levels = next;
    if (service != 0) encoder_capture_irq();
}

static void test_cycles_between_task_reads(void)
{
    const uint8_t forward[] = {1U, 3U, 2U, 0U};
    const uint8_t reverse[] = {2U, 3U, 1U, 0U};
    encoder_capture_t result;
    reset_capture(0U, 0U);
    /* No task snapshot is read until 4000 edge events have arrived on each wheel. */
    for (unsigned int cycle = 0U; cycle < 1000U; ++cycle) {
        for (unsigned int i = 0U; i < 4U; ++i) move_to(forward[i], reverse[i], 1);
    }
    encoder_capture_read(&result);
    assert(result.left.count == 4000);
    assert(result.right.count == -4000);
    assert(result.irq_count == 4000U);
    assert(result.left.invalid_transitions == 0U && result.right.invalid_transitions == 0U);
    assert(result.left_overrun == 0U && result.right_overrun == 0U);
}

static void test_initial_state_and_independent_wheels(void)
{
    encoder_capture_t result;
    reset_capture(3U, 1U);
    encoder_capture_read(&result);
    assert(result.left.count == 0 && result.right.count == 0);
    assert(result.left.ab == 3U && result.right.ab == 1U);
    move_to(2U, 1U, 1);
    encoder_capture_read(&result);
    assert(result.left.count == 1 && result.right.count == 0);
    move_to(2U, 3U, 1);
    move_to(2U, 1U, 1);
    encoder_capture_read(&result);
    assert(result.left.count == 1 && result.right.count == 0);
    assert(result.irq_count == 3U);
}

static void test_unrelated_events_preserved(void)
{
    encoder_capture_t result;
    const uint32_t unrelated = UINT32_C(8) << 24U; /* GP6 rising edge */
    reset_capture(0U, 0U);
    pending[0] = unrelated;
    encoder_capture_irq();
    encoder_capture_read(&result);
    assert(result.irq_count == 0U && pending[0] == unrelated);
    move_to(1U, 0U, 1);
    encoder_capture_read(&result);
    assert(result.left.count == 1 && result.right.count == 0);
    assert(pending[0] == unrelated);
}

static void test_coalesced_edges_are_not_invented_counts(void)
{
    encoder_capture_t result;
    reset_capture(0U, 0U);
    move_to(1U, 0U, 0);
    move_to(0U, 0U, 0); /* Both edges on B latched before service. */
    encoder_capture_irq();
    encoder_capture_read(&result);
    assert(result.left_overrun == 1U && result.left.count == 0);
    move_to(1U, 0U, 1);
    move_to(3U, 0U, 0);
    move_to(2U, 0U, 0); /* Two channels changed; direction history is ambiguous. */
    encoder_capture_irq();
    encoder_capture_read(&result);
    assert(result.left_overrun == 2U && result.left.count == 1 && result.left.ab == 2U);
    move_to(0U, 0U, 1);
    encoder_capture_read(&result);
    assert(result.left.count == 2 && result.right.count == 0);
}

static void test_diagonal_rejection(void)
{
    encoder_capture_t result;
    reset_capture(0U, 0U);
    levels = wheel_levels(3U, APP_ENCODER_LEFT_A, APP_ENCODER_LEFT_B);
    pending[0] = UINT32_C(8) << (APP_ENCODER_LEFT_A * 4U);
    encoder_capture_irq();
    encoder_capture_read(&result);
    assert(result.left.count == 0 && result.left.invalid_transitions == 1U);
    assert(result.left_overrun == 0U && result.left.ab == 3U);
}

static void test_edges_after_ack_do_not_invent_reverse_motion(void)
{
    encoder_capture_t result;
    reset_capture(0U, 0U);
    move_to(1U, 0U, 0);
    late_edge_injection = 1;
    encoder_capture_irq();
    encoder_capture_read(&result);
    assert(result.left.count == 1); /* The sampled first forward edge only. */
    encoder_capture_irq();
    encoder_capture_read(&result);
    assert(result.left.count == 1 && result.left_overrun == 1U);
    assert(result.left.ab == 2U);
}

static void test_edges_during_gpio_sample_are_flagged(void)
{
    encoder_capture_t result;
    reset_capture(0U, 0U);
    move_to(1U, 0U, 0);
    late_edge_injection = 2;
    encoder_capture_irq();
    encoder_capture_read(&result);
    assert(result.left.count == 0 && result.left_overrun == 1U);
    assert(result.left.ab == 2U);
}

int main(void)
{
    test_cycles_between_task_reads();
    test_initial_state_and_independent_wheels();
    test_unrelated_events_preserved();
    test_coalesced_edges_are_not_invented_counts();
    test_diagonal_rejection();
    test_edges_after_ack_do_not_invent_reverse_motion();
    test_edges_during_gpio_sample_are_flagged();
    assert(lock_depth == 0U);
    puts("PASS: 7 interrupt capture test groups, including late-edge interleavings");
    return 0;
}
