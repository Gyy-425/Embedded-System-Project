/*
 *	mqtt_utk.c
 *	MQTT client wiring for micro T-Kernel (RP2040, NO_SYS=1)
 *
 *	Runs entirely on the single processor-1-owned task that already owns
 *	the CYW43439 and the rest of lwIP (docs/RELEASE.md safety rule 6):
 *	cyw43_utk_task() (cyw43_utk.c) calls mqtt_utk_start() once, the first
 *	time it observes CYW43_LINK_UP. mqtt.c's own keepalive/ping/retry
 *	timers, and the heartbeat below, both ride the same sys_check_timeouts()
 *	polling that task already does for DHCP and TCP -- nothing extra runs
 *	on any other task or core.
 *
 *	This file does not include tk/tkernel.h or any pico-sdk/cyw43 header
 *	(unlike cyw43_utk.c and sys_arch.c), so it isn't subject to either
 *	file's size_t workaround -- tm/tmonitor.h alone is namespace-safe here.
 */

#include "lwip/apps/mqtt.h"
#include "lwip/ip_addr.h"
#include "lwip/timeouts.h"
#include "wifi_secrets.h"
#include "mqtt_utk.h"
#include <tm/tmonitor.h>
#include <string.h>

#define MQTT_UTK_HEARTBEAT_MS   5000
#define MQTT_UTK_KEEPALIVE_S    60
#define MQTT_UTK_RETRY_MS       3000

/* Inbound message staging. A publish arrives as one topic callback followed by
 * one or more data callbacks, so the topic has to be kept until the fragment
 * carrying MQTT_DATA_FLAG_LAST shows up. Both buffers are sized well past the
 * heartbeat this demo publishes; anything longer is truncated and flagged
 * rather than being allowed to run off the end. */
#define MQTT_UTK_RX_TOPIC_MAX   64
#define MQTT_UTK_RX_DATA_MAX    128

static mqtt_client_t	*client;
static ip_addr_t	broker_addr;
static u8_t		connecting;

static char		rx_topic[MQTT_UTK_RX_TOPIC_MAX];
static UB		rx_data[MQTT_UTK_RX_DATA_MAX];
static u16_t		rx_len;
static u8_t		rx_truncated;

#if AAI05_COMMS_APP
static uint32_t g_rx_expected;
static bool g_is_app_ready;
static bool g_is_sub_pending;
static void mqtt_app_subscribe(void);
static void app_subscribe_retry(void *p_argument);
#else
static void heartbeat_timeout(void *arg);
#endif

/* mqtt.c NUL-terminates the topic before handing it over, but it is only
 * valid for the duration of this call -- copy it out. */
static void incoming_publish_cb(void *arg, const char *topic, u32_t tot_len)
{
	u32_t	i;

	(void)arg;
	(void)tot_len;

	for(i = 0; i < (MQTT_UTK_RX_TOPIC_MAX - 1) && topic[i] != '\0'; i++) {
		rx_topic[i] = topic[i];
	}
	rx_topic[i] = '\0';

	rx_len = 0;
	rx_truncated = 0;
#if AAI05_COMMS_APP
    g_rx_expected = tot_len;
    if (('\0' != topic[i]) || (tot_len > MQTT_UTK_RX_DATA_MAX))
    {
        rx_truncated = 1U;
    }
#endif
}

static void incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags)
{
#if !AAI05_COMMS_APP
	u16_t	i;
#endif

	(void)arg;

#if AAI05_COMMS_APP
    for (uint16_t index = 0U; index < len; ++index)
    {
        if (rx_len < MQTT_UTK_RX_DATA_MAX)
        {
            rx_data[rx_len] = data[index];
            ++rx_len;
        }
        else
        {
            rx_truncated = 1U;
        }
    }
    if (((flags & MQTT_DATA_FLAG_LAST) != 0U) && g_is_app_ready &&
        (0 == strcmp(rx_topic, mqtt_app_command_topic())))
    {
        if ((0U == rx_truncated) && (rx_len == g_rx_expected))
        {
            mqtt_app_receive(rx_data, rx_len);
        }
        else
        {
            mqtt_app_receive(NULL, 0U);
        }
    }
#else
	for(i = 0; i < len; i++) {
		if(rx_len < (MQTT_UTK_RX_DATA_MAX - 1)) {
			/* The console is a terminal -- keep raw bytes from a
			 * payload this demo did not publish from scrambling it. */
			UB c = data[i];
			rx_data[rx_len++] = (c >= 0x20 && c < 0x7f)? c: '.';
		} else {
			rx_truncated = 1;
		}
	}

	if((flags & MQTT_DATA_FLAG_LAST) != 0) {
		rx_data[rx_len] = '\0';
		tm_printf((UB*)"mqtt: RX [%s] \"%s\"%s\n", rx_topic, rx_data,
			  rx_truncated? " (truncated)": "");
	}
#endif
}

