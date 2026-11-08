#ifndef AAI05_ENCODER_PIO_IO_H
#define AAI05_ENCODER_PIO_IO_H

#include <stdint.h>

typedef enum
{
    ENCODER_PIO_IO_OK,
    ENCODER_PIO_IO_BUSY,
    ENCODER_PIO_IO_TIMEOUT,
    ENCODER_PIO_IO_STATE,
    ENCODER_PIO_IO_ARGUMENT
} encoder_pio_io_result_t;

typedef struct
{
    uint32_t raw[2];
    uint32_t levels;
    uint8_t invalid_flags; /* Sticky left/right diagonal flags, bits 0/1. */
    uint8_t block;
} encoder_pio_sample_t;

/* Motion task owns both calls. Never called by report/network tasks or ISRs. */
encoder_pio_io_result_t encoder_pio_io_start(void);
encoder_pio_io_result_t encoder_pio_io_read(encoder_pio_sample_t *p_sample);

#endif
