idf_component_register(
    SRCS "app_main.c" "wifi_sta.c" "cmd_dispatch.c"
    INCLUDE_DIRS "."
    REQUIRES comm camera audio chassis proximity imu espnow
             espressif__cjson
             esp_wifi esp_netif esp_event nvs_flash esp_timer esp_driver_gpio esp_system)