#if !AAI05_COMMS_APP
static void subscribe_done_cb(void *arg, err_t result)
{
	(void)arg;
	if(result == ERR_OK) {
		tm_printf((UB*)"mqtt: subscribed to " MQTT_TOPIC "\n");
	} else {
		tm_printf((UB*)"mqtt: subscribe failed, err=%d\n", (int)result);
	}
}
#endif

static void publish_done_cb(void *arg, err_t result)
{
	(void)arg;
	if(result != ERR_OK) {
		tm_printf((UB*)"mqtt: publish failed, err=%d\n", (int)result);
	}
}

#if !AAI05_COMMS_APP
static void heartbeat_timeout(void *arg)
{
	static unsigned int	count = 0;
	UB			payload[48];
	int			len;

	(void)arg;

	if(mqtt_client_is_connected(client)) {
		len = tm_sprintf(payload, (const UB*)"hb %u", count++);
		tm_printf((UB*)"mqtt: TX [" MQTT_TOPIC "] \"%s\"\n", payload);
		(void)mqtt_publish(client, MQTT_TOPIC, payload, (u16_t)len,
				   0 /* qos */, 0 /* retain */,
				   publish_done_cb, NULL);
	}

	/* Re-arms itself every call -- mqtt.c's connection callback (below)
	 * only starts this chain once, on the transition into MQTT_CONNECT_
	 * ACCEPTED, so there is exactly one heartbeat chain alive at a time. */
	sys_timeout(MQTT_UTK_HEARTBEAT_MS, heartbeat_timeout, NULL);
}
#endif

#if AAI05_COMMS_APP
static void app_subscribe_done(void *p_argument, err_t result)
{
    (void)p_argument;
    g_is_sub_pending = false;
    if ((ERR_OK == result) && mqtt_client_is_connected(client))
    {
        g_is_app_ready = true;
        sys_untimeout(app_subscribe_retry, NULL);
        mqtt_app_link_changed(true);
        tm_printf((UB *)"mqtt: command subscription ready\n");
    }
    else if (mqtt_client_is_connected(client))
    {
        sys_untimeout(app_subscribe_retry, NULL);
        sys_timeout(MQTT_UTK_RETRY_MS, app_subscribe_retry, NULL);
    }
}

static void mqtt_app_subscribe(void)
{
    err_t result;

    if (!mqtt_client_is_connected(client) || g_is_app_ready ||
        g_is_sub_pending)
    {
        return;
    }
    g_is_sub_pending = true;
    result = mqtt_subscribe(client, mqtt_app_command_topic(), 1U,
                            app_subscribe_done, NULL);
    if (ERR_OK != result)
    {
        g_is_sub_pending = false;
        sys_untimeout(app_subscribe_retry, NULL);
        sys_timeout(MQTT_UTK_RETRY_MS, app_subscribe_retry, NULL);
    }
}

static void app_subscribe_retry(void *p_argument)
{
    (void)p_argument;
    mqtt_app_subscribe();
}

int32_t mqtt_utk_publish_binary(const char *p_topic, const uint8_t *p_data,
                                uint16_t length, bool is_reliable)
{
    if ((NULL == client) || !g_is_app_ready ||
        !mqtt_client_is_connected(client))
    {
        return ERR_CONN;
    }
    if ((NULL == p_topic) || (NULL == p_data) || (0U == length) ||
        (length > MQTT_UTK_RX_DATA_MAX))
    {
        return ERR_ARG;
    }
    return mqtt_publish(client, p_topic, p_data, length,
                         (u8_t)(is_reliable ? 1U : 0U), 0U,
                         publish_done_cb, NULL);
}

void mqtt_utk_service(void)
{
    if ((NULL != client) && g_is_app_ready &&
        mqtt_client_is_connected(client))
    {
        mqtt_app_poll();
    }
}
#endif

