/* Portable, single-owner TCS34725 acquisition. No classification or actuation. */
#ifndef AAI05_RGB_SENSOR_H
#define AAI05_RGB_SENSOR_H

#include <stdbool.h>
#include <stdint.h>

#define RGB_ADDRESS 0x29U
#define RGB_DEVICE_ID 0x44U
#define RGB_ATIME 0xEBU
#define RGB_GAIN_CODE 1U
#define RGB_GAIN 4U
#define RGB_INTEGRATION_US 50400U
#define RGB_SATURATION_COUNT 21504U
#define RGB_FRESH_MS 200U
#define RGB_RETRY_MS 1000U
#define RGB_OSCILLATOR_WAIT_MS 13U /* 3ms minimum + 10ms clock quantum margin. */
#define RGB_CONVERSION_WAIT_MS 67U /* ceil(22 * 2.56ms) + 10ms clock margin. */

#define RGB_OK 0U
#define RGB_STARTING 1U
#define RGB_WAITING 2U
#define RGB_IO_ERROR 3U
#define RGB_TIMEOUT 4U
#define RGB_WRONG_ID 5U
#define RGB_BAD_CONFIG 6U
#define RGB_SATURATED 7U
#define RGB_RESOURCE_ERROR 8U
typedef uint8_t rgb_status_t;

typedef struct
{
    void *p_context;
    rgb_status_t (*open)(void *p_context);
    rgb_status_t (*transfer)(void *p_context, uint8_t address,
                             const uint8_t *p_tx, uint8_t tx_size,
                             uint8_t *p_rx, uint8_t rx_size);
    uint32_t (*now_ms)(void *p_context);
} rgb_bus_t;

typedef struct
{
    uint32_t sampled_ms;
    uint32_t sequence; /* Modulo 2^32; has_sample is independent of sequence. */
    uint32_t errors;   /* Saturating, counts failed acquisition/configuration. */
    uint16_t clear;
    uint16_t red;
    uint16_t green;
    uint16_t blue;
    uint8_t device_id;
    rgb_status_t status;
    bool ready;
    bool has_sample;
    bool valid;
    bool saturated;
} rgb_snapshot_t;

typedef struct
{
    rgb_snapshot_t latest;
    uint32_t phase_started_ms;
    uint8_t phase;
} rgb_sensor_t;

void rgb_sensor_init(rgb_sensor_t *p_sensor);
void rgb_sensor_step(rgb_sensor_t *p_sensor, const rgb_bus_t *p_bus);
bool rgb_sensor_snapshot(const rgb_sensor_t *p_sensor, uint32_t now_ms,
                          rgb_snapshot_t *p_snapshot);

#endif
