idf_component_register(SRCS "i2c_scanner.c" "lcd_task.c" "lcd_display.c" "audio_task.c" "audio_buffer_manager.c" "audio_effects.c" "audio_processor.c" "i2s_config.c" "led_control.c" "wifi_manager.c" "web_server.c" "device_params.c" "fft_analyzer.c" "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_http_server json nvs_flash esp_netif esp_event esp_wifi driver esp-dsp)
