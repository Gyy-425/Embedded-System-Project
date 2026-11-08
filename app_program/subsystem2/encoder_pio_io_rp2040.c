#if AAI05_ENCODER_PIO
#include "encoder_pio_io.h"
#include "encoder_capture.h"
#include <stddef.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#pragma GCC diagnostic pop
#include "encoder_counter.pio.h"

/* Existing micro T-Kernel compatibility layer; no SDK time runtime is linked. */
extern uint64_t time_us_64(void);

#define ENCODER_PIO_ACTIVE_MASK UINT32_C(3)
/* A preempted task may resume later; stay within the existing100 ms lease.
 * Two wheel reads are each bounded by20 ms (normally a few PIO loops). */
#define ENCODER_PIO_READ_US UINT32_C(20000)
#define ENCODER_PIO_MAX_POLLS UINT32_C(16384)

static PIO g_encoder_pio;

static void encoder_pio_release(PIO pio, const uint *p_claims, uint count,
                                bool has_program)
{
    if (has_program)
    {
        pio_set_sm_mask_enabled(pio, ENCODER_PIO_ACTIVE_MASK, false);
        pio_remove_program(pio, &encoder_counter_program, 0U);
    }
    for (uint index = 0U; index < count; ++index)
    {
        pio_sm_unclaim(pio, p_claims[index]);
    }
}

encoder_pio_io_result_t encoder_pio_io_start(void)
{
    const PIO blocks[] = {pio0, pio1};
    const uint pins[] = {APP_ENCODER_LEFT_A, APP_ENCODER_RIGHT_A};
    if (g_encoder_pio != NULL)
    {
        return ENCODER_PIO_IO_BUSY;
    }
    if (clock_get_hz(clk_sys) != UINT32_C(125000000))
    {
        return ENCODER_PIO_IO_STATE;
    }
    for (uint block = 0U; block < 2U; ++block)
    {
        PIO pio = blocks[block];
        uint claims[4];
        uint count = 0U;
        while (count < 4U)
        {
            int sm = pio_claim_unused_sm(pio, false);
            if (sm < 0)
            {
                break;
            }
            claims[count++] = (uint)sm;
        }
        /* Reserve a whole otherwise unused block, including its IRQ flags.
         * Never reset a PIO block or clear another owner's state. */
        if ((count != 4U) || ((pio->ctrl & 15U) != 0U) ||
            ((pio->inte0 | pio->inte1) != 0U))
        {
            encoder_pio_release(pio, claims, count, false);
            continue;
        }
        if (pio_add_program_at_offset(pio, &encoder_counter_program, 0U) < 0)
        {
            encoder_pio_release(pio, claims, count, false);
            continue;
        }
        for (uint sm = 0U; sm < 2U; ++sm)
        {
            pio_sm_config config = encoder_counter_program_get_default_config(0U);
            sm_config_set_in_pins(&config, pins[sm]);
            sm_config_set_in_shift(&config, false, false, 32U);
            sm_config_set_out_shift(&config, true, false, 32U);
            sm_config_set_fifo_join(&config, PIO_FIFO_JOIN_NONE);
            sm_config_set_clkdiv_int_frac8(&config, 125U, 0U);
            pio_gpio_init(pio, pins[sm]);
            pio_gpio_init(pio, pins[sm] + 1U);
            pio_sm_set_consecutive_pindirs(pio, sm, pins[sm], 2U, false);
            if (pio_sm_init(pio, sm, encoder_counter_wrap_target, &config) < 0)
            {
                encoder_pio_release(pio, claims, count, true);
                return ENCODER_PIO_IO_STATE;
            }
            pio_sm_exec(pio, sm, pio_encode_set(pio_y, 0U));
            /* Sample the real startup phase into OSR before the first loop. */
            pio_sm_exec(pio, sm, pio_encode_mov(pio_osr, pio_pins));
            pio_interrupt_clear(pio, sm);
        }
        pio_set_sm_mask_enabled(pio, ENCODER_PIO_ACTIVE_MASK, true);
        g_encoder_pio = pio;
        return ENCODER_PIO_IO_OK;
    }
    return ENCODER_PIO_IO_BUSY;
}

static bool encoder_pio_fresh_count(uint sm, uint32_t *p_raw)
{
    uint remaining = pio_sm_get_rx_fifo_level(g_encoder_pio, sm) + 1U;
    uint32_t started_us = (uint32_t)time_us_64();
    uint32_t polls = 0U;
    /* Drain the entries present at entry, plus one necessarily newer value.
     * Count acquisition keeps running even when the CPU cannot drain RX. */
    while (remaining != 0U)
    {
        if (((uint32_t)time_us_64() - started_us >= ENCODER_PIO_READ_US) ||
            (++polls >= ENCODER_PIO_MAX_POLLS))
        {
            return false;
        }
        if (!pio_sm_is_rx_fifo_empty(g_encoder_pio, sm))
        {
            *p_raw = pio_sm_get(g_encoder_pio, sm);
            --remaining;
        }
    }
    return (uint32_t)time_us_64() - started_us < ENCODER_PIO_READ_US;
}

encoder_pio_io_result_t encoder_pio_io_read(encoder_pio_sample_t *p_sample)
{
    if (p_sample == NULL)
    {
        return ENCODER_PIO_IO_ARGUMENT;
    }
    if ((g_encoder_pio == NULL) ||
        ((g_encoder_pio->ctrl & ENCODER_PIO_ACTIVE_MASK) != ENCODER_PIO_ACTIVE_MASK))
    {
        return ENCODER_PIO_IO_STATE;
    }
    encoder_pio_sample_t sample = {0};
    if (!encoder_pio_fresh_count(0U, &sample.raw[0]) ||
        !encoder_pio_fresh_count(1U, &sample.raw[1]))
    {
        return ENCODER_PIO_IO_TIMEOUT;
    }
    sample.levels = gpio_get_all();
    sample.invalid_flags = (uint8_t)(g_encoder_pio->irq & ENCODER_PIO_ACTIVE_MASK);
    sample.block = (uint8_t)pio_get_index(g_encoder_pio);
    *p_sample = sample;
    return ENCODER_PIO_IO_OK;
}
#endif
