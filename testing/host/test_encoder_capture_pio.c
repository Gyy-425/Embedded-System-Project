#define AAI05_APP_H
#define TK_SUPPORT_SMP 0
#define E_OK 0
#define E_BUSY (-65)
#define E_TMOUT (-50)
#define E_OBJ (-41)
#define E_PAR (-17)
#define E_IO (-57)
#include "encoder_capture.h"
#include "encoder_pio_io.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static encoder_pio_sample_t io_sample;
static encoder_pio_io_result_t io_result;
static unsigned int starts;
static unsigned int reads;
encoder_pio_io_result_t encoder_pio_io_start(void) {++starts; return io_result;}
encoder_pio_io_result_t encoder_pio_io_read(encoder_pio_sample_t *p_sample)
{
    ++reads;
    if (io_result == ENCODER_PIO_IO_OK) *p_sample = io_sample;
    return io_result;
}
#include "../../app_program/subsystem2/encoder_capture_pio.c"

int main(int argc, char **argv)
{
    assert(argc == 2);
    encoder_capture_t capture;
    if (strcmp(argv[1], "startfail") == 0)
    {
        io_result = ENCODER_PIO_IO_BUSY;
        assert(encoder_capture_start() == E_BUSY);
        assert(starts == 1U && reads == 0U);
    }
    else
    {
        assert(encoder_capture_start() == E_OK);
        assert(encoder_capture_start() == E_BUSY);
        io_sample = (encoder_pio_sample_t){.raw = {UINT32_MAX - 9U, 10U},
                    .levels = (1U << APP_ENCODER_LEFT_A) | (1U << APP_ENCODER_RIGHT_B),
                    .invalid_flags = 2U, .block = 1U};
        encoder_capture_read(&capture);
        assert(capture.left.count == -10 && capture.right.count == 10);
        assert(capture.left.ab == 2U && capture.right.ab == 1U);
        assert(capture.error == E_OK && capture.pio_invalid_flags == 2U && capture.pio_block == 1U);
        assert(capture.irq_count == 0U && capture.left_overrun == 0U && capture.right_overrun == 0U);
        if (strcmp(argv[1], "half") == 0)
        {
            io_sample.raw[0] += UINT32_C(0x80000000);
            encoder_capture_read(&capture);
            assert(capture.error == E_IO && capture.left.count == -10);
        }
        else
        {
            io_result = ENCODER_PIO_IO_TIMEOUT;
            encoder_capture_read(&capture);
            assert(capture.error == E_TMOUT && capture.left.count == -10 && capture.right.count == 10);
        }
        unsigned int old_reads = reads;
        io_result = ENCODER_PIO_IO_OK;
        encoder_capture_read(&capture);
        assert(capture.error != E_OK && reads == old_reads); /* Fault stays latched. */
    }
    puts("PASS: PIO capture adapter, raw signs, explicit diagnostics and latched faults");
    return 0;
}
