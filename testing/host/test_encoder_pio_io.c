/* Real SDK-boundary application code, fake peripherals/SDK claim API only. */
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "pico/time.h"
#include "encoder_pio_io.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

pio_hw_t fixture_pios[2];
static uint64_t clock_us;
static unsigned int configured_pins;
static unsigned int initialized;
static unsigned int seed_instructions;
static unsigned int fail_init;
static bool produce = true;
static bool freeze_clock;

int pio_claim_unused_sm(PIO pio, bool required)
{
    assert(!required);
    for (uint sm = 0U; sm < 4U; ++sm)
    {
        if (!pio->claimed[sm]) {pio->claimed[sm] = true; return (int)sm;}
    }
    return -1;
}
void pio_sm_unclaim(PIO pio, uint sm) {assert(pio->claimed[sm]); pio->claimed[sm] = false;}
int pio_add_program_at_offset(PIO pio, const struct pio_program *program, uint offset)
{
    assert(offset == 0U && program->origin == 0 && program->length == 26U);
    if (pio->program) return -1;
    pio->program = true;
    return 0;
}
void pio_remove_program(PIO pio, const struct pio_program *program, uint offset)
{
    assert(pio->program && program->length == 26U && offset == 0U);
    pio->program = false;
}
void pio_sm_set_enabled(PIO pio, uint sm, bool enabled)
{
    if (enabled) pio->ctrl |= 1U << sm;
    else pio->ctrl &= ~(1U << sm);
}
void pio_set_sm_mask_enabled(PIO pio, uint mask, bool enabled)
{
    assert(mask == 3U);
    if (enabled) pio->ctrl |= mask;
    else pio->ctrl &= ~mask;
}
void pio_sm_set_consecutive_pindirs(PIO pio, uint sm, uint pin, uint count, bool output)
{
    (void)pio;
    assert(sm < 2U && (pin == 4U || pin == 16U) && count == 2U && !output);
}
void pio_gpio_init(PIO pio, uint pin)
{
    (void)pio;
    assert(pin == 4U || pin == 5U || pin == 16U || pin == 17U);
    ++configured_pins;
}
int pio_sm_init(PIO pio, uint sm, uint pc, const pio_sm_config *config)
{
    (void)pio;
    assert(sm < 2U && pc == 15U && config->bottom == 15U && config->top == 23U);
    assert(config->base == (sm == 0U ? 4U : 16U));
    assert(config->divider == 125U && !config->in_right && config->out_right);
    ++initialized;
    return initialized == fail_init ? -1 : 0;
}
void pio_sm_exec(PIO pio, uint sm, uint instruction)
{
    (void)pio;
    assert(sm < 2U && (instruction == 0xe040U || instruction == 0xa0e0U));
    ++seed_instructions;
}
void pio_interrupt_clear(PIO pio, uint irq) {pio->irq &= ~(1U << irq);}
uint pio_sm_get_rx_fifo_level(PIO pio, uint sm) {return pio->level[sm];}
bool pio_sm_is_rx_fifo_empty(PIO pio, uint sm)
{
    if (produce && pio->level[sm] == 0U)
    {
        pio->fifo[sm][0] = pio->live[sm];
        pio->level[sm] = 1U;
    }
    return pio->level[sm] == 0U;
}
uint32_t pio_sm_get(PIO pio, uint sm)
{
    assert(pio->level[sm] > 0U);
    uint32_t value = pio->fifo[sm][0];
    --pio->level[sm];
    memmove(pio->fifo[sm], &pio->fifo[sm][1], pio->level[sm] * sizeof(uint32_t));
    return value;
}
uint pio_get_index(PIO pio) {return pio == pio0 ? 0U : 1U;}
uint32_t gpio_get_all(void) {return (1U << 4U) | (1U << 17U);}
uint64_t time_us_64(void) {if (!freeze_clock) ++clock_us; return clock_us;}
uint32_t clock_get_hz(unsigned int clock) {assert(clock == clk_sys); return 125000000U;}

