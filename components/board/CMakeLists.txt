# Jedyne miejsce w projekcie, które zna BSP płytki. Reszta kodu widzi tylko board.hpp.
idf_component_register(
    SRCS "board.cpp" "lv_mem_psram.c" "usb_console.cpp"
    INCLUDE_DIRS "include"
    REQUIRES board_bsp lvgl
    PRIV_REQUIRES heap esp_driver_usb_serial_jtag esp_mm
)

# LVGL woła alokator z lv_mem_psram.c, ale linker czyta bibliotekę `board` przed `lvgl`
# i bez tego pominąłby plik, którego nikt jeszcze nie potrzebował.
target_link_libraries(${COMPONENT_LIB} INTERFACE "-u lv_mem_init")
