# Model stanu i zamiary użytkownika. Nie zna LVGL ani ESP-IDF poza `core`.
idf_component_register(
    SRCS "app_state.cpp"
    INCLUDE_DIRS "include"
    REQUIRES core
)
