/* Explicit big-endian binary fields. CRC-8: polynomial07, initial00. */
#include "comm_protocol.h"

#include <stddef.h>

#define COMM_STATE_HAS_SEQUENCE 0x08U
#define COMM_STATE_READY 0x10U
#define COMM_STATE_TX_DROP 0x80U

static uint16_t comm_protocol_read_sequence(const uint8_t *p_data)
{
    return (uint16_t)(((uint32_t)p_data[0] << 8U) | p_data[1]);
}

static uint32_t comm_protocol_read_session(const uint8_t *p_data)
{
    return ((uint32_t)p_data[0] << 24U) |
           ((uint32_t)p_data[1] << 16U) |
           ((uint32_t)p_data[2] << 8U) | p_data[3];
}

static void comm_protocol_write_sequence(uint8_t *p_data, uint16_t sequence)
{
    p_data[0] = (uint8_t)((uint32_t)sequence >> 8U);
    p_data[1] = (uint8_t)sequence;
}

static void comm_protocol_write_word(uint8_t *p_data, uint32_t value)
{
    p_data[0] = (uint8_t)(value >> 24U);
    p_data[1] = (uint8_t)(value >> 16U);
    p_data[2] = (uint8_t)(value >> 8U);
    p_data[3] = (uint8_t)value;
}

uint8_t comm_protocol_crc(const uint8_t *p_data, uint16_t length)
{
    uint32_t crc = 0U;

    if (NULL == p_data)
    {
        return 0U;
    }
    for (uint16_t index = 0U; index < length; ++index)
    {
        crc ^= p_data[index];
        for (uint8_t bit = 0U; bit < 8U; ++bit)
        {
            crc = ((crc & 0x80U) != 0U) ?
                  ((crc << 1U) ^ 0x07U) : (crc << 1U);
            crc &= 0xFFU;
        }
    }
    return (uint8_t)crc;
}

comm_result_t comm_protocol_decode(const uint8_t *p_data, uint16_t length,
                                  comm_command_t *p_command)
{
    uint32_t control;
    uint8_t opcode;
    uint8_t target;

    if ((NULL == p_data) || (NULL == p_command) ||
        (COMM_COMMAND_SIZE != length))
    {
        return COMM_RESULT_BAD_FRAME;
    }
    control = p_data[0];
    if (((control & 0xC1U) != 0U) ||
        (comm_protocol_crc(p_data, 7U) != p_data[7]))
    {
        return COMM_RESULT_BAD_FRAME;
    }
    opcode = (uint8_t)((control >> 4U) & 0x03U);
    target = (uint8_t)((control >> 1U) & 0x07U);
    if (opcode > COMM_OP_CLEAR)
    {
        return COMM_RESULT_BAD_FRAME;
    }
    if (((COMM_OP_TARGET == opcode) && (target >= COMM_COLOUR_UNKNOWN)) ||
        ((COMM_OP_TARGET != opcode) && (COMM_COLOUR_UNKNOWN != target)))
    {
        return COMM_RESULT_BAD_TARGET;
    }
    p_command->control = (uint8_t)control;
    p_command->sequence = comm_protocol_read_sequence(&p_data[1]);
    p_command->session = comm_protocol_read_session(&p_data[3]);
    return COMM_RESULT_OK;
}

void comm_protocol_reset(comm_state_t *p_state, uint32_t session,
                         bool is_ready)
{
    if (NULL != p_state)
    {
        p_state->session = session;
        p_state->sequence = 0U;
        p_state->control = (uint8_t)(COMM_COLOUR_UNKNOWN |
                           (is_ready ? COMM_STATE_READY : 0U));
    }
}

uint8_t comm_protocol_target(const comm_state_t *p_state)
{
    return (NULL == p_state) ? COMM_COLOUR_UNKNOWN :
           (uint8_t)((uint32_t)p_state->control & 0x07U);
}

bool comm_protocol_is_ready(const comm_state_t *p_state)
{
    return (NULL != p_state) &&
           (((uint32_t)p_state->control & COMM_STATE_READY) != 0U);
}

