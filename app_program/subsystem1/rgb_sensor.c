#include "rgb_sensor.h"
#include <stddef.h>

enum {RGB_PHASE_INIT, RGB_PHASE_WARM, RGB_PHASE_ARM,
       RGB_PHASE_CONVERT, RGB_PHASE_RETRY};

static rgb_status_t write_register(const rgb_bus_t *p_bus, uint8_t reg,
                                    uint8_t value)
{
    const uint8_t data[2] = {(uint8_t)(0x80U | reg), value};
    return p_bus->transfer(p_bus->p_context, RGB_ADDRESS, data, 2U, NULL, 0U);
}

static rgb_status_t read_register(const rgb_bus_t *p_bus, uint8_t reg,
                                   uint8_t *p_value)
{
    uint8_t command = (uint8_t)(0x80U | reg);
    return p_bus->transfer(p_bus->p_context, RGB_ADDRESS, &command, 1U,
                            p_value, 1U);
}

static void fail(rgb_sensor_t *p_sensor, const rgb_bus_t *p_bus,
                  rgb_status_t status)
{
    p_sensor->latest.ready = false;
    p_sensor->latest.valid = false;
    p_sensor->latest.status = status;
    if (p_sensor->latest.errors != UINT32_MAX)
    {
        ++p_sensor->latest.errors;
    }
    p_sensor->phase = RGB_PHASE_RETRY;
    p_sensor->phase_started_ms = p_bus->now_ms(p_bus->p_context);
}

static rgb_status_t configure(rgb_sensor_t *p_sensor, const rgb_bus_t *p_bus)
{
    rgb_status_t result = p_bus->open(p_bus->p_context);
    if (result == RGB_OK)
    {
        result = read_register(p_bus, 0x12U, &p_sensor->latest.device_id);
    }
    if (result != RGB_OK)
    {
        return result;
    }
    if (p_sensor->latest.device_id != RGB_DEVICE_ID)
    {
        return RGB_WRONG_ID;
    }
    const uint8_t registers[3] = {0U, 1U, 0x0FU};
    const uint8_t values[3] = {0U, RGB_ATIME, RGB_GAIN_CODE};
    for (uint8_t i = 0U; i < 3U; ++i)
    {
        uint8_t actual = 0U;
        result = write_register(p_bus, registers[i], values[i]);
        if (result == RGB_OK)
        {
            result = read_register(p_bus, registers[i], &actual);
        }
        if (result != RGB_OK)
        {
            return result;
        }
        if (actual != values[i])
        {
            return RGB_BAD_CONFIG;
        }
    }
    return write_register(p_bus, 0U, 1U);
}

void rgb_sensor_init(rgb_sensor_t *p_sensor)
{
    if (p_sensor != NULL)
    {
        *p_sensor = (rgb_sensor_t){.latest = {.status = RGB_STARTING}};
    }
}

static uint16_t decode_word(const uint8_t *p_bytes)
{
    return (uint16_t)((uint16_t)p_bytes[0] | ((uint16_t)p_bytes[1] << 8U));
}

