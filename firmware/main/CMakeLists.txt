idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp32-camera driver esp_timer spiffs bt nvs_flash esp_psram posix_stub)