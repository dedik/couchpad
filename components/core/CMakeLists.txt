# Najniższa warstwa: typy i narzędzia bez wiedzy o LVGL, BSP i sieci.
idf_component_register(
    SRCS "clock.cpp" "memory.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_timer heap
    PRIV_REQUIRES freertos log
)
