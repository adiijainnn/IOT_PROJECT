#include "lcd.h"
#include "irrigation.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "LCD_DISPLAY";
static i2c_port_t g_i2c_port = I2C_NUM_0;
static uint8_t g_backlight = 0x08; // Bit 3 = Backlight ON

#define PIN_RS (1 << 0)
#define PIN_RW (1 << 1)
#define PIN_EN (1 << 2)

static esp_err_t pcf8574_write(uint8_t data) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (LCD_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, data | g_backlight, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(g_i2c_port, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

static void lcd_pulse_enable(uint8_t data) {
    pcf8574_write(data | PIN_EN);
    ets_delay_us(1);
    pcf8574_write(data & ~PIN_EN);
    ets_delay_us(50);
}

static void lcd_write_nibble(uint8_t nibble, uint8_t mode) {
    uint8_t data = (nibble & 0xF0) | mode;
    pcf8574_write(data);
    lcd_pulse_enable(data);
}

static void lcd_send_byte(uint8_t value, uint8_t mode) {
    lcd_write_nibble(value & 0xF0, mode);
    lcd_write_nibble((value << 4) & 0xF0, mode);
}

void lcd_send_cmd(uint8_t cmd) {
    lcd_send_byte(cmd, 0);
}

void lcd_send_data(uint8_t data) {
    lcd_send_byte(data, PIN_RS);
}

void lcd_clear(void) {
    lcd_send_cmd(0x01);
    vTaskDelay(pdMS_TO_TICKS(2));
}

void lcd_set_cursor(uint8_t col, uint8_t row) {
    static const uint8_t row_offsets[] = {0x00, 0x40};
    if (row > 1) row = 1;
    lcd_send_cmd(0x80 | (col + row_offsets[row]));
}

void lcd_put_string(const char *str) {
    while (*str) {
        lcd_send_data((uint8_t)*str++);
    }
}

esp_err_t lcd_init(i2c_port_t i2c_num, uint8_t sda_pin, uint8_t scl_pin) {
    g_i2c_port = i2c_num;
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda_pin,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = scl_pin,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000
    };
    esp_err_t err = i2c_param_config(g_i2c_port, &conf);
    if (err != ESP_OK) return err;
    err = i2c_driver_install(g_i2c_port, conf.mode, 0, 0, 0);
    if (err != ESP_OK) return err;

    vTaskDelay(pdMS_TO_TICKS(50));

    // HD44780 4-bit initialization sequence
    lcd_write_nibble(0x30, 0);
    vTaskDelay(pdMS_TO_TICKS(5));
    lcd_write_nibble(0x30, 0);
    ets_delay_us(150);
    lcd_write_nibble(0x30, 0);
    lcd_write_nibble(0x20, 0);

    lcd_send_cmd(0x28); // 2 lines, 5x8 dots
    lcd_send_cmd(0x0C); // Display ON, Cursor OFF
    lcd_send_cmd(0x06); // Entry mode
    lcd_clear();

    ESP_LOGI(TAG, "LCD HD44780 initialized over I2C at 0x%02X", LCD_ADDR);
    return ESP_OK;
}

void lcd_task(void *pvParameters) {
    uint8_t screen_index = 0;
    char line1[17];
    char line2[17];

    // Initial Splash Screen
    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_put_string("SMART IRRIGATION");
    lcd_set_cursor(0, 1);
    lcd_put_string("SYSTEM STARTING");
    vTaskDelay(pdMS_TO_TICKS(2000));

    while (1) {
        system_telemetry_t local_telem;
        if (xSemaphoreTake(g_telemetry_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            local_telem = g_telemetry;
            xSemaphoreGive(g_telemetry_mutex);
        }

        memset(line1, ' ', 16); line1[16] = '\0';
        memset(line2, ' ', 16); line2[16] = '\0';

        switch (screen_index) {
        case 0:
            // Screen 1: Title & Soil Moisture
            snprintf(line1, sizeof(line1), "SMART IRRIGATION");
            snprintf(line2, sizeof(line2), "Soil: %5.1f %%", local_telem.soil_moisture_pct);
            break;

        case 1:
            // Screen 2: Temperature & Humidity
            if (local_telem.dht_valid) {
                snprintf(line1, sizeof(line1), "Temp: %5.1f C", local_telem.temperature_c);
                snprintf(line2, sizeof(line2), "Hum : %5.1f %%", local_telem.humidity_pct);
            } else {
                snprintf(line1, sizeof(line1), "Temp: DHT ERROR");
                snprintf(line2, sizeof(line2), "Hum : DHT ERROR");
            }
            break;

        case 2:
            // Screen 3: Pump & Control Mode
            snprintf(line1, sizeof(line1), "Pump: %s", (local_telem.pump_state == PUMP_STATE_ON) ? "ON " : "OFF");
            snprintf(line2, sizeof(line2), "Mode: %s", (local_telem.mode == IRRIGATION_MODE_AUTO) ? "AUTO  " : "MANUAL");
            break;

        case 3:
            // Screen 4: System Status
            if (local_telem.pump_state == PUMP_STATE_ON) {
                snprintf(line1, sizeof(line1), "SOIL DRY       ");
                snprintf(line2, sizeof(line2), "WATERING...    ");
            } else {
                snprintf(line1, sizeof(line1), "SOIL OPTIMAL   ");
                snprintf(line2, sizeof(line2), "PUMP OFF       ");
            }
            break;
        }

        // Pad line strings to 16 characters for clean rendering
        for (int i = strlen(line1); i < 16; i++) line1[i] = ' '; line1[16] = '\0';
        for (int i = strlen(line2); i < 16; i++) line2[i] = ' '; line2[16] = '\0';

        lcd_set_cursor(0, 0);
        lcd_put_string(line1);
        lcd_set_cursor(0, 1);
        lcd_put_string(line2);

        screen_index = (screen_index + 1) % 4;
        vTaskDelay(pdMS_TO_TICKS(2500)); // Timed screen change every 2.5s
    }
}
