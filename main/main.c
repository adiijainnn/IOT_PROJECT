#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "irrigation.h"
#include "soil_sensor.h"
#include "dht22.h"
#include "lcd.h"
#include "wifi_manager.h"
#include "mqtt_manager.h"

static const char *TAG = "MAIN_APP";

static void sensor_task(void *pvParameters) {
    ESP_LOGI(TAG, "Sensor sampling task started");
    while (1) {
        uint16_t raw_adc = 0;
        float soil_pct = soil_sensor_read_pct(&raw_adc);

        float temp = 0.0f, hum = 0.0f;
        bool dht_ok = dht22_read(PIN_DHT22, &temp, &hum);

        if (xSemaphoreTake(g_telemetry_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            g_telemetry.soil_raw_adc = raw_adc;
            g_telemetry.soil_moisture_pct = soil_pct;
            g_telemetry.dht_valid = dht_ok;
            if (dht_ok) {
                g_telemetry.temperature_c = temp;
                g_telemetry.humidity_pct = hum;
            }
            xSemaphoreGive(g_telemetry_mutex);
        }

        if (!dht_ok) {
            ESP_LOGW(TAG, "DHT22 read failure. Retrying next cycle...");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void serial_monitor_task(void *pvParameters) {
    ESP_LOGI(TAG, "Serial telemetry logging task started");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(2000));

        system_telemetry_t telem;
        if (xSemaphoreTake(g_telemetry_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            telem = g_telemetry;
            xSemaphoreGive(g_telemetry_mutex);
        }

        printf("\n================================\n");
        printf("    SMART IRRIGATION SYSTEM     \n");
        printf("================================\n");
        printf("Soil Moisture : %5.1f %% (ADC: %u)\n", telem.soil_moisture_pct, telem.soil_raw_adc);
        if (telem.dht_valid) {
            printf("Temperature   : %5.1f C\n", telem.temperature_c);
            printf("Humidity      : %5.1f %%\n", telem.humidity_pct);
        } else {
            printf("Temperature   : DHT ERROR\n");
            printf("Humidity      : DHT ERROR\n");
        }
        printf("Pump State    : %s\n", (telem.pump_state == PUMP_STATE_ON) ? "ON" : "OFF");
        printf("Control Mode  : %s\n", (telem.mode == IRRIGATION_MODE_AUTO) ? "AUTO" : "MANUAL");
        printf("System Status : %s\n", telem.status_reason);
        printf("WiFi Status   : %s (%s)\n", telem.wifi_connected ? "Connected" : "Offline", telem.ip_address);
        printf("MQTT Status   : %s\n", telem.mqtt_connected ? "Connected" : "Disconnected");
        printf("================================\n");

        mqtt_publish_telemetry(&telem);
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "===============================================");
    ESP_LOGI(TAG, " ESP32 SMART IRRIGATION SYSTEM - ESP-IDF C     ");
    ESP_LOGI(TAG, " Final Year B.Tech IoT Mini Project            ");
    ESP_LOGI(TAG, " Walchand Institute of Technology, Solapur     ");
    ESP_LOGI(TAG, "===============================================");

    // 1. Initialize Hardware Components
    irrigation_hardware_init();
    soil_sensor_init();
    dht22_init(PIN_DHT22);

    // 2. Initialize LCD
    if (lcd_init(I2C_MASTER_NUM, I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO) != ESP_OK) {
        ESP_LOGE(TAG, "LCD Initialization failed!");
    }

    // 3. Initialize WiFi and MQTT
    wifi_manager_init();
    mqtt_manager_init();

    // 4. Create FreeRTOS Tasks
    xTaskCreatePinnedToCore(sensor_task, "sensor_task", 3072, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(irrigation_task, "irrigation_task", 3072, NULL, 4, NULL, 1);
    xTaskCreatePinnedToCore(button_task, "button_task", 2048, NULL, 6, NULL, 0);
    xTaskCreatePinnedToCore(lcd_task, "lcd_task", 3072, NULL, 3, NULL, 0);
    xTaskCreatePinnedToCore(serial_monitor_task, "serial_task", 3072, NULL, 2, NULL, 0);

    ESP_LOGI(TAG, "All FreeRTOS tasks initialized and running successfully.");
}
