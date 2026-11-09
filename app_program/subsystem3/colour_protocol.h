#ifndef AAI05_COLOUR_PROTOCOL_H
#define AAI05_COLOUR_PROTOCOL_H

#include "../subsystem1/colour_runtime.h"

#define COLOUR_PACKET_SIZE 23U

/* Explicit binary encoding, never a native struct. Zero on bad input. */
uint16_t colour_protocol_encode(uint8_t *p_output, uint32_t session,
                                 const colour_report_t *p_report,
                                 bool is_snapshot);

#endif /* AAI05_COLOUR_PROTOCOL_H */
