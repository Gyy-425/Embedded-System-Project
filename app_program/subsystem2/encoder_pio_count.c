#include "encoder_pio_count.h"
#include <limits.h>
#include <stddef.h>

void encoder_pio_count_init(encoder_pio_count_t *p_count, uint32_t raw)
{
    if (p_count != NULL)
    {
        *p_count = (encoder_pio_count_t){.previous = raw};
    }
}

bool encoder_pio_count_update(encoder_pio_count_t *p_count, uint32_t raw)
{
    if (p_count == NULL)
    {
        return false;
    }
    uint32_t difference = raw - p_count->previous;
    if (difference == UINT32_C(0x80000000))
    {
        return false;
    }
    int64_t delta = (int64_t)difference;
    if (difference > UINT32_C(0x7fffffff))
    {
        delta -= INT64_C(4294967296);
    }
    int64_t total = (int64_t)p_count->count + delta;
    if (total > INT32_MAX)
    {
        total = INT32_MAX;
        p_count->saturated = true;
    }
    else if (total < INT32_MIN)
    {
        total = INT32_MIN;
        p_count->saturated = true;
    }
    p_count->count = (int32_t)total;
    p_count->previous = raw;
    return true;
}
