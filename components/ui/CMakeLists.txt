# Interfejs: motyw, komponenty, ekrany i wiązanie danych LVGL.
# Zależność idzie wyłącznie w dół — `ui` zna `application`, nigdy odwrotnie.
file(GLOB UI_FONTS "assets/fonts/*.c")
file(GLOB UI_IMAGES "assets/images/*.c")

idf_component_register(
    SRCS "ui.cpp" "theme.cpp" "shell.cpp"
         "widgets.cpp" "widgets_controls.cpp" "widgets_nav.cpp" "widgets_editor.cpp"
         "screens/screen_tv.cpp"
         "screens/screen_appletv.cpp"
         "screens/screen_audio.cpp"
         "screens/screen_home.cpp"
         "screens/home_common.cpp"
         "screens/home_room.cpp"
         "screens/home_light.cpp"
         "screens/home_radiator.cpp"
         "screens/home_drawers.cpp"
         "screens/screen_climate.cpp"
         "screens/climate_tile.cpp"
         "screens/screen_start.cpp"
         "screens/screen_more.cpp"
         "screens/screen_wifi.cpp"
         "screens/screen_mqtt.cpp"
         "screens/screen_device.cpp"
         ${UI_FONTS} ${UI_IMAGES}
    INCLUDE_DIRS "include" "assets/images"
    PRIV_INCLUDE_DIRS "."
    REQUIRES lvgl application domain core transport storage
    PRIV_REQUIRES esp_app_format esp_timer esp_netif
)
