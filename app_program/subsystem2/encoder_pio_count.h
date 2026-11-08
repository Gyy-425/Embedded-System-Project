#ifndef AAI05_ENCODER_PIO_COUNT_H
#define AAI05_ENCODER_PIO_COUNT_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint32_t previous;
    int32_t count;
    bool saturated;
} encoder_pio_count_t;

void encoder_pio_count_init(encoder_pio_count_t *p_count, uint32_t raw);
/* Requires fewer than 2^31 net edges between reads. Half-range is rejected. */
bool encoder_pio_count_update(encoder_pio_count_t *p_count, uint32_t raw);

#endif
