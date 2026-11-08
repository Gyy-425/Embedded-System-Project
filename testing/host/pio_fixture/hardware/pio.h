#ifndef AAI05_PIO_FIXTURE_H
#define AAI05_PIO_FIXTURE_H
#include <stdbool.h>
#include <stdint.h>
#define PICO_PIO_VERSION 0
typedef unsigned int uint;
typedef struct
{
    uint32_t ctrl;
    uint32_t irq;
    uint32_t inte0;
    uint32_t inte1;
    uint32_t fifo[4][4];
    uint32_t live[4];
    unsigned int level[4];
    bool claimed[4];
    bool program;
} pio_hw_t;
typedef pio_hw_t *PIO;
extern pio_hw_t fixture_pios[2];
#define pio0 (&fixture_pios[0])
#define pio1 (&fixture_pios[1])
struct pio_program
{
    const uint16_t *instructions;
    uint8_t length;
    int8_t origin;
    uint8_t pio_version;
};
typedef struct
{
    uint base;
    uint bottom;
    uint top;
    uint divider;
    bool in_right;
    bool out_right;
} pio_sm_config;
enum {PIO_FIFO_JOIN_NONE = 0, pio_pins = 0, pio_y = 2, pio_osr = 7};
int pio_claim_unused_sm(PIO pio, bool required);
void pio_sm_unclaim(PIO pio, uint sm);
int pio_add_program_at_offset(PIO pio, const struct pio_program *program, uint offset);
void pio_remove_program(PIO pio, const struct pio_program *program, uint offset);
void pio_sm_set_enabled(PIO pio, uint sm, bool enabled);
void pio_set_sm_mask_enabled(PIO pio, uint mask, bool enabled);
void pio_sm_set_consecutive_pindirs(PIO pio, uint sm, uint pin, uint count, bool output);
void pio_gpio_init(PIO pio, uint pin);
int pio_sm_init(PIO pio, uint sm, uint pc, const pio_sm_config *config);
void pio_sm_exec(PIO pio, uint sm, uint instruction);
void pio_interrupt_clear(PIO pio, uint irq);
uint pio_sm_get_rx_fifo_level(PIO pio, uint sm);
bool pio_sm_is_rx_fifo_empty(PIO pio, uint sm);
uint32_t pio_sm_get(PIO pio, uint sm);
uint pio_get_index(PIO pio);
static inline pio_sm_config pio_get_default_sm_config(void)
{
    return (pio_sm_config){0};
}
static inline void sm_config_set_wrap(pio_sm_config *config, uint bottom, uint top)
{
    config->bottom = bottom;
    config->top = top;
}
static inline void sm_config_set_in_pins(pio_sm_config *config, uint pin) {config->base = pin;}
static inline void sm_config_set_in_shift(pio_sm_config *config, bool right, bool automatic, uint threshold)
{
    (void)automatic;
    (void)threshold;
    config->in_right = right;
}
static inline void sm_config_set_out_shift(pio_sm_config *config, bool right, bool automatic, uint threshold)
{
    (void)automatic;
    (void)threshold;
    config->out_right = right;
}
static inline void sm_config_set_fifo_join(pio_sm_config *config, uint join) {(void)config; (void)join;}
static inline void sm_config_set_clkdiv_int_frac8(pio_sm_config *config, uint16_t integer, uint8_t fraction)
{
    (void)fraction;
    config->divider = integer;
}
static inline uint pio_encode_set(uint destination, uint value) {return 0xe000U | (destination << 5U) | value;}
static inline uint pio_encode_mov(uint destination, uint source) {return 0xa000U | (destination << 5U) | source;}
#endif
