# Reguły: zamiar -> polecenie, zdarzenie źródła -> stan interfejsu.
# Nie zna LVGL. Źródło snapshotów jest wymienne: dziś symulacja, jutro MQTT.
idf_component_register(
    SRCS "app_controller.cpp" "network_controller.cpp" "z906_source.cpp" "live_source.cpp"
    INCLUDE_DIRS "include"
    REQUIRES domain core transport storage
    PRIV_REQUIRES esp_timer
)