void rgb_sensor_step(rgb_sensor_t *p_sensor, const rgb_bus_t *p_bus)
{
    if (p_sensor == NULL)
    {
        return;
    }
    if ((p_bus == NULL) || (p_bus->open == NULL) ||
        (p_bus->transfer == NULL) || (p_bus->now_ms == NULL))
    {
        p_sensor->latest.ready = false;
        p_sensor->latest.valid = false;
        p_sensor->latest.status = RGB_BAD_CONFIG;
        return;
    }
    uint32_t elapsed = p_bus->now_ms(p_bus->p_context) - p_sensor->phase_started_ms;
    if (p_sensor->phase == RGB_PHASE_RETRY)
    {
        if (elapsed < RGB_RETRY_MS)
        {
            return;
        }
        p_sensor->phase = RGB_PHASE_INIT;
    }
    if (p_sensor->phase == RGB_PHASE_INIT)
    {
        rgb_status_t result = configure(p_sensor, p_bus);
        if (result != RGB_OK)
        {
            fail(p_sensor, p_bus, result);
            return;
        }
        p_sensor->latest.status = RGB_STARTING;
        p_sensor->phase = RGB_PHASE_WARM;
        p_sensor->phase_started_ms = p_bus->now_ms(p_bus->p_context);
        return;
    }
    if ((p_sensor->phase == RGB_PHASE_WARM) &&
        (elapsed < RGB_OSCILLATOR_WAIT_MS))
    {
        return;
    }
    if ((p_sensor->phase == RGB_PHASE_WARM) ||
        (p_sensor->phase == RGB_PHASE_ARM))
    {
        p_sensor->conversion_window_start_ms = p_bus->now_ms(p_bus->p_context);
        rgb_status_t result = write_register(p_bus, 0U, 3U);
        if (result != RGB_OK)
        {
            fail(p_sensor, p_bus, result);
            return;
        }
        p_sensor->latest.ready = true;
        p_sensor->phase = RGB_PHASE_CONVERT;
        p_sensor->phase_started_ms = p_bus->now_ms(p_bus->p_context);
        return;
    }
    if ((p_sensor->phase != RGB_PHASE_CONVERT) ||
        (elapsed < RGB_CONVERSION_WAIT_MS))
    {
        return;
    }
    uint8_t status = 0U;
    rgb_status_t result = read_register(p_bus, 0x13U, &status);
    if (result != RGB_OK)
    {
        fail(p_sensor, p_bus, result);
        return;
    }
    if ((status & 1U) == 0U)
    {
        if (elapsed >= 200U)
        {
            fail(p_sensor, p_bus, RGB_TIMEOUT);
        }
        return;
    }
    uint8_t command = 0xB4U; /* COMMAND + auto increment + CDATAL. */
    uint8_t bytes[8] = {0};
    /* Freeze acquisition before the burst: per-word shadowing alone does not
     * guarantee all four channels belong to one cycle if this task is delayed. */
    result = write_register(p_bus, 0U, 1U);
    uint32_t frozen_ms = p_bus->now_ms(p_bus->p_context);
    if (result == RGB_OK)
    {
        result = p_bus->transfer(p_bus->p_context, RGB_ADDRESS, &command, 1U,
                                  bytes, sizeof(bytes));
    }
    if (result != RGB_OK)
    {
        fail(p_sensor, p_bus, result);
        return;
    }
    /* Commit only after all eight bytes AND cycle-stop succeed. */
    p_sensor->latest.clear = decode_word(&bytes[0]);
    p_sensor->latest.red = decode_word(&bytes[2]);
    p_sensor->latest.green = decode_word(&bytes[4]);
    p_sensor->latest.blue = decode_word(&bytes[6]);
    p_sensor->latest.saturated =
        (p_sensor->latest.clear >= RGB_SATURATION_COUNT) ||
        (p_sensor->latest.red >= RGB_SATURATION_COUNT) ||
        (p_sensor->latest.green >= RGB_SATURATION_COUNT) ||
        (p_sensor->latest.blue >= RGB_SATURATION_COUNT);
    p_sensor->latest.has_sample = true;
    p_sensor->latest.valid = !p_sensor->latest.saturated;
    p_sensor->latest.status = p_sensor->latest.saturated ? RGB_SATURATED : RGB_OK;
    /* Timestamp the freeze, not a potentially delayed retrieval of old data. */
    p_sensor->latest.sampled_ms = frozen_ms;
    p_sensor->latest.window_start_ms = p_sensor->conversion_window_start_ms;
    p_sensor->latest.window_end_ms = frozen_ms + RGB_CLOCK_QUANTUM_MS;
    ++p_sensor->latest.sequence;
    p_sensor->phase = RGB_PHASE_ARM;
}

bool rgb_sensor_snapshot(const rgb_sensor_t *p_sensor, uint32_t now_ms,
                          rgb_snapshot_t *p_snapshot)
{
    if ((p_sensor == NULL) || (p_snapshot == NULL))
    {
        return false;
    }
    *p_snapshot = p_sensor->latest;
    p_snapshot->valid = p_snapshot->valid && p_snapshot->has_sample &&
        ((uint32_t)(now_ms - p_snapshot->sampled_ms) <= RGB_FRESH_MS);
    return true;
}
