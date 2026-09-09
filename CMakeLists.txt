cmake_minimum_required(VERSION 3.25)

project(maclock-local LANGUAGES C CXX)

if(NOT APPLE AND NOT (WIN32 AND MINGW))
    message(FATAL_ERROR
        "The desktop simulator supports macOS and Windows through MSYS2 UCRT64")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.2.10
    GIT_SHALLOW TRUE)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.9b
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR maclock-header-only)

set(LV_BUILD_CONF_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/include/lv_conf.h"
    CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    lvgl
    GIT_REPOSITORY https://github.com/lvgl/lvgl.git
    GIT_TAG v9.4.0
    GIT_SHALLOW TRUE)

FetchContent_Declare(
    ArduinoJson
    GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git
    GIT_TAG v7.4.3
    GIT_SHALLOW TRUE)

FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG 0.11.22
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR maclock-header-only)

set(HTTPLIB_COMPILE OFF CACHE BOOL "" FORCE)
set(HTTPLIB_REQUIRE_OPENSSL OFF CACHE BOOL "" FORCE)
set(HTTPLIB_REQUIRE_ZLIB OFF CACHE BOOL "" FORCE)
set(HTTPLIB_USE_ZLIB_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    cpp_httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.18.7
    GIT_SHALLOW TRUE)

