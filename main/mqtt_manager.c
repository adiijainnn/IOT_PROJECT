#include "mqtt_manager.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "MQTT_MGR";
static esp_mqtt_client_handle_t client = NULL;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT Connected to Broker!");
        if (xSemaphoreTake(g_telemetry_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            g_telemetry.mqtt_connected = true;
            xSemaphoreGive(g_telemetry_mutex);
        }
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT Disconnected");
        if (xSemaphoreTake(g_telemetry_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            g_telemetry.mqtt_connected = false;
            xSemaphoreGive(g_telemetry_mutex);
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT Event Error");
        break;
    default:
        break;
    }
}

void mqtt_manager_init(void) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://broker.emqx.io:1883",
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return;
    }
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
    ESP_LOGI(TAG, "MQTT Client initialized targeting broker.emqx.io");
}

void mqtt_publish_telemetry(const system_telemetry_t *telem) {
    if (client == NULL || !telem->mqtt_connected) {
        return;
    }

    char payload[256];
    snprintf(payload, sizeof(payload),
             "{\"soil\":%.1f,\"temperature\":%.1f,\"humidity\":%.1f,\"pump\":\"%s\",\"mode\":\"%s\"}",
             telem->soil_moisture_pct,
             telem->dht_valid ? telem->temperature_c : 0.0f,
             telem->dht_valid ? telem->humidity_pct : 0.0f,
             telem->pump_state == PUMP_STATE_ON ? "ON" : "OFF",
             telem->mode == IRRIGATION_MODE_AUTO ? "AUTO" : "MANUAL");

    esp_mqtt_client_publish(client, "smart_irrigation/telemetry", payload, 0, 1, 0);

    char val_buf[16];
    snprintf(val_buf, sizeof(val_buf), "%.1f", telem->soil_moisture_pct);
    esp_mqtt_client_publish(client, "smart_irrigation/soil", val_buf, 0, 1, 0);

    snprintf(val_buf, sizeof(val_buf), "%s", telem->pump_state == PUMP_STATE_ON ? "ON" : "OFF");
    esp_mqtt_client_publish(client, "smart_irrigation/pump", val_buf, 0, 1, 0);
}
