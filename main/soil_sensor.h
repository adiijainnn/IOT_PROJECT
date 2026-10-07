#ifndef SOIL_SENSOR_H
#define SOIL_SENSOR_H

#include <stdint.h>

void soil_sensor_init(void);
float soil_sensor_read_pct(uint16_t *out_raw);

#endif // SOIL_SENSOR_H
