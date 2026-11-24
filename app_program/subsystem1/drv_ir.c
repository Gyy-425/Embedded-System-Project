/* Called only by the IR owner task. The existing kernel adca driver owns ADC IRQs. */
#include "drv_ir.h"

static ID adc_descriptor = -1;

ER drv_ir_open(void)
{
    out_w(GPIO_CTRL(APP_IR_LEFT_PIN), GPIO_CTRL_FUNCSEL_NULL);
    clr_w(GPIO(APP_IR_LEFT_PIN), GPIO_IE | GPIO_PUE | GPIO_PDE);
    out_w(GPIO_CTRL(APP_IR_RIGHT_PIN), GPIO_CTRL_FUNCSEL_NULL);
    clr_w(GPIO(APP_IR_RIGHT_PIN), GPIO_IE | GPIO_PUE | GPIO_PDE);
    adc_descriptor = tk_opn_dev((UB *)"adca", TD_READ);
    return (adc_descriptor < E_OK) ? adc_descriptor : E_OK;
}

static ER read_channel(INT channel, uint16_t *sample)
{
    UW raw = 0U;
    SZ actual_size = 0;
    ER error = tk_srea_dev(adc_descriptor, channel, &raw, 1, &actual_size);
    if (error < E_OK)
    {
        return error;
    }
    if ((actual_size != 1) || (raw > 4095U))
    {
        return E_IO;
    }
    *sample = (uint16_t)raw;
    return E_OK;
}

ER drv_ir_read(uint16_t *left, uint16_t *right)
{
    ER error;
    if (adc_descriptor < E_OK)
    {
        return E_NOEXS;
    }
    error = read_channel(0, left);
    if (error < E_OK)
    {
        return error;
    }
    return read_channel(1, right);
}
