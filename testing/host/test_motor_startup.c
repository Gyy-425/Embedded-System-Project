#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "motor_io.h"

/* Model reset pulldowns: a non-driven pad is low. Inspect every actual driver operation. */
static uint32_t output_enable;
static uint32_t latch;
static uint32_t mux_sio;
static uint32_t enable_operations;

static void assert_no_drive(void)
{
    uint32_t levels = output_enable & latch & mux_sio;
    uint32_t left = (levels >> 8U) & 3U;
    uint32_t right = (levels >> 10U) & 3U;
    assert((left == 0U) || (left == 3U));
    assert((right == 0U) || (right == 3U));
}

void motor_io_disable_outputs(uint32_t mask)
{
    output_enable &= ~mask;
    assert_no_drive();
}
void motor_io_latch_high(uint32_t mask)
{
    latch |= mask;
    assert_no_drive();
}
void motor_io_configure_sio(uint32_t pin)
{
    mux_sio |= UINT32_C(1) << pin;
    assert_no_drive();
}
void motor_io_enable_outputs(uint32_t mask)
{
    output_enable |= mask;
    ++enable_operations;
    assert_no_drive();
}

int main(void)
{
    const uint32_t unrelated_pin = UINT32_C(1) << 18U;
    output_enable = unrelated_pin;
    latch = unrelated_pin;
    mux_sio = unrelated_pin;
    drv_motor_coast_init();
    assert((output_enable & latch & mux_sio & MOTOR_OUTPUT_MASK) == MOTOR_OUTPUT_MASK);
    assert(enable_operations == 1U);
    assert((output_enable & latch & mux_sio & unrelated_pin) != 0U);
    /* Reinitializing an already-coasting bridge must also remain non-driving. */
    enable_operations = 0U;
    drv_motor_coast_init();
    assert(enable_operations == 1U);
    puts("PASS: motor startup stays non-driving after every operation");
    return 0;
}
