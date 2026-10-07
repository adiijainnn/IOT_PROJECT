#ifndef LCD_H
#define LCD_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/i2c.h"

#define LCD_ADDR 0x27

esp_err_t lcd_init(i2c_port_t i2c_num, uint8_t sda_pin, uint8_t scl_pin);
void lcd_clear(void);
void lcd_set_cursor(uint8_t col, uint8_t row);
void lcd_put_string(const char *str);
void lcd_send_cmd(uint8_t cmd);
void lcd_send_data(uint8_t data);
void lcd_task(void *pvParameters);

#endif // LCD_H