int main(int argc, char **argv)
{
    assert(argc == 2);
    encoder_pio_sample_t sample = {.raw = {123U, 456U}};
    assert(encoder_pio_io_read(NULL) == ENCODER_PIO_IO_ARGUMENT);
    assert(encoder_pio_io_read(&sample) == ENCODER_PIO_IO_STATE);
    /* Existing radio-like owner in PIO0 must survive all attempts. */
    pio0->claimed[3] = true;
    pio0->ctrl = 8U;
    pio0->program = true;
    if (strcmp(argv[1], "busy") == 0)
    {
        pio1->claimed[2] = true;
        assert(encoder_pio_io_start() == ENCODER_PIO_IO_BUSY);
        assert(pio0->claimed[3] && pio0->ctrl == 8U && pio0->program);
        assert(pio1->claimed[2] && !pio1->claimed[0] && !pio1->claimed[1]);
        assert(configured_pins == 0U);
    }
    else if (strcmp(argv[1], "programbusy") == 0)
    {
        pio1->program = true;
        assert(encoder_pio_io_start() == ENCODER_PIO_IO_BUSY);
        assert(pio1->program && pio1->ctrl == 0U && configured_pins == 0U);
        for (uint i = 0U; i < 4U; ++i) assert(!pio1->claimed[i]);
    }
    else if (strcmp(argv[1], "unclaimedactive") == 0)
    {
        pio1->ctrl = 4U;
        assert(encoder_pio_io_start() == ENCODER_PIO_IO_BUSY);
        assert(pio1->ctrl == 4U && configured_pins == 0U);
        for (uint i = 0U; i < 4U; ++i) assert(!pio1->claimed[i]);
    }
    else if (strcmp(argv[1], "initfail") == 0)
    {
        fail_init = 2U;
        assert(encoder_pio_io_start() == ENCODER_PIO_IO_STATE);
        assert(pio1->ctrl == 0U && !pio1->program);
        for (uint i = 0U; i < 4U; ++i) assert(!pio1->claimed[i]);
        assert(pio0->claimed[3] && pio0->ctrl == 8U && pio0->program);
    }
    else
    {
        assert(encoder_pio_io_start() == ENCODER_PIO_IO_OK);
        assert(pio1->ctrl == 3U && pio1->program && configured_pins == 4U);
        for (uint i = 0U; i < 4U; ++i) assert(pio1->claimed[i]);
        assert(seed_instructions == 4U);
        assert(encoder_pio_io_start() == ENCODER_PIO_IO_BUSY);
        pio1->live[0] = UINT32_MAX - 99U;
        pio1->live[1] = 100U;
        pio1->irq = 2U;
        pio1->level[0] = pio1->level[1] = 4U;
        assert(encoder_pio_io_read(&sample) == ENCODER_PIO_IO_OK);
        assert(sample.raw[0] == UINT32_MAX - 99U && sample.raw[1] == 100U);
        assert(sample.invalid_flags == 2U && sample.block == 1U);
        assert(sample.levels == gpio_get_all());
        clock_us = UINT32_MAX - 2U;
        assert(encoder_pio_io_read(&sample) == ENCODER_PIO_IO_OK);
        if (strcmp(argv[1], "disabled") == 0)
        {
            pio1->ctrl = 1U;
            assert(encoder_pio_io_read(&sample) == ENCODER_PIO_IO_STATE);
        }
        else
        {
            produce = false;
            freeze_clock = strcmp(argv[1], "frozen") == 0;
            sample.raw[0] = 123U;
            assert(encoder_pio_io_read(&sample) == ENCODER_PIO_IO_TIMEOUT);
            assert(sample.raw[0] == 123U); /* No partial publication. */
        }
    }
    puts("PASS: PIO SDK-boundary claims, seeding, fresh reads and bounded failure");
    return 0;
}
