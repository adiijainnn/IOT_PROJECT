#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include "irrigation.h"

void mqtt_manager_init(void);
void mqtt_publish_telemetry(const system_telemetry_t *telem);

#endif // MQTT_MANAGER_H
