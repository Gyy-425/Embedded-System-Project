#ifndef AAI05_APP_H
#define AAI05_APP_H

#include <stdint.h>
#include <stdbool.h>
#include <tk/tkernel.h>
#include <tm/tmonitor.h>
#include <bsp/libbsp.h>

#define TASK_STACK_SIZE 2048
#define APP_PRI_MOTION  12
#define APP_PRI_IR      14
#define APP_PRI_REPORT  16
#define APP_IR_LEFT_PIN 26U
#define APP_IR_RIGHT_PIN 27U
#define APP_ENCODER_LEFT_A 4U
#define APP_ENCODER_LEFT_B 5U
#define APP_ENCODER_RIGHT_A 16U
#define APP_ENCODER_RIGHT_B 17U
#define APP_MOTOR_FIRST_PIN 8U
#define APP_MOTOR_LAST_PIN 11U

typedef struct {
    uint32_t sequence;
    uint32_t sampled_ms;
    uint16_t left_raw;
    uint16_t right_raw;
    ER error;
    bool valid;
} ir_snapshot_t;

typedef struct {
    uint32_t sequence;
    uint32_t sampled_ms;
    int32_t left_count;
    int32_t right_count;
    uint32_t left_invalid;
    uint32_t right_invalid;
    uint8_t left_ab;
    uint8_t right_ab;
    bool count_saturated;
} motion_snapshot_t;

IMPORT ID app_start_task(FP entry, PRI priority, UW processor, const char *name);
IMPORT uint32_t app_now_ms(void);
IMPORT ER subsystem1_init(void);
IMPORT ER subsystem1_start(void);
IMPORT ER subsystem1_get_snapshot(ir_snapshot_t *snapshot);
IMPORT ER subsystem2_init(void);
IMPORT ER subsystem2_start(void);
IMPORT ER subsystem2_get_snapshot(motion_snapshot_t *snapshot);
IMPORT ER subsystem3_init(void);
IMPORT ER subsystem3_start(void);
#if APP_TEST
IMPORT void subsystem2_test(void);
#endif
#endif
