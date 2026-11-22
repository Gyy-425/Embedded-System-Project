#include "rgb_i2c_rp2040.h"
#include "hardware/i2c.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

i2c_inst_t g_test_i2c1 = {1U};
static uint32_t g_pins;
static uint32_t g_pulls;
static uint32_t g_resets;
static uint32_t g_reads;
static uint32_t g_deadlines;
static int g_write_result;
static int g_read_result;
static bool g_expect_read;
static absolute_time_t g_until;

uint32_t app_now_ms(void);
uint32_t app_now_ms(void) { return 123U; }
absolute_time_t make_timeout_time_us(uint64_t us)
{
    assert(us == 5000U);
    ++g_deadlines;
    return 98765U;
}
void gpio_set_function(unsigned int pin, unsigned int function)
{
    assert((pin == 2U || pin == 3U) && function == 3U);
    g_pins |= UINT32_C(1) << pin;
}
void gpio_pull_up(unsigned int pin) {g_pulls |= UINT32_C(1) << pin;}
uint i2c_init(i2c_inst_t *p_i2c, uint baud)
{
    assert(p_i2c == i2c1 && baud == 100000U);
    return baud;
}
void i2c_deinit(i2c_inst_t *p_i2c)
{
    assert(p_i2c == i2c1);
    ++g_resets;
}
int i2c_write_blocking_until(i2c_inst_t *p_i2c, uint8_t address,
                             const uint8_t *p_data, size_t size, bool nostop,
                             absolute_time_t until)
{
    assert(p_i2c == i2c1 && address == 0x29U && p_data != NULL);
    assert(nostop == g_expect_read);
    g_until = until;
    return g_write_result == 100 ? (int)size : g_write_result;
}
int i2c_read_blocking_until(i2c_inst_t *p_i2c, uint8_t address,
                            uint8_t *p_data, size_t size, bool nostop,
                            absolute_time_t until)
{
    assert(p_i2c == i2c1 && address == 0x29U && !nostop);
    assert(until == g_until); /* One deadline for the WHOLE combined transfer. */
    ++g_reads;
    memset(p_data, 0x44, size);
    return g_read_result == 100 ? (int)size : g_read_result;
}

int main(void)
{
    const rgb_bus_t *p_bus = rgb_rp2040_bus();
    uint8_t command = 0xB4U;
    uint8_t data[8] = {0};
    assert(p_bus->now_ms(NULL) == 123U);
    assert(p_bus->open(NULL) == RGB_OK);
    g_write_result = 1;
    g_read_result = 100;
    g_expect_read = true;
    uint8_t short_write[2] = {0x80U, 0U};
    uint32_t reads_before = g_reads;
    assert(p_bus->transfer(NULL, 0x29U, short_write, 2U, data, 1U) == RGB_IO_ERROR);
    assert(g_reads == reads_before);
    assert(p_bus->open(NULL) == RGB_OK);
    assert(g_pins == 12U && g_pulls == 12U);
    g_expect_read = true;
    g_write_result = 100;
    g_read_result = 100;
    uint32_t deadlines_before = g_deadlines;
    assert(p_bus->transfer(NULL, 0x29U, &command, 1U, data, 8U) == RGB_OK);
    assert(g_deadlines == deadlines_before + 1U && data[7] == 0x44U);
    for (unsigned int failure = 0U; failure < 4U; ++failure)
    {
        assert(p_bus->open(NULL) == RGB_OK);
        g_write_result = failure == 0U ? -1 : failure == 1U ? -2 : 100;
        g_read_result = failure == 2U ? 7 : failure == 3U ? -2 : 100;
        uint32_t resets = g_resets;
        uint32_t reads = g_reads;
        rgb_status_t expected = failure == 1U || failure == 3U ? RGB_TIMEOUT : RGB_IO_ERROR;
        assert(p_bus->transfer(NULL, 0x29U, &command, 1U, data, 8U) == expected);
        assert(g_resets == resets + 1U);
        if (failure < 2U) {assert(g_reads == reads);}
        assert(p_bus->transfer(NULL, 0x29U, &command, 1U, data, 8U) == RGB_IO_ERROR);
    }
    assert(p_bus->open(NULL) == RGB_OK);
    assert(p_bus->transfer(NULL, 0x29U, NULL, 1U, data, 8U) == RGB_BAD_CONFIG);
    assert(p_bus->transfer(NULL, 0x29U, &command, 1U, NULL, 8U) == RGB_BAD_CONFIG);
    assert(p_bus->transfer(NULL, 0x28U, &command, 1U, data, 8U) == RGB_BAD_CONFIG);
    g_expect_read = false;
    g_write_result = 100;
    uint8_t write[2] = {0x80U, 1U};
    assert(p_bus->transfer(NULL, 0x29U, write, 2U, NULL, 0U) == RGB_OK);
    puts("RGB adapter: pins/shared deadline/NACK/short read/timeout recovery PASS");
    return 0;
}
