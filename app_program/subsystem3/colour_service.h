#ifndef AAI05_COLOUR_SERVICE_H
#define AAI05_COLOUR_SERVICE_H

#include "colour_protocol.h"
#include "../../config/comm_config.h"

#define COLOUR_TOPIC_TELEMETRY "aai05/colour/v1/" COMM_DEVICE_ID "/telemetry"
#define COLOUR_HEARTBEAT_MS 1000U

/* Single network-owner context. No calls into Motion. */
void colour_service_link(uint32_t session);
void colour_service_poll(void);

uint32_t colour_io_now(void);
bool colour_io_begin(colour_report_t *p_report);
bool colour_io_take(colour_report_t *p_report, bool *p_is_snapshot,
                      bool allow_snapshot);
int32_t colour_io_publish(const uint8_t *p_frame, uint16_t length);

#endif /* AAI05_COLOUR_SERVICE_H */
