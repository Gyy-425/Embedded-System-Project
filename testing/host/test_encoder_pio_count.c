#include "encoder_pio_count.h"
#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>

int main(void)
{
    encoder_pio_count_t count;
    encoder_pio_count_init(&count, 0U);
    assert(encoder_pio_count_update(&count, UINT32_MAX));
    assert(count.count == -1 && !count.saturated);
    assert(encoder_pio_count_update(&count, 0U));
    assert(count.count == 0);
    assert(encoder_pio_count_update(&count, 10000U));
    assert(count.count == 10000);
    assert(encoder_pio_count_update(&count, 0U));
    assert(count.count == 0);

    encoder_pio_count_init(&count, UINT32_MAX - 2U);
    assert(encoder_pio_count_update(&count, 2U));
    assert(count.count == 5);
    assert(encoder_pio_count_update(&count, UINT32_MAX - 2U));
    assert(count.count == 0);

    encoder_pio_count_init(&count, 10U);
    count.count = INT32_MAX - 1;
    assert(encoder_pio_count_update(&count, 13U));
    assert(count.count == INT32_MAX && count.saturated);
    assert(encoder_pio_count_update(&count, 12U));
    assert(count.count == INT32_MAX - 1 && count.saturated);
    encoder_pio_count_init(&count, 10U);
    count.count = INT32_MIN + 1;
    assert(encoder_pio_count_update(&count, 7U));
    assert(count.count == INT32_MIN && count.saturated);

    encoder_pio_count_init(&count, 0U);
    assert(!encoder_pio_count_update(&count, UINT32_C(0x80000000)));
    assert(count.count == 0 && count.previous == 0U && !count.saturated);
    assert(encoder_pio_count_update(&count, UINT32_C(0x7fffffff)));
    assert(count.count == INT32_MAX && !count.saturated);
    encoder_pio_count_init(&count, 0U);
    assert(encoder_pio_count_update(&count, UINT32_C(0x80000001)));
    assert(count.count == -INT32_MAX && !count.saturated);
    assert(!encoder_pio_count_update(NULL, 0U));
    encoder_pio_count_init(NULL, 0U);
    puts("PASS: PIO modular deltas, reversals, rails, half-range and null guards");
    return 0;
}
