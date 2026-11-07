#ifndef AAI05_MQTT_TEST_TMONITOR_H
#define AAI05_MQTT_TEST_TMONITOR_H
typedef unsigned char UB;
int tm_printf(UB *p_format, ...);
int tm_sprintf(UB *p_output, const UB *p_format, ...);
#endif
