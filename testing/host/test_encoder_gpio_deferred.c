#include "encoder_capture.h"
#include <assert.h>
#include <stdio.h>

uint32_t encoder_io_lock(void) {return 0U;}
void encoder_io_unlock(uint32_t saved) {(void)saved;}
uint32_t encoder_io_levels(void) {return 0U;}
static uint32_t pending;
uint32_t encoder_io_pending(unsigned int bank) {return bank == ENCODER_LEFT_BANK ? pending : 0U;}
void encoder_io_ack(unsigned int bank, uint32_t edges) {(void)bank; pending &= ~edges;}

int main(void)
{
    encoder_capture_t capture;
    encoder_capture_reset();
    /* 1000 complete cycles while the CPU cannot service GPIO. Only one
     * rising/falling bit per channel remains; their order is irretrievable. */
    for (unsigned int cycle = 0U; cycle < 1000U; ++cycle) pending |= ENCODER_LEFT_EDGES;
    encoder_capture_irq();
    encoder_capture_read(&capture);
    printf("GPIO deferred reference4000, observed%d, ambiguous batches%u\n",
           capture.left.count, capture.left_overrun);
    fflush(stdout);
    /* Characterize the retained legacy backend; PIO tests require all4000. */
    assert(capture.left.count == 0 && capture.left_overrun == 1U);
    return 0;
}
