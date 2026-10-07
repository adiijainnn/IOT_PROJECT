#include "soil_sensor.h"
#include "driver/adc.h"
#include "esp_log.h"
#include "irrigation.h"

static const char *TAG = "SOIL_SENSOR";

void soil_sensor_init(void) {
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(PIN_SOIL_ADC, ADC_ATTEN_DB_12);
    ESP_LOGI(TAG, "Soil Moisture Sensor ADC configured on GPIO 34 (ADC1_CH6)");
}

float soil_sensor_read_pct(uint16_t *out_raw) {
    uint32_t adc_accum = 0;
    const int num_samples = 16;

    for (int i = 0; i < num_samples; i++) {
        adc_accum += adc1_get_raw(PIN_SOIL_ADC);
    }
    uint16_t raw_val = (uint16_t)(adc_accum / num_samples);

    if (out_raw) {
        *out_raw = raw_val;
    }

    // Scale 0..4095 ADC value to 0.0% .. 100.0% moisture level
    // In Wokwi Potentiometer: Minimum (0V) -> 0.0%, Maximum (3.3V) -> 100.0%
    float moisture_pct = ((float)raw_val / 4095.0f) * 100.0f;
    if (moisture_pct < 0.0f) moisture_pct = 0.0f;
    if (moisture_pct > 100.0f) moisture_pct = 100.0f;

    return moisture_pct;
}
