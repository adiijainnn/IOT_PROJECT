#ifndef IRRIGATION_H
#define IRRIGATION_H

#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "driver/adc.h"

// Hardware Pin Definitions
#define PIN_DHT22           GPIO_NUM_4
#define PIN_SOIL_ADC        ADC1_CHANNEL_6 // GPIO 34
#define PIN_RELAY           GPIO_NUM_26
#define PIN_PUMP_LED        GPIO_NUM_27
#define PIN_BUZZER          GPIO_NUM_25
#define PIN_MANUAL_BTN      GPIO_NUM_14

#define I2C_MASTER_SDA_IO   GPIO_NUM_21
#define I2C_MASTER_SCL_IO   GPIO_NUM_22
#define I2C_MASTER_NUM      I2C_NUM_0

// Thresholds & Control Parameters
#define SOIL_DRY_THRESHOLD  35.0f // Soil moisture <= 35% -> Turn ON Pump
#define SOIL_WET_THRESHOLD  45.0f // Soil moisture >= 45% -> Turn OFF Pump
#define MANUAL_WATERING_SEC 5     // Manual watering duration in seconds

typedef enum {
    IRRIGATION_MODE_AUTO = 0,
    IRRIGATION_MODE_MANUAL
} irrigation_mode_t;

typedef enum {
    PUMP_STATE_OFF = 0,
    PUMP_STATE_ON
} pump_state_t;

typedef struct {
    float soil_moisture_pct;
    uint16_t soil_raw_adc;
    float temperature_c;
    float humidity_pct;
    bool dht_valid;
    pump_state_t pump_state;
    irrigation_mode_t mode;
    uint32_t manual_timer_sec;
    char status_reason[32];
    bool wifi_connected;
    bool mqtt_connected;
    char ip_address[16];
} system_telemetry_t;

extern system_telemetry_t g_telemetry;
extern SemaphoreHandle_t g_telemetry_mutex;

void irrigation_hardware_init(void);
void irrigation_task(void *pvParameters);
void button_task(void *pvParameters);
void trigger_manual_irrigation(void);
void buzzer_beep(uint32_t duration_ms);

#endif // IRRIGATION_H
