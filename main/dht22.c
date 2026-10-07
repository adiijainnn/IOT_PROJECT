#include "dht22.h"
#include "esp_timer.h"
#include "rom/ets_sys.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "DHT22";

static void delay_us(uint32_t us) {
    ets_delay_us(us);
}

void dht22_init(gpio_num_t pin) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    ESP_LOGI(TAG, "DHT22 GPIO %d initialized", pin);
}

static int wait_for_level(gpio_num_t pin, int level, uint32_t timeout_us) {
    int64_t start_time = esp_timer_get_time();
    while (gpio_get_level(pin) != level) {
        if ((esp_timer_get_time() - start_time) > timeout_us) {
            return -1;
        }
    }
    return (int)(esp_timer_get_time() - start_time);
}

bool dht22_read(gpio_num_t pin, float *temperature, float *humidity) {
    uint8_t data[5] = {0, 0, 0, 0, 0};

    // Send host start signal: LOW for 20ms, then HIGH
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(pin, 1);
    delay_us(30);

    // Switch to input mode
    gpio_set_direction(pin, GPIO_MODE_INPUT);

    // Wait for response sequence from DHT22 (LOW ~80us, HIGH ~80us)
    if (wait_for_level(pin, 0, 100) < 0) return false;
    if (wait_for_level(pin, 1, 100) < 0) return false;
    if (wait_for_level(pin, 0, 100) < 0) return false;

    // Read 40 data bits
    for (int i = 0; i < 40; i++) {
        if (wait_for_level(pin, 1, 100) < 0) return false;

        int64_t high_start = esp_timer_get_time();
        if (wait_for_level(pin, 0, 100) < 0) return false;
        int64_t high_dur = esp_timer_get_time() - high_start;

        data[i / 8] <<= 1;
        if (high_dur > 40) { // Pulse > 40us indicates logic '1'
            data[i / 8] |= 1;
        }
    }

    // Verify 8-bit Checksum
    uint8_t calculated_checksum = (data[0] + data[1] + data[2] + data[3]) & 0xFF;
    if (calculated_checksum != data[4]) {
        ESP_LOGW(TAG, "DHT22 checksum mismatch: calc=0x%02X, rcv=0x%02X", calculated_checksum, data[4]);
        return false;
    }

    uint16_t raw_hum = ((uint16_t)data[0] << 8) | data[1];
    uint16_t raw_temp = ((uint16_t)(data[2] & 0x7F) << 8) | data[3];

    float hum_val = raw_hum / 10.0f;
    float temp_val = raw_temp / 10.0f;
    if (data[2] & 0x80) {
        temp_val = -temp_val;
    }

    if (humidity) *humidity = hum_val;
    if (temperature) *temperature = temp_val;

    return true;
}
