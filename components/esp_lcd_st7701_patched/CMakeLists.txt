idf_component_register(SRCS "esp_lcd_st7701.c" "esp_lcd_st7701_rgb.c" "esp_lcd_st7701_mipi.c"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "priv_include"
                       REQUIRES "esp_lcd" "driver")

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})

# cu_pkg_define_version buduje nazwy makr z nazwy katalogu, a ten katalog nosi
# przyrostek `_patched`. Źródła producenta logują wersję pod oryginalnymi nazwami,
# więc podstawiamy je tutaj zamiast ruszać jego kod.
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    "ESP_LCD_ST7701_VER_MAJOR=ESP_LCD_ST7701_PATCHED_VER_MAJOR"
    "ESP_LCD_ST7701_VER_MINOR=ESP_LCD_ST7701_PATCHED_VER_MINOR"
    "ESP_LCD_ST7701_VER_PATCH=ESP_LCD_ST7701_PATCHED_VER_PATCH")
