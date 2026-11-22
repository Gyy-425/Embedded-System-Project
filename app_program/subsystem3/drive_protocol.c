#include "drive_protocol.h"
#include "comm_protocol.h"
#include <stddef.h>

static uint32_t drive_protocol_read_word(const uint8_t *p_data)
{
    return ((uint32_t)p_data[0] << 24U) |
           ((uint32_t)p_data[1] << 16U) |
           ((uint32_t)p_data[2] << 8U) | p_data[3];
}

static void drive_protocol_write_word(uint8_t *p_data, uint32_t value)
{
    p_data[0] = (uint8_t)(value >> 24U);
    p_data[1] = (uint8_t)(value >> 16U);
    p_data[2] = (uint8_t)(value >> 8U);
    p_data[3] = (uint8_t)value;
}

static void drive_protocol_write_short(uint8_t *p_data, uint16_t value)
{
    p_data[0] = (uint8_t)((uint32_t)value >> 8U);
    p_data[1] = (uint8_t)value;
}

static int8_t drive_protocol_signed_seven(uint32_t value)
{
    int32_t signed_value = (int32_t)(value & 0x7FU);
    return (int8_t)((signed_value >= 64) ? signed_value - 128 : signed_value);
}

static bool drive_protocol_valid_duty(int8_t duty)
{
    return (duty >= -50) && (duty <= 50);
}

drive_result_t drive_protocol_decode(const uint8_t *p_data, uint16_t length,
                                      drive_command_t *p_command)
{
    if ((p_data == NULL) || (p_command == NULL) ||
        (length != DRIVE_COMMAND_SIZE) ||
        (comm_protocol_crc(p_data, 13U) != p_data[13]))
    {
        return DRIVE_BAD_FRAME;
    }
    uint32_t control = ((uint32_t)p_data[0] << 16U) |
                       ((uint32_t)p_data[1] << 8U) | p_data[2];
    drive_command_t command = {
        .session = drive_protocol_read_word(&p_data[5]),
        .grant = drive_protocol_read_word(&p_data[9]),
        .sequence = (uint16_t)(((uint32_t)p_data[3] << 8U) | p_data[4]),
        .opcode = (uint8_t)((control >> 17U) & 7U),
        .target = (uint8_t)((control >> 14U) & 7U),
        .left_pct = drive_protocol_signed_seven(control >> 7U),
        .right_pct = drive_protocol_signed_seven(control)};
    if (((control >> 20U) != 2U) || (command.opcode > DRIVE_OP_MAX))
    {
        return DRIVE_BAD_FRAME;
    }
    bool is_start = command.opcode == DRIVE_OP_START;
    bool is_status = command.opcode == DRIVE_OP_STATUS;
    bool is_stop = command.opcode == DRIVE_OP_STOP;
    bool is_neutral = (command.left_pct == 0) && (command.right_pct == 0);
    if (!drive_protocol_valid_duty(command.left_pct) ||
        !drive_protocol_valid_duty(command.right_pct) ||
        (is_start && is_neutral) || (!is_start && !is_neutral) ||
        ((command.opcode == DRIVE_OP_TARGET) ?
         command.target >= DRIVE_UNKNOWN : command.target != DRIVE_UNKNOWN) ||
        (is_status && ((command.session != 0U) || (command.grant != 0U))) ||
        (!is_status && (command.session == 0U)) ||
        (is_stop && (command.grant != 0U)) ||
        (!is_status && !is_stop && (command.grant == 0U)))
    {
        return DRIVE_BAD_VALUE;
    }
    *p_command = command;
    return DRIVE_OK;
}

uint16_t drive_protocol_encode(uint8_t *p_output,
                                const drive_status_t *p_status,
                                bool is_telemetry)
{
    if ((p_output == NULL) || (p_status == NULL) ||
        (p_status->opcode > DRIVE_OP_MAX) ||
        (p_status->result > DRIVE_BUSY) || (p_status->target > DRIVE_UNKNOWN) ||
        (p_status->phase > DRIVE_FAULT) || (p_status->motion_phase > 3U) ||
        (p_status->motion_reason > 4U) || (p_status->flags > 0x1FU) ||
        (p_status->lease_ms > DRIVE_LEASE_MS) ||
        (p_status->left_raw > 4095U) || (p_status->right_raw > 4095U) ||
        !drive_protocol_valid_duty(p_status->left_pct) ||
        !drive_protocol_valid_duty(p_status->right_pct) ||
        ((p_status->flags & (DRIVE_FLAG_IR_VALID | DRIVE_FLAG_IR_STALE)) ==
         (DRIVE_FLAG_IR_VALID | DRIVE_FLAG_IR_STALE)))
    {
        return 0U;
    }
    p_output[0] = is_telemetry ? 0xC0U : (uint8_t)(0xA0U |
        ((uint32_t)p_status->opcode << 1U) |
        ((uint32_t)p_status->result >> 3U));
    p_output[1] = (uint8_t)(p_status->flags | (is_telemetry ? 0U :
        ((uint32_t)(p_status->result & 7U) << 5U)));
    p_output[2] = (uint8_t)(p_status->phase |
        ((uint32_t)p_status->motion_phase << 3U) |
        ((uint32_t)p_status->motion_reason << 5U));
    drive_protocol_write_short(&p_output[3], p_status->echo);
    drive_protocol_write_short(&p_output[5], p_status->mutation_sequence);
    drive_protocol_write_word(&p_output[7], p_status->session);
    drive_protocol_write_word(&p_output[11], p_status->grant);
    drive_protocol_write_word(&p_output[15], p_status->uptime_ms);
    uint32_t packed = ((uint32_t)p_status->target << 21U) |
        (((uint32_t)(uint8_t)p_status->left_pct & 0x7FU) << 14U) |
        (((uint32_t)(uint8_t)p_status->right_pct & 0x7FU) << 7U) |
        (((uint32_t)p_status->lease_ms + 9U) / 10U);
    p_output[19] = (uint8_t)(packed >> 16U);
    p_output[20] = (uint8_t)(packed >> 8U);
    p_output[21] = (uint8_t)packed;
    uint16_t size = DRIVE_ACK_SIZE;
    if (is_telemetry)
    {
        p_output[22] = (uint8_t)((uint32_t)p_status->left_raw >> 4U);
        p_output[23] = (uint8_t)(
            (((uint32_t)p_status->left_raw & 15U) << 4U) |
            ((uint32_t)p_status->right_raw >> 8U));
        p_output[24] = (uint8_t)p_status->right_raw;
        size = DRIVE_TELEMETRY_SIZE;
    }
    p_output[size - 1U] = comm_protocol_crc(p_output, (uint16_t)(size - 1U));
    return size;
}