static void retry_timeout(void *arg)
{
	(void)arg;
	connecting = 0;
	mqtt_utk_start();
}

static void connection_cb(mqtt_client_t *c, void *arg, mqtt_connection_status_t status)
{
	(void)c;
	(void)arg;

#if AAI05_COMMS_APP
    g_is_app_ready = false;
    g_is_sub_pending = false;
    sys_untimeout(app_subscribe_retry, NULL);
    mqtt_app_link_changed(false);
#endif
	if(status == MQTT_CONNECT_ACCEPTED) {
#if !AAI05_COMMS_APP
		err_t	err;
#endif

		/* No longer *connecting* -- clear the guard so that if this
		 * connection later drops, the poll loop's mqtt_utk_start() call
		 * is allowed to reconnect. Leaving it set would wedge the client
		 * permanently after the first disconnect. */
		connecting = 0;
		sys_untimeout(retry_timeout, NULL);

		tm_printf((UB*)"mqtt: connected to " MQTT_BROKER_IP "\n");

		/* Subscribe to the same topic this client publishes to, so every
		 * heartbeat comes back from the broker and is printed by
		 * incoming_data_cb(). A matching RX for each TX is end-to-end
		 * proof of the round trip without needing mosquitto_sub on the
		 * broker host. Callbacks must be registered before subscribing,
		 * or the first delivery has nowhere to go. */
		mqtt_set_inpub_callback(client, incoming_publish_cb,
					incoming_data_cb, NULL);

#if AAI05_COMMS_APP
        mqtt_app_subscribe();
#else
		err = mqtt_subscribe(client, MQTT_TOPIC, 0 /* qos */,
				     subscribe_done_cb, NULL);
		if(err != ERR_OK) {
			tm_printf((UB*)"mqtt: mqtt_subscribe() failed, err=%d\n",
				  (int)err);
		}

		/* Drop any chain left over from a previous connection before
		 * arming a new one. Each reconnect would otherwise add another
		 * self-perpetuating heartbeat, and every one of them holds a
		 * MEMP_SYS_TIMEOUT slot until the pool is exhausted. */
		sys_untimeout(heartbeat_timeout, NULL);
		sys_timeout(MQTT_UTK_HEARTBEAT_MS, heartbeat_timeout, NULL);
#endif
	} else {
		/* mqtt_client_connect() reconnects on its own for a dropped
		 * keepalive; a rejected/never-established connection does not
		 * retry itself, so do that here -- on a delay, so a broker
		 * that keeps rejecting (e.g. bad client ID) can't turn this
		 * into a tight reconnect loop. */
		tm_printf((UB*)"mqtt: connection status %d, retrying in %ds\n",
			  (int)status, MQTT_UTK_RETRY_MS / 1000);
        connecting = 1U;
        sys_untimeout(retry_timeout, NULL);
		sys_timeout(MQTT_UTK_RETRY_MS, retry_timeout, NULL);
	}
}

void mqtt_utk_start(void)
{
	struct mqtt_connect_client_info_t	client_info = {0};
	err_t					err;

	if(connecting || (client != NULL && mqtt_client_is_connected(client))) {
		return;
	}

	if(client == NULL) {
		client = mqtt_client_new();
		if(client == NULL) {
			tm_printf((UB*)"mqtt: mqtt_client_new() failed\n");
			return;
		}
	}

	if(!ip4addr_aton(MQTT_BROKER_IP, ip_2_ip4(&broker_addr))) {
		tm_printf((UB*)"mqtt: bad MQTT_BROKER_IP\n");
		return;
	}
	IP_SET_TYPE(&broker_addr, IPADDR_TYPE_V4);

	client_info.client_id = MQTT_CLIENT_ID;
	client_info.keep_alive = MQTT_UTK_KEEPALIVE_S;

	connecting = 1;
	err = mqtt_client_connect(client, &broker_addr, MQTT_BROKER_PORT,
				  connection_cb, NULL, &client_info);
	if(err != ERR_OK) {
		tm_printf((UB*)"mqtt: mqtt_client_connect() failed, err=%d\n", (int)err);
        connecting = 1U;
        sys_untimeout(retry_timeout, NULL);
        sys_timeout(MQTT_UTK_RETRY_MS, retry_timeout, NULL);
	}
}
