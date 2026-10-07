#include "irrigation.h"
#include "esp_log.h"
#include "freertos/task.h"

static const char *TAG = "IRRIGATION_CTRL";

system_telemetry_t g_telemetry = {
    .soil_moisture_pct = 0.0f,
    .soil_raw_adc = 0,
    .temperature_c = 0.0f,
    .humidity_pct = 0.0f,
    .dht_valid = false,
    .pump_state = PUMP_STATE_OFF,
    .mode = IRRIGATION_MODE_AUTO,
    .manual_timer_sec = 0,
    .status_reason = "INITIALIZING",
    .wifi_connected = false,
    .mqtt_connected = false,
    .ip_address = "Disconnected"
};

SemaphoreHandle_t g_telemetry_mutex = NULL;

void buzzer_beep(uint32_t duration_ms) {
    gpio_set_level(PIN_BUZZER, 1);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    gpio_set_level(PIN_BUZZER, 0);
}

void irrigation_hardware_init(void) {
    g_telemetry_mutex = xSemaphoreCreateMutex();

    // Relay Output
    gpio_config_t relay_conf = {
        .pin_bit_mask = (1ULL << PIN_RELAY),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&relay_conf);
    gpio_set_level(PIN_RELAY, 0);

    // Pump LED Output
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << PIN_PUMP_LED),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&led_conf);
    gpio_set_level(PIN_PUMP_LED, 0);

    // Buzzer Output
    gpio_config_t buzzer_conf = {
        .pin_bit_mask = (1ULL << PIN_BUZZER),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&buzzer_conf);
    gpio_set_level(PIN_BUZZER, 0);

    // Manual Button Input (Active LOW with internal pull-up)
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << PIN_MANUAL_BTN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&btn_conf);

    ESP_LOGI(TAG, "Actuators & Controls GPIO initialized successfully");
}

void trigger_manual_irrigation(void) {
    if (xSemaphoreTake(g_telemetry_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        g_telemetry.mode = IRRIGATION_MODE_MANUAL;
        g_telemetry.manual_timer_sec = MANUAL_WATERING_SEC;
        g_telemetry.pump_state = PUMP_STATE_ON;
        snprintf(g_telemetry.status_reason, sizeof(g_telemetry.status_reason), "MANUAL WATERING");
        xSemaphoreGive(g_telemetry_mutex);

        gpio_set_level(PIN_RELAY, 1);
        gpio_set_level(PIN_PUMP_LED, 1);
        ESP_LOGI(TAG, "Manual irrigation triggered for %d seconds", MANUAL_WATERING_SEC);
        buzzer_beep(200);
    }
}

void irrigation_task(void *pvParameters) {
    ESP_LOGI(TAG, "Irrigation control task started");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(500));

        if (xSemaphoreTake(g_telemetry_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            irrigation_mode_t current_mode = g_telemetry.mode;
            float current_moisture = g_telemetry.soil_moisture_pct;
            pump_state_t current_pump = g_telemetry.pump_state;

            if (current_mode == IRRIGATION_MODE_MANUAL) {
                if (g_telemetry.manual_timer_sec > 0) {
                    g_telemetry.manual_timer_sec--;
                    g_telemetry.pump_state = PUMP_STATE_ON;
                    snprintf(g_telemetry.status_reason, sizeof(g_telemetry.status_reason), "MANUAL (%lds)", (long)g_telemetry.manual_timer_sec);
                } else {
                    // Manual duration finished, revert to auto
                    g_telemetry.mode = IRRIGATION_MODE_AUTO;
                    ESP_LOGI(TAG, "Manual watering complete. Reverting to AUTO mode.");
                }
            }

            if (g_telemetry.mode == IRRIGATION_MODE_AUTO) {
                // Hysteresis decision logic
                if (current_moisture <= SOIL_DRY_THRESHOLD) {
                    if (current_pump == PUMP_STATE_OFF) {
                        g_telemetry.pump_state = PUMP_STATE_ON;
                        buzzer_beep(150);
                    }
                    snprintf(g_telemetry.status_reason, sizeof(g_telemetry.status_reason), "SOIL DRY (<=%.0f%%)", SOIL_DRY_THRESHOLD);
                } else if (current_moisture >= SOIL_WET_THRESHOLD) {
                    if (current_pump == PUMP_STATE_ON) {
                        g_telemetry.pump_state = PUMP_STATE_OFF;
                    }
                    snprintf(g_telemetry.status_reason, sizeof(g_telemetry.status_reason), "SOIL WET (>=%.0f%%)", SOIL_WET_THRESHOLD);
                } else {
                    // In hysteresis deadband (35% to 45%): maintain previous state
                    if (g_telemetry.pump_state == PUMP_STATE_ON) {
                        snprintf(g_telemetry.status_reason, sizeof(g_telemetry.status_reason), "BAND (PUMP ON)");
                    } else {
                        snprintf(g_telemetry.status_reason, sizeof(g_telemetry.status_reason), "BAND (PUMP OFF)");
                    }
                }
            }

            pump_state_t new_pump_state = g_telemetry.pump_state;
            xSemaphoreGive(g_telemetry_mutex);

            // Apply output states to relay & pump LED
            gpio_set_level(PIN_RELAY, (new_pump_state == PUMP_STATE_ON) ? 1 : 0);
            gpio_set_level(PIN_PUMP_LED, (new_pump_state == PUMP_STATE_ON) ? 1 : 0);
        }
    }
}

void button_task(void *pvParameters) {
    ESP_LOGI(TAG, "Button monitoring task started on GPIO 14");
    while (1) {
        if (gpio_get_level(PIN_MANUAL_BTN) == 0) { // Pressed (Active LOW)
            vTaskDelay(pdMS_TO_TICKS(50)); // Debounce delay
            if (gpio_get_level(PIN_MANUAL_BTN) == 0) {
                trigger_manual_irrigation();
                // Wait for button release
                while (gpio_get_level(PIN_MANUAL_BTN) == 0) {
                    vTaskDelay(pdMS_TO_TICKS(50));
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
