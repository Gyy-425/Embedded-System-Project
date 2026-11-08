#ifndef AAI05_ENCODER_CAPTURE_H
#define AAI05_ENCODER_CAPTURE_H

#include "encoder_logic.h"

#ifndef AAI05_ENCODER_PIO
#define AAI05_ENCODER_PIO 0
#endif

#define APP_ENCODER_LEFT_A  4U
#define APP_ENCODER_LEFT_B  5U
#define APP_ENCODER_RIGHT_A 16U
#define APP_ENCODER_RIGHT_B 17U
#define ENCODER_LEFT_BANK  (APP_ENCODER_LEFT_A / 8U)
#define ENCODER_RIGHT_BANK (APP_ENCODER_RIGHT_A / 8U)
#define ENCODER_EDGE_BITS(pin) (UINT32_C(12) << (((pin) % 8U) * 4U))
#define ENCODER_LEFT_EDGES (ENCODER_EDGE_BITS(APP_ENCODER_LEFT_A) | ENCODER_EDGE_BITS(APP_ENCODER_LEFT_B))
#define ENCODER_RIGHT_EDGES (ENCODER_EDGE_BITS(APP_ENCODER_RIGHT_A) | ENCODER_EDGE_BITS(APP_ENCODER_RIGHT_B))

typedef struct {
    encoder_state_t left;
    encoder_state_t right;
    uint32_t irq_count;
    uint32_t left_overrun;
    uint32_t right_overrun;
    int32_t error; /* Latched runtime error; zero in the legacy IRQ backend. */
    uint8_t pio_invalid_flags; /* Bits 0/1; inapplicable to legacy GPIO capture. */
    uint8_t pio_block;
} encoder_capture_t;

int32_t encoder_capture_start(void);
/* Reset only during initialization. PIO calls are Motion-owner only. */
void encoder_capture_reset(void);
void encoder_capture_irq(void);
void encoder_capture_read(encoder_capture_t *snapshot);

/* Platform hooks: the lock preserves interrupts and excludes the other core. */
uint32_t encoder_io_lock(void);
void encoder_io_unlock(uint32_t saved);
uint32_t encoder_io_levels(void);
uint32_t encoder_io_pending(unsigned int bank);
void encoder_io_ack(unsigned int bank, uint32_t edges);

#endif
