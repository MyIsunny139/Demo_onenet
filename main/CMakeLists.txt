idf_component_register(SRCS "Demo_onenet.c"
                             "led_ws2812.c"
                             "iot/onenet_mqtt.c"
                             "iot/onenet_token.c"
                             "iot/wifi_manager.c"
                             "iot/onenet_dm.c"
                             "iot/onenet_ota.c"
                    INCLUDE_DIRS "." "iot")
