cmake_minimum_required(VERSION 3.20)
project(pilot_phone C CXX)

# Pilot na telefonie: współdzielone komponenty firmware'u (ui, application, domain,
# core, storage, transport) z repozytorium obok, na warstwie zgodności ESP-IDF/FreeRTOS.
# Tego katalogu nie zmieniamy z tego repozytorium — tylko z niego czytamy.
set(PILOT_FIRMWARE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." CACHE PATH "Katalog firmware'u (korzeń repozytorium)")
get_filename_component(FW "${PILOT_FIRMWARE_DIR}" ABSOLUTE)
set(LVGL_DIR "${FW}/managed_components/lvgl__lvgl" CACHE PATH "Źródła LVGL 9.2 (managed_components firmware'u)")

if(NOT EXISTS "${FW}/components/ui/ui.cpp")
    message(FATAL_ERROR "Nie widzę firmware'u w ${FW} (PILOT_FIRMWARE_DIR)")
endif()
if(NOT EXISTS "${LVGL_DIR}/lvgl.h")
    message(FATAL_ERROR "Brak LVGL w ${LVGL_DIR} — uruchom w firmware `idf.py reconfigure`")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

find_package(Threads REQUIRED)

# Część instalacji Command Line Tools ma niepełny katalog usr/include/c++/v1, który
# zasłania nagłówki libc++ z SDK („'cstdio' file not found”). Wtedy wskazujemy je jawnie.
if(APPLE AND NOT ANDROID)
    execute_process(COMMAND xcrun --show-sdk-path OUTPUT_VARIABLE PILOT_SDK OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(EXISTS "/Library/Developer/CommandLineTools/usr/include/c++/v1"
       AND NOT EXISTS "/Library/Developer/CommandLineTools/usr/include/c++/v1/cstdio"
       AND EXISTS "${PILOT_SDK}/usr/include/c++/v1/cstdio")
        add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-nostdinc++>"
                            "$<$<COMPILE_LANGUAGE:CXX>:-isystem${PILOT_SDK}/usr/include/c++/v1>")
    endif()
endif()

# ---------------------------------------------------------------- SDL2
if(ANDROID)
    # SDL2 budowany ze źródeł przez projekt Gradle (android/), cel `SDL2`.
    if(NOT TARGET SDL2)
        add_subdirectory("${SDL2_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/SDL2")
    endif()
    set(PILOT_SDL SDL2)
else()
    find_package(SDL2 REQUIRED)
    set(PILOT_SDL SDL2::SDL2)
endif()

# ---------------------------------------------------------------- LVGL
file(GLOB_RECURSE LVGL_SOURCES CONFIGURE_DEPENDS "${LVGL_DIR}/src/*.c")
add_library(lvgl STATIC ${LVGL_SOURCES})
target_include_directories(lvgl PUBLIC "${LVGL_DIR}" "${LVGL_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/platform")
target_compile_definitions(lvgl PUBLIC LV_CONF_KCONFIG_EXTERNAL_INCLUDE="lv_kconfig_phone.h")
target_compile_options(lvgl PRIVATE -w)

# ---------------------------------------------------------------- warstwa zgodności
add_library(compat STATIC
    compat/src/esp.cpp
    compat/src/freertos.cpp
    compat/src/nvs.cpp
    compat/src/mqtt_client.cpp
    third_party/cJSON/cJSON.c)
target_include_directories(compat PUBLIC compat/include third_party/cJSON)
target_link_libraries(compat PUBLIC Threads::Threads)
if(ANDROID)
    target_link_libraries(compat PUBLIC log)
endif()

# ---------------------------------------------------------------- komponenty firmware'u
set(C "${FW}/components")
file(GLOB UI_FONTS CONFIGURE_DEPENDS "${C}/ui/assets/fonts/*.c")
file(GLOB UI_IMAGES CONFIGURE_DEPENDS "${C}/ui/assets/images/*.c")
file(GLOB UI_SCREENS CONFIGURE_DEPENDS "${C}/ui/screens/*.cpp")
file(GLOB UI_SOURCES CONFIGURE_DEPENDS "${C}/ui/*.cpp")

add_library(pilot_shared STATIC
    ${C}/core/clock.cpp
    ${C}/core/memory.cpp
    ${C}/domain/app_state.cpp
    ${C}/storage/settings.cpp
    ${C}/application/app_controller.cpp
    ${C}/application/network_controller.cpp
    ${C}/application/z906_source.cpp
    ${C}/application/live_source.cpp
    ${C}/transport/mqtt_bus.cpp
    ${C}/transport/z906_client.cpp
    ${C}/transport/ha_client.cpp
    ${C}/transport/tv_client.cpp
    # wifi_service.cpp, time_sync.cpp i log_sink.cpp płytki zastępuje platform/.
    platform/wifi_service_phone.cpp
    platform/time_sync_phone.cpp
    ${UI_SOURCES}
    ${UI_SCREENS}
    ${UI_FONTS}
    ${UI_IMAGES})
target_include_directories(pilot_shared PUBLIC
    ${C}/core/include
    ${C}/domain/include
    ${C}/storage/include
    ${C}/application/include
    ${C}/transport/include
    ${C}/ui/include
    ${C}/ui/assets/images
    ${C}/board/include
    platform
    PRIVATE ${C}/ui)
target_link_libraries(pilot_shared PUBLIC compat lvgl)

# ---------------------------------------------------------------- aplikacja
set(APP_SOURCES platform/board_sdl.cpp platform/app_main.cpp)
if(ANDROID)
    # SDLActivity ładuje bibliotekę `main` i woła SDL_main.
    add_library(main SHARED ${APP_SOURCES})
    target_link_libraries(main PRIVATE pilot_shared ${PILOT_SDL} android log)
else()
    add_executable(pilot_sim ${APP_SOURCES})
    target_link_libraries(pilot_sim PRIVATE pilot_shared ${PILOT_SDL})
endif()
