#ifndef RGB_TEST_I2C_H
#define RGB_TEST_I2C_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef unsigned int uint;
typedef struct {uint8_t number;} i2c_inst_t;
extern i2c_inst_t g_test_i2c1;
#define i2c1 (&g_test_i2c1)
typedef uint64_t absolute_time_t;
#define PICO_ERROR_TIMEOUT (-2)
absolute_time_t make_timeout_time_us(uint64_t us);
uint i2c_init(i2c_inst_t *p_i2c, uint baud);
void i2c_deinit(i2c_inst_t *p_i2c);
int i2c_write_blocking_until(i2c_inst_t *p_i2c, uint8_t address,
                             const uint8_t *p_data, size_t size, bool nostop,
                             absolute_time_t until);
int i2c_read_blocking_until(i2c_inst_t *p_i2c, uint8_t address,
                            uint8_t *p_data, size_t size, bool nostop,
                            absolute_time_t until);
#endif
