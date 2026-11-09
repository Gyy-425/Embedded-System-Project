#include "colour_service.h"

static uint32_t g_colour_session;
static uint32_t g_colour_published_ms;
static colour_report_t g_colour_pending;
static bool g_colour_needs_begin;
static bool g_colour_has_pending;
static bool g_colour_is_snapshot;

void colour_service_link(uint32_t session)
{
    g_colour_session = session;
    g_colour_needs_begin = (session != 0U);
    g_colour_has_pending = false;
}

void colour_service_poll(void)
{
    if (g_colour_session == 0U)
    {
        return;
    }
    uint32_t now_ms = colour_io_now();
    if (!g_colour_has_pending)
    {
        if (g_colour_needs_begin)
        {
            /* Begin clears old events and takes the initial snapshot atomically. */
            if (!colour_io_begin(&g_colour_pending))
            {
                return;
            }
            g_colour_needs_begin = false;
            g_colour_is_snapshot = true;
        }
        else if (!colour_io_take(&g_colour_pending, &g_colour_is_snapshot,
                   (uint32_t)(now_ms - g_colour_published_ms) >= COLOUR_HEARTBEAT_MS))
        {
            return;
        }
        g_colour_has_pending = true;
    }
    uint8_t frame[COLOUR_PACKET_SIZE];
    uint16_t length = colour_protocol_encode(frame, g_colour_session,
                                              &g_colour_pending,
                                              g_colour_is_snapshot);
    if ((length != 0U) && (colour_io_publish(frame, length) == 0))
    {
        g_colour_has_pending = false;
        g_colour_published_ms = now_ms;
    }
}
