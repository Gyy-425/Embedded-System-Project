#ifndef AAI05_ENCODER_LOGIC_H
#define AAI05_ENCODER_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int32_t count;
    uint32_t invalid_transitions;
    uint8_t ab;
    bool count_saturated;
} encoder_state_t;

void encoder_init(encoder_state_t *state, uint8_t ab);
void encoder_update(encoder_state_t *state, uint8_t ab);

#endif
