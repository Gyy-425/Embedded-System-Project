/* Synthetic bus fixtures; no tape calibration values. */
#include "rgb_sensor.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    uint32_t now;
    uint32_t calls;
    uint32_t fail_call;
    uint32_t opens;
    rgb_status_t open_status;
    rgb_status_t failure;
    uint8_t registers[32];
    uint8_t commands[128];
    uint32_t pon_ms;
    uint32_t aen_ms;
    uint32_t burst_delay_ms;
    bool quantized_clock;
    bool ignore_atime_write;
} fixture_t;

static rgb_status_t open_bus(void *p_context)
{
    fixture_t *p_fixture = p_context;
    ++p_fixture->opens;
    return p_fixture->open_status;
}

static uint32_t now_ms(void *p_context)
{
    fixture_t *p_fixture = p_context;
    return p_fixture->quantized_clock ?
        p_fixture->now - (p_fixture->now % 10U) : p_fixture->now;
}

static rgb_status_t transfer(void *p_context, uint8_t address,
                              const uint8_t *p_tx, uint8_t tx_size,
                              uint8_t *p_rx, uint8_t rx_size)
{
    fixture_t *p_fixture = p_context;
    assert(address == RGB_ADDRESS);
    assert(p_tx != NULL && (p_tx[0] & 0x80U) != 0U);
    ++p_fixture->calls;
    ++p_fixture->now; /* Bus time must not shorten post-PON waits. */
    if (p_fixture->calls < 128U)
    {
        p_fixture->commands[p_fixture->calls] = p_tx[0];
    }
    if (p_fixture->calls == p_fixture->fail_call)
    {
        return p_fixture->failure;
    }
    uint8_t reg = (uint8_t)(p_tx[0] & 0x1FU);
    if (rx_size != 0U)
    {
        if (rx_size == 8U)
        {
            assert(p_fixture->registers[0] == 1U); /* Freeze all channels first. */
            p_fixture->now += p_fixture->burst_delay_ms;
        }
        assert(tx_size == 1U && p_rx != NULL);
        assert(rx_size == 1U || (p_tx[0] & 0x20U) != 0U);
        assert((size_t)reg + rx_size <= 32U);
        memcpy(p_rx, &p_fixture->registers[reg], rx_size);
    }
    else
    {
        assert(tx_size == 2U);
        if (!(p_fixture->ignore_atime_write && reg == 1U))
        {
            p_fixture->registers[reg] = p_tx[1];
        }
        if (reg == 0U && p_tx[1] == 1U)
        {
            p_fixture->pon_ms = p_fixture->now;
        }
        if (reg == 0U && p_tx[1] == 3U)
        {
            p_fixture->aen_ms = p_fixture->now;
        }
    }
    return RGB_OK;
}

static rgb_bus_t bus(fixture_t *p_fixture)
{
    return (rgb_bus_t){p_fixture, open_bus, transfer, now_ms};
}

static void setup(fixture_t *p_fixture, rgb_sensor_t *p_sensor)
{
    *p_fixture = (fixture_t){0};
    p_fixture->registers[0x12] = RGB_DEVICE_ID;
    rgb_sensor_init(p_sensor);
}

static void arm(fixture_t *p_fixture, rgb_sensor_t *p_sensor)
{
    rgb_bus_t io = bus(p_fixture);
    rgb_sensor_step(p_sensor, &io);
    assert(p_fixture->registers[1] == RGB_ATIME);
    assert(p_fixture->registers[0x0F] == RGB_GAIN_CODE);
    assert(p_fixture->registers[0] == 1U);
    uint32_t calls = p_fixture->calls;
    p_fixture->now = p_fixture->pon_ms + RGB_OSCILLATOR_WAIT_MS - 1U;
    rgb_sensor_step(p_sensor, &io);
    assert(p_fixture->calls == calls);
    p_fixture->now = p_fixture->pon_ms + RGB_OSCILLATOR_WAIT_MS;
    rgb_sensor_step(p_sensor, &io);
    assert(p_fixture->registers[0] == 3U);
    assert(p_sensor->latest.ready);
}

