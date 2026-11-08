#if AAI05_ENCODER_PIO
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include "../app.h"
#pragma GCC diagnostic pop
#include "encoder_capture.h"
#include "encoder_pio_count.h"
#include "encoder_pio_io.h"

static encoder_capture_t g_pio_capture;
static encoder_pio_count_t g_pio_count[2];
static bool g_pio_capture_started;

static int32_t encoder_pio_error(encoder_pio_io_result_t result)
{
    switch (result)
    {
        case ENCODER_PIO_IO_OK: return E_OK;
        case ENCODER_PIO_IO_BUSY: return E_BUSY;
        case ENCODER_PIO_IO_TIMEOUT: return E_TMOUT;
        case ENCODER_PIO_IO_ARGUMENT: return E_PAR;
        default: return E_OBJ;
    }
}

void encoder_capture_reset(void)
{
    g_pio_capture = (encoder_capture_t){.pio_block = UINT8_MAX};
    encoder_pio_count_init(&g_pio_count[0], 0U);
    encoder_pio_count_init(&g_pio_count[1], 0U);
}

int32_t encoder_capture_start(void)
{
    if (g_pio_capture_started)
    {
        return E_BUSY;
    }
#if TK_SUPPORT_SMP
    if (tk_get_prc() != 1)
    {
        return E_CTX;
    }
#endif
    encoder_capture_reset();
    g_pio_capture.error = encoder_pio_error(encoder_pio_io_start());
    g_pio_capture_started = g_pio_capture.error == E_OK;
    return g_pio_capture.error;
}

/* No GPIO handler is registered in this profile. */
void encoder_capture_irq(void) { }

void encoder_capture_read(encoder_capture_t *p_snapshot)
{
    if (p_snapshot == NULL)
    {
        return;
    }
    if (!g_pio_capture_started && (g_pio_capture.error == E_OK))
    {
        g_pio_capture.error = E_OBJ;
    }
    if (g_pio_capture.error == E_OK)
    {
        encoder_pio_sample_t sample;
        int32_t error = encoder_pio_error(encoder_pio_io_read(&sample));
        if (error != E_OK)
        {
            g_pio_capture.error = error;
        }
        else
        {
            encoder_pio_count_t left = g_pio_count[0];
            encoder_pio_count_t right = g_pio_count[1];
            g_pio_capture.pio_invalid_flags |= sample.invalid_flags;
            g_pio_capture.pio_block = sample.block;
            if (!encoder_pio_count_update(&left, sample.raw[0]) ||
                !encoder_pio_count_update(&right, sample.raw[1]))
            {
                g_pio_capture.error = E_IO;
            }
            else
            {
                g_pio_count[0] = left;
                g_pio_count[1] = right;
                g_pio_capture.left.count = left.count;
                g_pio_capture.right.count = right.count;
                g_pio_capture.left.count_saturated = left.saturated;
                g_pio_capture.right.count_saturated = right.saturated;
                g_pio_capture.left.ab = (uint8_t)((((sample.levels >> APP_ENCODER_LEFT_A) & 1U) << 1U) |
                                                     ((sample.levels >> APP_ENCODER_LEFT_B) & 1U));
                g_pio_capture.right.ab = (uint8_t)((((sample.levels >> APP_ENCODER_RIGHT_A) & 1U) << 1U) |
                                                      ((sample.levels >> APP_ENCODER_RIGHT_B) & 1U));
            }
        }
    }
    *p_snapshot = g_pio_capture;
}
#endif
