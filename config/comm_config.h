/* Public topic configuration. Wi-Fi credentials stay in wifi_secrets.h. */
#ifndef COMM_CONFIG_H
#define COMM_CONFIG_H

#define COMM_DEVICE_ID "robot01"
#define COMM_TOPIC_COMMAND "aai05/v1/" COMM_DEVICE_ID "/command"
#define COMM_TOPIC_ACK "aai05/v1/" COMM_DEVICE_ID "/ack"
#define COMM_TOPIC_TELEMETRY "aai05/v1/" COMM_DEVICE_ID "/telemetry"
#define COMM_TELEMETRY_PERIOD_MS 500U
#define COMM_IR_MAX_AGE_MS 100U

#endif /* COMM_CONFIG_H */
