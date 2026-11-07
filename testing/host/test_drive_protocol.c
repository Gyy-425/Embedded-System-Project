#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "drive_protocol.h"
#include "comm_protocol.h"

static void test_decode_and_rejection(void)
{
    uint8_t frame[14] = {0x27U, 0xD9U, 0x4EU, 255U, 255U,
        1U, 2U, 3U, 4U, 0x11U, 0x22U, 0x33U, 0x44U, 0U};
    drive_command_t command;

    frame[13] = comm_protocol_crc(frame, 13U);
    assert(drive_protocol_decode(frame, 14U, &command) == DRIVE_OK);
    assert(command.opcode == DRIVE_OP_START && command.target == 7U);
    assert(command.left_pct == 50 && command.right_pct == -50);
    assert(command.sequence == 65535U && command.session == 0x01020304U);
    assert(command.grant == 0x11223344U);
    assert(drive_protocol_decode(NULL, 14U, &command) == DRIVE_BAD_FRAME);
    assert(drive_protocol_decode(frame, 13U, &command) == DRIVE_BAD_FRAME);
    assert(drive_protocol_decode(frame, 14U, NULL) == DRIVE_BAD_FRAME);
    frame[13] ^= 1U;
    assert(drive_protocol_decode(frame, 14U, &command) == DRIVE_BAD_FRAME);
    frame[0] = 0x37U;
    frame[13] = comm_protocol_crc(frame, 13U);
    assert(drive_protocol_decode(frame, 14U, &command) == DRIVE_BAD_FRAME);
    frame[0] = 0x27U;
    frame[1] = 0xE0U;
    frame[2] = 0U;
    frame[13] = comm_protocol_crc(frame, 13U);
    assert(drive_protocol_decode(frame, 14U, &command) == DRIVE_BAD_VALUE);
    frame[1] = 0xC0U;
    frame[13] = comm_protocol_crc(frame, 13U);
    assert(drive_protocol_decode(frame, 14U, &command) == DRIVE_BAD_VALUE);
    frame[0] = 0x2BU;
    memset(&frame[9], 0, 4U);
    frame[13] = comm_protocol_crc(frame, 13U);
    assert(drive_protocol_decode(frame, 14U, &command) == DRIVE_OK);
    frame[0] = 0x2AU;
    frame[1] = 0U;
    frame[13] = comm_protocol_crc(frame, 13U);
    assert(drive_protocol_decode(frame, 14U, &command) == DRIVE_BAD_VALUE);
}

static void print_vector(const char *p_name, const uint8_t *p_data,
                           uint16_t length)
{
    printf("%s ", p_name);
    for (uint16_t index = 0U; index < length; ++index)
    {
        printf("%02x", p_data[index]);
    }
    putchar('\n');
}

static void test_encoding(void)
{
    drive_status_t status = {.session = 0x01020304U, .grant = 0x11223344U,
        .uptime_ms = UINT32_MAX, .echo = 0x1234U, .mutation_sequence = 65535U,
        .lease_ms = 741U, .left_raw = 0xABCU, .right_raw = 0x123U,
        .left_pct = 50, .right_pct = -50, .opcode = DRIVE_OP_START,
        .result = DRIVE_OK, .target = 7U,
        .flags = DRIVE_FLAG_READY | DRIVE_FLAG_ACTIVE | DRIVE_FLAG_IR_VALID,
        .phase = DRIVE_RUNNING, .motion_phase = 1U, .motion_reason = 0U};
    uint8_t output[26];
    const uint8_t expected[22] = {0xA6U, 0x07U, 0x0AU, 0x12U, 0x34U,
        0xFFU, 0xFFU, 1U, 2U, 3U, 4U, 0x11U, 0x22U, 0x33U, 0x44U,
        0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xECU, 0xA7U, 0x4BU};

    assert(drive_protocol_encode(output, &status, false) == 23U);
    assert(memcmp(output, expected, sizeof(expected)) == 0);
    assert(output[22] == comm_protocol_crc(output, 22U));
    print_vector("DRIVE_ACK", output, 23U);
    assert(drive_protocol_encode(output, &status, true) == 26U);
    assert(output[0] == 0xC0U);
    assert(output[22] == 0xABU && output[23] == 0xC1U && output[24] == 0x23U);
    assert(output[25] == comm_protocol_crc(output, 25U));
    print_vector("DRIVE_TELEMETRY", output, 26U);
    status.left_raw = 4096U;
    assert(drive_protocol_encode(output, &status, true) == 0U);
    status.left_raw = 0U;
    status.flags |= DRIVE_FLAG_IR_STALE;
    assert(drive_protocol_encode(output, &status, true) == 0U);
}

int main(void)
{
    test_decode_and_rejection();
    test_encoding();
    puts("PASS: v2 strict command fields, signed duties, CRC and packed status");
    return 0;
}
