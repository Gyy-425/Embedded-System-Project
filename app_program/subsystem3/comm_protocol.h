/* Binary communication v1; independent of RTOS/hardware/native layout. */
#ifndef COMM_PROTOCOL_H
#define COMM_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

#define COMM_COMMAND_SIZE 8U
#define COMM_ACK_SIZE 9U
#define COMM_TELEMETRY_SIZE 16U
#define COMM_COLOUR_UNKNOWN 7U

#define COMM_OP_STATUS 0U
#define COMM_OP_TARGET 1U
#define COMM_OP_CLEAR 2U

#define COMM_RESULT_OK 0U
#define COMM_RESULT_DUPLICATE 1U
#define COMM_RESULT_BAD_FRAME 2U
#define COMM_RESULT_BAD_TARGET 3U
#define COMM_RESULT_SESSION 4U
#define COMM_RESULT_STALE 5U
#define COMM_RESULT_CONFLICT 6U
#define COMM_RESULT_NOT_READY 7U

#define COMM_CAP_TARGET 0x01U
#define COMM_CAP_RAW_IR 0x02U
#define COMM_FLAG_IR_VALID 0x10U
#define COMM_FLAG_IR_STALE 0x20U
#define COMM_FLAG_TX_DROP 0x40U
#define COMM_FLAG_READY 0x80U

typedef uint8_t comm_result_t;

/* Decoded arguments are temporary; all persistent flags share control. */
typedef struct
{
    uint32_t session;
    uint16_t sequence;
    uint8_t control;
} comm_command_t;

typedef struct
{
    uint32_t session;
    uint16_t sequence;
    uint8_t control;
} comm_state_t;

typedef struct
{
    uint32_t session;
    uint32_t uptime_ms;
    uint16_t sequence;
    uint16_t left_raw;
    uint16_t right_raw;
    uint8_t selection;
    uint8_t flags;
} comm_sample_t;

uint8_t comm_protocol_crc(const uint8_t *p_data, uint16_t length);
comm_result_t comm_protocol_decode(const uint8_t *p_data, uint16_t length,
                                  comm_command_t *p_command);
void comm_protocol_reset(comm_state_t *p_state, uint32_t session,
                         bool is_ready);
comm_result_t comm_protocol_apply(comm_state_t *p_state,
                                 const uint8_t *p_data, uint16_t length);
uint8_t comm_protocol_target(const comm_state_t *p_state);
bool comm_protocol_is_ready(const comm_state_t *p_state);
void comm_protocol_tx_drop(comm_state_t *p_state, bool is_dropped);
bool comm_protocol_has_tx_drop(const comm_state_t *p_state);
void comm_protocol_encode_ack(uint8_t *p_output, const comm_state_t *p_state,
                              uint8_t opcode, comm_result_t result);
bool comm_protocol_encode_telemetry(uint8_t *p_output,
                                    const comm_sample_t *p_sample);

#endif /* COMM_PROTOCOL_H */
