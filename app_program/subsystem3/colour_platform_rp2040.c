/* Network owner only; IR functions copy under their nonblocking mutex. */
#include "colour_service.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include "../subsystem1/subsystem1.h"
#pragma GCC diagnostic pop

#if AAI05_REMOTE_APP
#include "mqtt_utk.h"

uint32_t colour_io_now(void)
{
    return app_now_ms();
}

bool colour_io_begin(colour_report_t *p_report)
{
    return subsystem1_colour_begin(p_report);
}

bool colour_io_take(colour_report_t *p_report, bool *p_is_snapshot,
                      bool allow_snapshot)
{
    return subsystem1_colour_take(p_report, p_is_snapshot, allow_snapshot);
}

int32_t colour_io_publish(const uint8_t *p_frame, uint16_t length)
{
    return mqtt_utk_publish_binary(COLOUR_TOPIC_TELEMETRY, p_frame, length, false);
}
#endif
