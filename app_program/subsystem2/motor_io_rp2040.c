#include "../app.h"
#include "motor_io.h"

void motor_io_disable_outputs(uint32_t mask)
{
    out_w(GPIO_OE_CLR, mask);
}

void motor_io_latch_high(uint32_t mask)
{
    out_w(GPIO_OUT_SET, mask);
}

void motor_io_configure_sio(uint32_t pin)
{
    /* Unlike gpio_set_pin(...OUT), this deliberately leaves SIO OE unchanged. */
    clr_w(GPIO(pin), GPIO_IE | GPIO_OD);
    out_w(GPIO_CTRL(pin), GPIO_CTRL_FUNCSEL_SIO);
}

void motor_io_enable_outputs(uint32_t mask)
{
    out_w(GPIO_OE_SET, mask);
}
