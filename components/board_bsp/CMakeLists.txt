
idf_component_register(
    SRCS "esp32_p4_function_ev_board.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "priv_include"
    REQUIRES driver
    # esp_lcd_st7701_patched to łatana kopia producenta trzymana w repozytorium.
    # Wersja z rejestru daje na tym panelu czarny ekran — patrz components/README.md.
    PRIV_REQUIRES esp_lcd usb spiffs fatfs esp_lcd_st7701_patched
)
