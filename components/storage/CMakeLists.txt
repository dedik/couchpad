# Ustawienia urządzenia w NVS. Sekrety żyją wyłącznie tutaj — nigdy w repozytorium.
idf_component_register(
    SRCS "settings.cpp"
    INCLUDE_DIRS "include"
    REQUIRES core
    PRIV_REQUIRES nvs_flash
)