static comm_result_t comm_protocol_check_sequence(const comm_state_t *p_state,
                                    const comm_command_t *p_command)
{
    uint32_t previous = p_state->control;
    uint32_t incoming = p_command->control;
    uint16_t difference;

    if ((previous & COMM_STATE_HAS_SEQUENCE) == 0U)
    {
        return COMM_RESULT_OK;
    }
    difference = (uint16_t)((uint32_t)p_command->sequence -
                           p_state->sequence);
    if (0U == difference)
    {
        bool is_same = (((previous >> 5U) & 0x03U) ==
                        ((incoming >> 4U) & 0x03U)) &&
                       ((previous & 0x07U) == ((incoming >> 1U) & 0x07U));
        return is_same ? COMM_RESULT_DUPLICATE : COMM_RESULT_CONFLICT;
    }
    return (difference < 0x8000U) ? COMM_RESULT_OK : COMM_RESULT_STALE;
}

comm_result_t comm_protocol_apply(comm_state_t *p_state,
                                 const uint8_t *p_data, uint16_t length)
{
    comm_command_t command = {0};
    comm_result_t result = comm_protocol_decode(p_data, length, &command);
    uint32_t control;
    uint8_t opcode;
    uint8_t target;

    if (COMM_RESULT_OK != result)
    {
        return result;
    }
    if (!comm_protocol_is_ready(p_state))
    {
        return COMM_RESULT_NOT_READY;
    }
    control = command.control;
    opcode = (uint8_t)((control >> 4U) & 0x03U);
    if (COMM_OP_STATUS == opcode)
    {
        return COMM_RESULT_OK;
    }
    if (command.session != p_state->session)
    {
        return COMM_RESULT_SESSION;
    }
    result = comm_protocol_check_sequence(p_state, &command);
    if (COMM_RESULT_OK != result)
    {
        return result;
    }
    target = (uint8_t)((control >> 1U) & 0x07U);
    p_state->control = (uint8_t)(((uint32_t)p_state->control & 0x90U) |
                       COMM_STATE_HAS_SEQUENCE | target |
                       ((uint32_t)opcode << 5U));
    p_state->sequence = command.sequence;
    return COMM_RESULT_OK;
}

void comm_protocol_tx_drop(comm_state_t *p_state, bool is_dropped)
{
    if (NULL != p_state)
    {
        uint32_t control = p_state->control;
        p_state->control = (uint8_t)(is_dropped ?
                           (control | COMM_STATE_TX_DROP) :
                           (control & ~COMM_STATE_TX_DROP));
    }
}

bool comm_protocol_has_tx_drop(const comm_state_t *p_state)
{
    return (NULL != p_state) &&
           (((uint32_t)p_state->control & COMM_STATE_TX_DROP) != 0U);
}

void comm_protocol_encode_ack(uint8_t *p_output, const comm_state_t *p_state,
                              uint8_t opcode, comm_result_t result)
{
    uint32_t control;

    if ((NULL == p_output) || (NULL == p_state))
    {
        return;
    }
    control = 0x40U | (((uint32_t)opcode & 0x03U) << 4U) |
              ((uint32_t)comm_protocol_target(p_state) << 1U);
    p_output[0] = (uint8_t)control;
    p_output[1] = (uint8_t)(((uint32_t)result & 0x07U) |
                  ((COMM_CAP_TARGET | COMM_CAP_RAW_IR) << 3U) |
                  (comm_protocol_is_ready(p_state) ? 0x80U : 0U));
    comm_protocol_write_sequence(&p_output[2], p_state->sequence);
    comm_protocol_write_word(&p_output[4], p_state->session);
    p_output[8] = comm_protocol_crc(p_output, 8U);
}

bool comm_protocol_encode_telemetry(uint8_t *p_output,
                                    const comm_sample_t *p_sample)
{
    if ((NULL == p_output) || (NULL == p_sample) ||
        (p_sample->left_raw > 4095U) || (p_sample->right_raw > 4095U))
    {
        return false;
    }
    p_output[0] = (uint8_t)(0x80U |
                  ((uint32_t)p_sample->selection & 0x3FU));
    p_output[1] = p_sample->flags;
    comm_protocol_write_sequence(&p_output[2], p_sample->sequence);
    comm_protocol_write_word(&p_output[4], p_sample->session);
    comm_protocol_write_word(&p_output[8], p_sample->uptime_ms);
    p_output[12] = (uint8_t)((uint32_t)p_sample->left_raw >> 4U);
    p_output[13] = (uint8_t)((((uint32_t)p_sample->left_raw & 0x0FU) << 4U) |
                            ((uint32_t)p_sample->right_raw >> 8U));
    p_output[14] = (uint8_t)p_sample->right_raw;
    p_output[15] = comm_protocol_crc(p_output, 15U);
    return true;
}
