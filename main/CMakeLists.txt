idf_component_register(
    SRCS "main.c"
         "dht22.c"
         "soil_sensor.c"
         "irrigation.c"
         "lcd.c"
         "wifi_manager.c"
         "mqtt_manager.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_timer esp_wifi esp_event nvs_flash mqtt freertos log
)