set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_LIBCURL_DOCS OFF CACHE BOOL "" FORCE)
set(BUILD_MISC_DOCS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(CURL_BROTLI OFF CACHE BOOL "" FORCE)
set(CURL_USE_GSSAPI OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBPSL OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBSSH2 OFF CACHE BOOL "" FORCE)
if(APPLE)
    set(CURL_USE_SECTRANSP ON CACHE BOOL "" FORCE)
elseif(WIN32)
    set(CURL_USE_SCHANNEL ON CACHE BOOL "" FORCE)
endif()
set(CURL_ZLIB OFF CACHE BOOL "" FORCE)
set(CURL_ZSTD OFF CACHE BOOL "" FORCE)
set(HTTP_ONLY ON CACHE BOOL "" FORCE)
set(USE_LIBIDN2 OFF CACHE BOOL "" FORCE)
set(USE_NGHTTP2 OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    curl
    GIT_REPOSITORY https://github.com/curl/curl.git
    GIT_TAG curl-8_12_1
    GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(
    SDL3 imgui lvgl ArduinoJson miniaudio cpp_httplib curl)

find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)
if(WIN32)
    if(DEFINED ENV{MACLOCK_NPM_EXECUTABLE})
        set(NPM_EXECUTABLE "$ENV{MACLOCK_NPM_EXECUTABLE}")
    else()
        find_program(NPM_EXECUTABLE npm.cmd REQUIRED)
    endif()
else()
    find_program(NPM_EXECUTABLE npm REQUIRED)
endif()
get_filename_component(NPM_DIRECTORY "${NPM_EXECUTABLE}" DIRECTORY)
if(WIN32)
    set(NPM_PATH_SEPARATOR ";")
else()
    set(NPM_PATH_SEPARATOR ":")
endif()
set(NPM_ENV_PATH "PATH=${NPM_DIRECTORY}${NPM_PATH_SEPARATOR}$ENV{PATH}")

set(CONTROL_PANEL_HEADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(CONTROL_PANEL_HEADER "${CONTROL_PANEL_HEADER_DIR}/control_panel_page.h")
add_custom_target(maclock-control-panel
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CONTROL_PANEL_HEADER_DIR}"
    COMMAND ${CMAKE_COMMAND} -E env
        "CONTROL_PANEL_HEADER_PATH=${CONTROL_PANEL_HEADER}"
        "${NPM_ENV_PATH}"
        "${NPM_EXECUTABLE}" run build
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/web/control-panel"
    BYPRODUCTS "${CONTROL_PANEL_HEADER}"
    COMMENT "Building embedded control panel")

add_library(maclock-imgui STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp")
target_include_directories(maclock-imgui PUBLIC
    "${imgui_SOURCE_DIR}"
    "${imgui_SOURCE_DIR}/backends")
target_link_libraries(maclock-imgui PUBLIC SDL3::SDL3-static)

set(MACLOCK_APPLICATION_SOURCES
    src/alarm_ui.cpp
    src/audio_service.cpp
    src/configuration_archive.cpp
    src/control_panel.cpp
    src/control_panel_sound_library.cpp
    src/datetime_ui.cpp
    src/fensoft_logo.cpp
    src/FT6336.cpp
    src/i2c_bus.cpp
    src/input_service.cpp
    src/init.cpp
    src/localization.cpp
    src/lv_font_chicago_8.c
    src/lv_font_chicago_24.c
    src/lv_font_chicago_32.c
    src/lv_font_chicago_48.c
    src/lv_font_chicago_digits_6.c
    src/lv_font_chicago_digits_10.c
    src/lv_font_chicago_digits_40.c
    src/lv_font_chicago_digits_56.c
    src/lv_font_seven_segment_24.c
    src/lv_font_seven_segment_48.c
    src/lv_font_seven_segment_64.c
    src/lv_font_seven_segment_80.c
    src/lv_font_seven_segment_96.c
    src/maclock_app.cpp
    src/maclock_hal.cpp
    src/minivmac_ArduinoAPI.cpp
    src/minivmac_OSGLUE.c
    src/mouse.cpp
    src/mqtt_service.cpp
    src/rtc_service.cpp
    src/rotary_navigator.cpp
    src/selector_list_style.cpp
    src/settings_store.cpp
    src/sound_selector.cpp
    src/timer_ui.cpp
    src/touch.cpp
    src/update_service.cpp
    src/weather_service.cpp
    src/wifi_mode.cpp
    src/alarm_editor_view.cpp
    src/alarm_ringing_view.cpp
    src/alarm_service.cpp
    src/alarm_storage.cpp
    src/configuration_archive_export.cpp
    src/configuration_archive_import.cpp
    src/configuration_archive_media.cpp
    src/configuration_archive_media_restore.cpp
    src/configuration_archive_settings.cpp
    src/configuration_archive_zip.cpp
    src/configuration_archive_zip_parser.cpp
    src/control_panel_clockface.cpp
    src/control_panel_loading.cpp
    src/control_panel_core.cpp
    src/control_panel_minivmac.cpp
    src/control_panel_routes.cpp
    src/control_panel_screensaver.cpp
    src/control_panel_settings_api.cpp
    src/control_panel_update.cpp
    src/mqtt_commands.cpp
    src/mqtt_home_assistant.cpp
    src/mqtt_message_queue.cpp
    src/mqtt_transport.cpp
    src/update_assets.cpp
    src/update_facade.cpp
    src/update_install.cpp
    src/update_manifest.cpp
    src/update_manual_upload.cpp
    src/update_ota.cpp
    src/update_release_check.cpp
    src/update_worker.cpp
    src/wifi_portal.cpp
    src/wifi_service.cpp
    src/wifi_settings.cpp
    src/wifi_station_task.cpp
    src/wifi_weather.cpp)

set(MINIVMAC_CORE_SOURCES
    src/minivmac/src/ASCEMDEV.c
    src/minivmac/src/GLOBGLUE.c
    src/minivmac/src/IWMEMDEV.c
    src/minivmac/src/KBRDEMDV.c
    src/minivmac/src/M68KITAB.c
    src/minivmac/src/MINEM68K.c
    src/minivmac/src/MOUSEMDV.c
    src/minivmac/src/PROGMAIN.c
    src/minivmac/src/ROMEMDEV.c
    src/minivmac/src/RTCEMDEV.c
    src/minivmac/src/SCCEMDEV.c
    src/minivmac/src/SCRNEMDV.c
    src/minivmac/src/SCSIEMDV.c
    src/minivmac/src/SNDEMDEV.c
    src/minivmac/src/SONYEMDV.c
    src/minivmac/src/VIAEMDEV.c)

set(MACLOCK_DESKTOP_SOURCES
    desktop/src/arduino_compat.cpp
    desktop/src/audio_compat.cpp
    desktop/src/device_compat.cpp
    desktop/src/freertos_compat.cpp
    desktop/src/host_compat.cpp
    desktop/src/local_audio_output.cpp
    desktop/src/local_hal_self_test.cpp
    desktop/src/local_maclock_hal.cpp
    desktop/src/local_mqtt_client.cpp
    desktop/src/local_simulator_ui.cpp
    desktop/src/network_compat.cpp
    desktop/src/storage_compat.cpp)

set(MACLOCK_EXECUTABLE_OPTIONS)
if(APPLE)
    list(APPEND MACLOCK_EXECUTABLE_OPTIONS MACOSX_BUNDLE)
endif()

add_executable(maclock-local ${MACLOCK_EXECUTABLE_OPTIONS}
    desktop/src/main.cpp
    ${MACLOCK_APPLICATION_SOURCES}
    ${MINIVMAC_CORE_SOURCES}
    ${MACLOCK_DESKTOP_SOURCES})
add_dependencies(maclock-local maclock-control-panel)

target_include_directories(maclock-local PRIVATE
    "${CONTROL_PANEL_HEADER_DIR}"
    desktop/compat/include
    desktop/include
    include
    include/minivmac
    src/minivmac/cfg
    src/minivmac/src
    "${miniaudio_SOURCE_DIR}")

target_compile_definitions(maclock-local PRIVATE
    MACLOCK_LOCAL=1
    MACLOCK_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
    LV_CONF_INCLUDE_SIMPLE
    LV_LVGL_H_INCLUDE_SIMPLE
    USE_WIRE
    UseLargeScreenHack=1
    LCD_W=320
    LCD_H=240
    TFT_BL_VAR=45
    I2S_EN=1
    I2S_MCK=4
    I2S_BCK=5
    I2S_DOUT=8
    I2S_WS=7
    I2C_SCL=15
    I2C_SDA=16
    GPIO_TOUCH=2
    GPIO_FLOPPY=47
    GPIO_ALARM=40
    GPIO_CLOCK=48
    GPIO_ENCODER1=14
    GPIO_ENCODER2=21
    GPIO_CHARGING=43
    GPIO_BAT_EN=44
    GPIO_BAT_ADC=9
    TOUCH_FT6336_INT=17
    TOUCH_FT6336_RST=18)

target_compile_options(maclock-local PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)

target_link_libraries(maclock-local PRIVATE
    lvgl
    ArduinoJson
    maclock-imgui
    httplib::httplib
    CURL::libcurl
    ZLIB::ZLIB
    Threads::Threads)

if(WIN32)
    target_compile_definitions(maclock-local PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX)
    target_link_libraries(maclock-local PRIVATE
        ws2_32 kernel32 shell32)
endif()

set_target_properties(maclock-local PROPERTIES
    OUTPUT_NAME "Maclock Simulator")
if(APPLE)
    set_target_properties(maclock-local PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "Maclock Simulator"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.fensoft.maclock-simulator"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
        MACOSX_BUNDLE_BUNDLE_VERSION "1")
endif()

target_compile_definitions(maclock-local PRIVATE
    $<$<CONFIG:Debug>:MACLOCK_LOCAL_DEBUG=1>)

include(CTest)
if(BUILD_TESTING)
    add_test(
        NAME maclock-local-config-startup
        COMMAND $<TARGET_FILE:maclock-local>
            --headless
            --startup config
            --state-dir
                "${CMAKE_BINARY_DIR}/local-test-state"
            --reset-state
            --run-for-ms 2500
            --framebuffer-out
                "${CMAKE_BINARY_DIR}/local-test-framebuffer.ppm")
    set_tests_properties(
        maclock-local-config-startup PROPERTIES
        PASS_REGULAR_EXPRESSION "RTC: .* OK"
        TIMEOUT 20)

    add_test(
        NAME maclock-local-hal-persistence-write
        COMMAND $<TARGET_FILE:maclock-local>
            --state-dir
                "${CMAKE_BINARY_DIR}/local-hal-self-test"
            --http-port 18080
            --reset-state
            --self-test write)
    set_tests_properties(
        maclock-local-hal-persistence-write PROPERTIES
        FIXTURES_SETUP local-hal-persistence
        PASS_REGULAR_EXPRESSION
            "Local HAL self-test \\(write\\) passed"
        TIMEOUT 20)

    add_test(
        NAME maclock-local-hal-persistence-verify
        COMMAND $<TARGET_FILE:maclock-local>
            --state-dir
                "${CMAKE_BINARY_DIR}/local-hal-self-test"
            --http-port 18080
            --self-test verify)
    set_tests_properties(
        maclock-local-hal-persistence-verify PROPERTIES
        FIXTURES_REQUIRED local-hal-persistence
        PASS_REGULAR_EXPRESSION
            "Local HAL self-test \\(verify\\) passed"
        TIMEOUT 20)
endif()
