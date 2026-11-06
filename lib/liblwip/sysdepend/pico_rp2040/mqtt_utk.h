/*
 *	mqtt_utk.h
 *	MQTT client wiring for micro T-Kernel (RP2040, NO_SYS=1)
 */

#ifndef MQTT_UTK_H
#define MQTT_UTK_H

#include <stdbool.h>
#include <stdint.h>

#ifndef AAI05_COMMS_APP
#define AAI05_COMMS_APP 0
#endif

/*
 * Call once, from cyw43_utk_task() the first time the link reaches
 * CYW43_LINK_UP (see cyw43_utk.c). Starts the MQTT connection to
 * MQTT_BROKER_IP:MQTT_BROKER_PORT (config/wifi_secrets.h) and, once
 * connected, a periodic heartbeat publish on MQTT_TOPIC. Safe to call more
 * than once -- a second call while already connecting/connected is a no-op.
 */
void mqtt_utk_start(void);

#if AAI05_COMMS_APP
/* Called only by the existing radio owner, never by an app/report task. */
void mqtt_utk_service(void);
int32_t mqtt_utk_publish_binary(const char *p_topic, const uint8_t *p_data,
                                uint16_t length, bool is_reliable);

/* Application hooks executed only on that same network-service task. */
const char *mqtt_app_command_topic(void);
void mqtt_app_link_changed(bool is_ready);
void mqtt_app_receive(const uint8_t *p_data, uint16_t length);
void mqtt_app_poll(void);
#endif

#endif /* MQTT_UTK_H */
