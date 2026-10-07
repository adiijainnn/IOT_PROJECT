#ifndef DHT22_H
#define DHT22_H

#include <stdbool.h>
#include "driver/gpio.h"

void dht22_init(gpio_num_t pin);
bool dht22_read(gpio_num_t pin, float *temperature, float *humidity);

#endif // DHT22_H
