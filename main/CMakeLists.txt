idf_component_register(SRCS "proyecto_3_embebidos.c" "dht11_utils.c" "led_utils.c" "buzzer_utils.c"
                    INCLUDE_DIRS "include"
                    REQUIRES mqtt esp_wifi nvs_flash esp_driver_gpio esp_rom json esp_driver_ledc)

if(WIFI_SSID)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE WIFI_SSID="${WIFI_SSID}")
endif()

if(WIFI_PASS)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE WIFI_PASS="${WIFI_PASS}")
endif()
