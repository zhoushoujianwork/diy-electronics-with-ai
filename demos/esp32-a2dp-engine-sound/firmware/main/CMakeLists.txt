set(VOICE "${CMAKE_CURRENT_LIST_DIR}/../../../../projects/ev-engine-sound/core")
idf_component_register(SRCS "main.c" "controls.c" "${VOICE}/engine_voice.c" "${VOICE}/commands.c"
    INCLUDE_DIRS "." "${VOICE}"
    REQUIRES bt nvs_flash esp_driver_gpio esp_driver_uart esp_timer esp_system esp_app_format)
