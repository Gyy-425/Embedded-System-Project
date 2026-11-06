#ifndef AAI05_DRV_IR_H
#define AAI05_DRV_IR_H
#include "../app.h"
ER drv_ir_open(void);
ER drv_ir_read(uint16_t *left, uint16_t *right);
#endif
