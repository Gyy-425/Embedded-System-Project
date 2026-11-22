#ifndef RGB_TEST_GPIO_H
#define RGB_TEST_GPIO_H
#define GPIO_FUNC_I2C 3U
void gpio_set_function(unsigned int pin, unsigned int function);
void gpio_pull_up(unsigned int pin);
#endif
