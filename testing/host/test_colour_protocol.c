#include "colour_protocol.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    colour_report_t report = {
        .observation = {.sequence = UINT32_C(0x05060708),
                        .sampled_ms = UINT32_C(0x090A0B0C),
                        .raw = {0xABCU, 0x123U}, .result = {2U, 15U}},
        .dropped = 2U};
    uint8_t encoded[COLOUR_PACKET_SIZE];
    assert(colour_protocol_encode(encoded, UINT32_C(0x01020304), &report, false) == 23U);
    const uint8_t expected[] = {0xD0U,1U,2U,3U,4U,5U,6U,7U,8U,9U,10U,11U,12U,
                                0U,0U,0U,2U,0xABU,0xC1U,0x23U,2U,15U,0x5CU};
    assert(memcmp(encoded, expected, sizeof(expected)) == 0);
    for (unsigned int index = 0U; index < sizeof(encoded); ++index)
    {
        printf("%02x", (unsigned int)encoded[index]);
    }
    puts("");
    assert(colour_protocol_encode(encoded, 1U, &report, true) == 23U && encoded[0] == 0xD1U);
    memset(encoded, 0xA5, sizeof(encoded));
    assert(colour_protocol_encode(encoded, 0U, &report, false) == 0U);
    assert(encoded[0] == 0xA5U);
    report.observation.raw[0] = 4096U;
    assert(colour_protocol_encode(encoded, 1U, &report, false) == 0U);
    report.observation.raw[0] = 1U;
    const uint8_t invalid[] = {7U,8U,16U,0x40U,0xFFU};
    for (unsigned int index = 0U; index < sizeof(invalid); ++index)
    {
        report.observation.result[1] = invalid[index];
        assert(colour_protocol_encode(encoded, 1U, &report, false) == 0U);
    }
    report.observation.result[1] = 63U;
    assert(colour_protocol_encode(encoded, 1U, &report, false) == 23U);
    assert(colour_protocol_encode(NULL, 1U, &report, false) == 0U);
    assert(colour_protocol_encode(encoded, 1U, NULL, false) == 0U);
    puts("PASS: exact colour wire frame and invalid-input rejection");
    return 0;
}