static void sample(fixture_t *p_fixture, rgb_sensor_t *p_sensor)
{
    rgb_bus_t io = bus(p_fixture);
    p_fixture->registers[0x13] = 1U;
    p_fixture->now = p_fixture->aen_ms + RGB_CONVERSION_WAIT_MS;
    rgb_sensor_step(p_sensor, &io);
}

int main(void)
{
    fixture_t f;
    rgb_sensor_t d;
    rgb_snapshot_t s;
    setup(&f, &d);
    rgb_bus_t io = bus(&f);
    assert(rgb_sensor_snapshot(&d, 0U, &s) && !s.valid && !s.has_sample);
    assert(!rgb_sensor_snapshot(NULL, 0U, &s));
    assert(!rgb_sensor_snapshot(&d, 0U, NULL));
    rgb_sensor_step(NULL, &io);
    arm(&f, &d);
    uint32_t calls = f.calls;
    f.now = f.aen_ms + RGB_CONVERSION_WAIT_MS - 1U;
    rgb_sensor_step(&d, &io);
    assert(f.calls == calls && !d.latest.has_sample);
    const uint8_t bytes[8] = {0x34U, 0x12U, 0x78U, 0x06U,
                              0xBCU, 0x0AU, 0xF0U, 0x0EU};
    memcpy(&f.registers[0x14], bytes, 8U);
    sample(&f, &d);
    assert(d.latest.clear == 0x1234U && d.latest.red == 0x0678U);
    assert(d.latest.green == 0x0ABCU && d.latest.blue == 0x0EF0U);
    assert(d.latest.sequence == 1U && d.latest.valid && !d.latest.saturated);
    assert(f.registers[0] == 1U); /* Stop the cycle before publishing it. */
    assert(f.commands[f.calls] == 0xB4U);
    uint32_t sampled = d.latest.sampled_ms;
    assert(d.latest.window_start_ms == f.aen_ms - 1U);
    assert(d.latest.window_end_ms == f.pon_ms + RGB_CLOCK_QUANTUM_MS);
    assert(rgb_sensor_snapshot(&d, sampled + RGB_FRESH_MS, &s) && s.valid);
    assert(rgb_sensor_snapshot(&d, sampled + RGB_FRESH_MS + 1U, &s) && !s.valid);
    rgb_sensor_step(&d, &io); /* Arm a NEW integration. */
    assert(d.latest.sequence == 1U);
    rgb_sensor_step(&d, &io);
    assert(d.latest.sequence == 1U); /* Sticky AVALID cannot duplicate samples. */

    /* Saturation is integration-dependent, not just 65535. Dark is not failure. */
    f.registers[0x14] = 0U;
    f.registers[0x15] = 0x54U;
    sample(&f, &d);
    assert(d.latest.saturated && !d.latest.valid && d.latest.status == RGB_SATURATED);
    rgb_sensor_step(&d, &io);
    memset(&f.registers[0x14], 0, 8U);
    d.latest.sequence = UINT32_MAX;
    sample(&f, &d);
    assert(d.latest.has_sample && d.latest.sequence == 0U && d.latest.valid);

    /* Failed burst/disable must never publish partially read data as new. */
    for (uint32_t offset = 1U; offset <= 3U; ++offset)
    {
        setup(&f, &d);
        arm(&f, &d);
        memcpy(&f.registers[0x14], bytes, 8U);
        sample(&f, &d);
        uint32_t stamp = d.latest.sampled_ms;
        uint32_t start = d.latest.window_start_ms, end = d.latest.window_end_ms;
        rgb_sensor_step(&d, &io);
        f.fail_call = f.calls + offset;
        f.failure = RGB_IO_ERROR;
        sample(&f, &d);
        assert(!d.latest.valid && !d.latest.ready && d.latest.errors == 1U);
        assert(d.latest.sequence == 1U && d.latest.sampled_ms == stamp);
        assert(d.latest.window_start_ms == start && d.latest.window_end_ms == end);
        assert(d.latest.clear == 0x1234U && d.latest.status == RGB_IO_ERROR);
        calls = f.calls;
        f.now = d.phase_started_ms + RGB_RETRY_MS - 1U;
        rgb_sensor_step(&d, &io);
        assert(f.calls == calls);
        f.now = d.phase_started_ms + RGB_RETRY_MS;
        rgb_sensor_step(&d, &io);
        assert(f.opens == 2U);
    }
    setup(&f, &d);
    f.registers[0x12] = 0x4DU;
    rgb_sensor_step(&d, &io);
    assert(d.latest.device_id == 0x4DU && d.latest.status == RGB_WRONG_ID);
    assert(f.registers[0] == 0U && !d.latest.ready);

    for (uint32_t call = 1U; call <= 8U; ++call)
    {
        setup(&f, &d);
        f.fail_call = call;
        f.failure = RGB_IO_ERROR;
        rgb_sensor_step(&d, &io);
        assert(d.latest.status == RGB_IO_ERROR && !d.latest.ready);
        assert(!d.latest.has_sample && d.latest.errors == 1U);
    }
    setup(&f, &d);
    f.ignore_atime_write = true;
    rgb_sensor_step(&d, &io);
    assert(d.latest.status == RGB_BAD_CONFIG && !d.latest.ready);

    setup(&f, &d);
    f.open_status = RGB_TIMEOUT;
    rgb_sensor_step(&d, &io);
    assert(f.calls == 0U && d.latest.status == RGB_TIMEOUT);
    d.latest.errors = UINT32_MAX;
    f.now = d.phase_started_ms + RGB_RETRY_MS;
    rgb_sensor_step(&d, &io);
    assert(d.latest.errors == UINT32_MAX);

    setup(&f, &d);
    arm(&f, &d);
    f.now = f.aen_ms + 200U;
    rgb_sensor_step(&d, &io);
    assert(d.latest.status == RGB_TIMEOUT && !d.latest.has_sample);

    setup(&f, &d);
    f.now = UINT32_MAX - 20U;
    arm(&f, &d);
    sample(&f, &d);
    assert(d.latest.valid);
    d.latest.sampled_ms = UINT32_MAX - 20U;
    assert(rgb_sensor_snapshot(&d, 10U, &s) && s.valid);
    assert(rgb_sensor_snapshot(&d, 300U, &s) && !s.valid);

    setup(&f, &d);
    arm(&f, &d);
    f.burst_delay_ms = RGB_FRESH_MS + 1U;
    sample(&f, &d);
    assert(rgb_sensor_snapshot(&d, f.now, &s) && !s.valid);

    /* Adversarial phase of the RTOS 10ms clock: PON at9ms, AEN at29ms.
     * A10ms clock difference can represent only1ms of actual elapsed time. */
    setup(&f, &d);
    f.quantized_clock = true;
    f.now = 1U;
    rgb_sensor_step(&d, &io);
    assert(f.pon_ms == 9U);
    f.now = 10U;
    calls = f.calls;
    rgb_sensor_step(&d, &io);
    assert(f.calls == calls && f.registers[0] == 1U);
    f.now = 28U;
    rgb_sensor_step(&d, &io);
    assert(f.aen_ms == 29U);
    f.registers[0x13] = 1U; /* Previous cycle's sticky AVALID. */
    f.now = 80U; /* Clock elapsed60, but only51ms since AEN. */
    calls = f.calls;
    rgb_sensor_step(&d, &io);
    assert(f.calls == calls && !d.latest.has_sample);
    f.now = 90U;
    rgb_sensor_step(&d, &io);
    assert(d.latest.has_sample);
    assert(d.latest.window_start_ms == 20U && d.latest.window_end_ms == 100U);
    rgb_sensor_step(&d, NULL);
    assert(d.latest.status == RGB_BAD_CONFIG && !d.latest.valid);
    puts("RGB core: register/timing/data/fault/retry/wrap checks PASS");
    return 0;
}
