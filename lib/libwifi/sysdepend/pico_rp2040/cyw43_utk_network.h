#ifndef CYW43_UTK_NETWORK_H
#define CYW43_UTK_NETWORK_H
#include "cyw43_utk.h"
#include "mqtt_utk.h"

/* The network-owner loop calls this once after observing the current link. */
static inline void cyw43_utk_mqtt_step(int32_t link_status)
{
    if (link_status == CYW43_UTK_LINK_UP)
    {
        mqtt_utk_start();
    }
#if AAI05_MQTT_APP
    /* Observe DOWN as well: REMOTE must stop, not resume on a short outage. */
    mqtt_utk_service();
#endif
}
#endif
