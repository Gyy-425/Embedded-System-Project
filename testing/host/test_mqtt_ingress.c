/* Actual port callbacks and actual SDK MQTT types, no sockets or credentials. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "lwip/apps/mqtt_priv.h"
#include "lwip/timeouts.h"
#include "mqtt_utk.h"
#include <tm/tmonitor.h>

static mqtt_client_t g_client;
static uint32_t g_deliveries;
static uint16_t g_length;
static bool g_retained;
static uint8_t g_payload[128];
static uint8_t g_published_qos;
static uint8_t g_published_retain;
static uint32_t g_service_calls;

int tm_printf(UB *p_format, ...) { (void)p_format; return 0; }
int tm_sprintf(UB *p_output, const UB *p_format, ...)
{
    (void)p_output; (void)p_format; return 0;
}
int ip4addr_aton(const char *p_text, ip4_addr_t *p_address)
{
    (void)p_text;
    p_address->addr = 0U;
    return 1;
}
mqtt_client_t *mqtt_client_new(void) { return &g_client; }
u8_t mqtt_client_is_connected(mqtt_client_t *p_client)
{
    (void)p_client; return 1U;
}
err_t mqtt_client_connect(mqtt_client_t *p_client, const ip_addr_t *p_address,
    u16_t port, mqtt_connection_cb_t callback, void *p_arg,
    const struct mqtt_connect_client_info_t *p_info)
{
    (void)p_client; (void)p_address; (void)port; (void)callback;
    (void)p_arg; (void)p_info; return ERR_OK;
}
void mqtt_set_inpub_callback(mqtt_client_t *p_client,
    mqtt_incoming_publish_cb_t pub_cb, mqtt_incoming_data_cb_t data_cb,
    void *p_argument)
{
    p_client->pub_cb = pub_cb;
    p_client->data_cb = data_cb;
    p_client->inpub_arg = p_argument;
}
err_t mqtt_sub_unsub(mqtt_client_t *p_client, const char *p_topic, u8_t qos,
    mqtt_request_cb_t callback, void *p_argument, u8_t sub)
{
    (void)p_client; (void)p_topic; (void)qos; (void)callback;
    (void)p_argument; (void)sub; return ERR_OK;
}
err_t mqtt_publish(mqtt_client_t *p_client, const char *p_topic,
    const void *p_payload, u16_t length, u8_t qos, u8_t retain,
    mqtt_request_cb_t callback, void *p_argument)
{
    (void)p_client; (void)p_topic; (void)p_payload; (void)length;
    (void)callback; (void)p_argument;
    g_published_qos = qos;
    g_published_retain = retain;
    return ERR_OK;
}
void sys_timeout(u32_t delay, sys_timeout_handler callback, void *p_argument)
{
    (void)delay; (void)callback; (void)p_argument;
}
void sys_untimeout(sys_timeout_handler callback, void *p_argument)
{
    (void)callback; (void)p_argument;
}
const char *mqtt_app_command_topic(void) { return "test/command"; }
void mqtt_app_link_changed(bool is_ready) { (void)is_ready; }
void mqtt_app_poll(void) { ++g_service_calls; }
static void test_deliver(const uint8_t *p_data, uint16_t length,
                           bool is_retained)
{
    ++g_deliveries;
    g_length = length;
    g_retained = is_retained;
    if (p_data != NULL)
    {
        assert(length <= sizeof(g_payload));
        memcpy(g_payload, p_data, length);
    }
}
#if AAI05_REMOTE_APP
void mqtt_drive_receive(const uint8_t *p_data, uint16_t length,
                         bool is_retained)
{
    test_deliver(p_data, length, is_retained);
}
#else
void mqtt_app_receive(const uint8_t *p_data, uint16_t length)
{
    test_deliver(p_data, length, false);
}
#endif

#include "../../lib/liblwip/sysdepend/pico_rp2040/mqtt_utk.c"
#include "../../lib/libwifi/sysdepend/pico_rp2040/cyw43_utk_network.h"

int main(void)
{
    const uint8_t payload[14] = {0U, 1U, 2U, 3U, 4U, 5U, 6U,
                                7U, 8U, 9U, 10U, 11U, 12U, 13U};
    mqtt_utk_start();
    assert(client == &g_client);
    g_is_app_ready = true;
    g_client.rx_buffer[0] = 0x33U; /* PUBLISH QoS1, retained delivery. */
    incoming_publish_cb(NULL, "test/command", 14U);
    g_client.rx_buffer[0] = 0x32U; /* Later bytes cannot change saved metadata. */
    incoming_data_cb(NULL, payload, 5U, 0U);
    assert(g_deliveries == 0U);
    incoming_data_cb(NULL, &payload[5], 9U, MQTT_DATA_FLAG_LAST);
    assert(g_deliveries == 1U && g_length == 14U);
    assert(memcmp(payload, g_payload, 14U) == 0);
    assert(g_retained == (AAI05_REMOTE_APP != 0));
    incoming_publish_cb(NULL, "test/command", 14U);
    incoming_data_cb(NULL, payload, 14U, MQTT_DATA_FLAG_LAST);
    assert(g_deliveries == 2U && !g_retained);
    incoming_publish_cb(NULL, "wrong/topic", 14U);
    incoming_data_cb(NULL, payload, 14U, MQTT_DATA_FLAG_LAST);
    assert(g_deliveries == 2U);
    incoming_publish_cb(NULL, "test/command", 14U);
    incoming_data_cb(NULL, payload, 13U, MQTT_DATA_FLAG_LAST);
    assert(g_deliveries == 3U && g_length == 0U);
    incoming_publish_cb(NULL, "test/command", 129U);
    incoming_data_cb(NULL, payload, 14U, MQTT_DATA_FLAG_LAST);
    assert(g_deliveries == 4U && g_length == 0U);
    assert(mqtt_utk_publish_binary("test/ack", payload, 14U, true) == ERR_OK);
    assert(g_published_qos == 1U && g_published_retain == 0U);
    cyw43_utk_mqtt_step(CYW43_UTK_LINK_UP);
    assert(g_service_calls == 1U); /* REMOTE must receive periodic service. */
    cyw43_utk_mqtt_step(0);
    assert(g_service_calls == 2U); /* Observed DOWN must reach app STOP policy. */
    puts("PASS: actual MQTT callback fragmentation/topic/length/metadata "
         "and non-retained publication boundary");
    return 0;
}
