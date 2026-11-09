#include "colour_service.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t now;
static bool busy;
static bool queued;
static bool reject;
static unsigned int begins;
static unsigned int publishes;
static uint8_t packet[23];
static colour_report_t source = {.observation = {.sequence = 10U,
    .sampled_ms = 100U, .raw = {123U,456U}, .result = {15U,15U}}};

uint32_t colour_io_now(void) {return now;}
bool colour_io_begin(colour_report_t *p_report)
{
    ++begins;
    if (busy) return false;
    queued = false; *p_report = source; return true;
}
bool colour_io_take(colour_report_t *p_report, bool *p_is_snapshot, bool allow_snapshot)
{
    if (busy || (!queued && !allow_snapshot)) return false;
    *p_is_snapshot = !queued; queued = false; *p_report = source; return true;
}
int32_t colour_io_publish(const uint8_t *p_frame, uint16_t length)
{
    assert(length == 23U);
    ++publishes;
    memcpy(packet, p_frame, length);
    return reject ? -1 : 0;
}

int main(void)
{
    colour_service_poll();
    assert(publishes == 0U && begins == 0U);
    colour_service_link(0x1234U);
    busy = true;
    colour_service_poll();
    assert(begins == 1U && publishes == 0U);
    busy = false;
    queued = true; /* Old session data must be cleared before initial snapshot. */
    reject = true;
    colour_service_poll();
    assert(begins == 2U && !queued && publishes == 1U && packet[0] == 0xD1U);
    uint8_t pending[23]; memcpy(pending, packet, sizeof(pending));
    source.observation.sequence = 11U; queued = true;
    reject = false;
    colour_service_poll();
    assert(publishes == 2U && queued && memcmp(packet, pending, 23U) == 0);
    now = 20U;
    colour_service_poll();
    assert(publishes == 3U && !queued && packet[0] == 0xD0U && packet[8] == 11U);
    now = 1019U; colour_service_poll(); assert(publishes == 3U);
    now = 1020U; colour_service_poll(); assert(publishes == 4U && packet[0] == 0xD1U);
    source.observation.sequence = 12U; queued = true; reject = true;
    colour_service_poll(); assert(publishes == 5U);
    colour_service_link(0U); colour_service_poll(); assert(publishes == 5U);
    source.observation.sequence = 15U; queued = true; reject = false;
    colour_service_link(0x5678U); colour_service_poll();
    assert(!queued && publishes == 6U && packet[0] == 0xD1U && packet[8] == 15U);
    assert(packet[3] == 0x56U && packet[4] == 0x78U);
    /* Heartbeat subtraction is safe across the uint32 clock wrap. */
    now = UINT32_MAX - 100U; colour_service_link(3U); colour_service_poll();
    unsigned int prior = publishes;
    now = 898U; colour_service_poll(); assert(publishes == prior);
    now = 899U; colour_service_poll(); assert(publishes == prior + 1U);
    puts("PASS: bounded reporting, busy lock, retry, heartbeat and session isolation");
    return 0;
}
