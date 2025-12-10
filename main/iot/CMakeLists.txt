idf_component_register(SRCS  "onenet_mqtt.c" "onenet_token.c" "wifi_manager.c" "onenet_dm.c" "onenet_ota.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi nvs_flash esp_event esp_netif esp_tls json mqtt drivers
                    )
