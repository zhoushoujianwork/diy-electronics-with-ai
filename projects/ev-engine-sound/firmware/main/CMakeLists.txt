set(EV_BOARD "legacy_esp32s3" CACHE STRING "Engine-sound board profile")
set(EV_BOARD_DIR "${CMAKE_CURRENT_LIST_DIR}/../config/boards/${EV_BOARD}")
if(NOT EXISTS "${EV_BOARD_DIR}/board_config.h" OR
   NOT EXISTS "${EV_BOARD_DIR}/board_audio.c" OR
   NOT EXISTS "${EV_BOARD_DIR}/board_ui.c")
    message(FATAL_ERROR "Unknown EV_BOARD='${EV_BOARD}' (missing ${EV_BOARD_DIR})")
endif()
message(STATUS "EV engine-sound board: ${EV_BOARD}")

set(EV_REQUIRES esp_driver_i2s esp_driver_i2c esp_driver_gpio esp_timer esp_tinyusb)
if(EV_BOARD STREQUAL "lichuang_szp" OR EV_BOARD STREQUAL "m5_sticks3")
    list(APPEND EV_REQUIRES esp_driver_spi esp_driver_ledc esp_lcd
         esp_lvgl_port lvgl)
endif()
if(EV_BOARD STREQUAL "lichuang_szp")
    list(APPEND EV_REQUIRES esp_lcd_touch_ft5x06)
endif()

idf_component_register(
    SRCS "main.c" "../../core/engine_voice.c" "../../core/commands.c" "../../core/engine_canvas.c" "../../core/powertrain_canvas.c" "../../core/stick_controls.c"
         "${EV_BOARD_DIR}/board_audio.c" "${EV_BOARD_DIR}/board_ui.c"
    INCLUDE_DIRS "." "../../core" "${EV_BOARD_DIR}"
    REQUIRES ${EV_REQUIRES})
