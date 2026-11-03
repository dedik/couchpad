# Łączność: Wi-Fi przez ESP32-C6 (esp_hosted) oraz docelowo MQTT.
# Adapter wołany przez `application`; nie zna LVGL ani ekranów.
idf_component_register(
    SRCS "wifi_service.cpp" "time_sync.cpp" "mqtt_bus.cpp" "z906_client.cpp" "ha_client.cpp" "tv_client.cpp" "log_sink.cpp"
    INCLUDE_DIRS "include"
    REQUIRES core domain storage
    PRIV_REQUIRES esp_timer esp_wifi esp_netif esp_event nvs_flash lwip mqtt json esp_hw_support esp_ringbuf
)
