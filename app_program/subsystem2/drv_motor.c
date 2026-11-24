#include "motor_io.h"

void drv_motor_coast_init(void)
{
    /* Keep both bridges undriven while muxes/pads are configured. */
    motor_io_disable_outputs(MOTOR_OUTPUT_MASK);
    motor_io_latch_high(MOTOR_OUTPUT_MASK);
    for (uint32_t pin = 8U; pin <= 11U; ++pin)
    {
        motor_io_configure_sio(pin);
    }
    /* One masked SIO write enables both A/B pairs together in coast (11). */
    motor_io_enable_outputs(MOTOR_OUTPUT_MASK);
}
