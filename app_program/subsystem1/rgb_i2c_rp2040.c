#include "rgb_i2c_rp2040.h"
#ifndef AAI05_RGB_DIAG
#define AAI05_RGB_DIAG 0
#endif
#ifndef AAI05_FOLLOW_APP
#define AAI05_FOLLOW_APP 0
#endif
#if AAI05_RGB_DIAG || AAI05_FOLLOW_APP
#include "../../config/config_device.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#pragma GCC diagnostic pop
#if DEVCNF_USE_IIC
#error RGB diagnostic owns I2C1; inherited I2C device driver must stay disabled
#endif

extern uint32_t app_now_ms(void);
static bool g_opened;

static rgb_status_t open_bus(void *p_context)
{
    (void)p_context;
    (void)i2c_init(i2c1, 100000U);
    gpio_set_function(2U, GPIO_FUNC_I2C);
    gpio_set_function(3U, GPIO_FUNC_I2C);
    gpio_pull_up(2U);
    gpio_pull_up(3U);
    g_opened = true;
    return RGB_OK;
}

static rgb_status_t transfer(void *p_context, uint8_t address,
                              const uint8_t *p_tx, uint8_t tx_size,
                              uint8_t *p_rx, uint8_t rx_size)
{
    (void)p_context;
    if ((address != RGB_ADDRESS) || (p_tx == NULL) || (tx_size == 0U) ||
        (tx_size > 2U) || (rx_size > 8U) ||
        ((rx_size != 0U) && (p_rx == NULL)))
    {
        return RGB_BAD_CONFIG;
    }
    if (!g_opened)
    {
        return RGB_IO_ERROR;
    }
    absolute_time_t until = make_timeout_time_us(5000U);
    int result = i2c_write_blocking_until(i2c1, address, p_tx, tx_size,
                                          rx_size != 0U, until);
    bool complete = result == (int)tx_size;
    if (complete && (rx_size != 0U))
    {
        result = i2c_read_blocking_until(i2c1, address, p_rx, rx_size, false, until);
        complete = result == (int)rx_size;
    }
    if (complete)
    {
        return RGB_OK;
    }
    /* Recover the controller, not the shared timer, GPIO or another I2C bus.
     * This does not promise to clear an external electrical bus fault. */
    i2c_deinit(i2c1);
    g_opened = false;
    return result == PICO_ERROR_TIMEOUT ? RGB_TIMEOUT : RGB_IO_ERROR;
}

static uint32_t now_ms(void *p_context)
{
    (void)p_context;
    return app_now_ms(); /* Same clock domain as IR and the report task. */
}

const rgb_bus_t *rgb_rp2040_bus(void)
{
    static const rgb_bus_t bus = {NULL, open_bus, transfer, now_ms};
    return &bus;
}
#endif
