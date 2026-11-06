#ifndef AAI05_MOTOR_IO_H
#define AAI05_MOTOR_IO_H
#include <stdint.h>
#define MOTOR_OUTPUT_MASK (UINT32_C(15) << 8U)
void motor_io_disable_outputs(uint32_t mask);
void motor_io_latch_high(uint32_t mask);
void motor_io_configure_sio(uint32_t pin);
void motor_io_enable_outputs(uint32_t mask);
void drv_motor_coast_init(void);
#endif
