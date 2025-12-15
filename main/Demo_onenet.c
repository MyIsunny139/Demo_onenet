#include <stdio.h>
#include "onenet_mqtt.h"
#include "wifi_manager.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "onenet_dm.h"

static EventGroupHandle_t wifi_event_group = NULL;
#define WIFI_CONNECTED_BIT    BIT0

static void wifi_state_callback(WIFI_STATE event)
{
    switch (event) {
        case WIFI_STATE_CONNECTED:
            printf("WIFI_MANAGER_EVENT_STA_GOT_IP\r\n");
            xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
            break;
        case WIFI_STATE_DISCONNECTED:
            printf("WIFI_MANAGER_EVENT_STA_DISCONNECTED\r\n");
            break;
        default:
            break;
    }
}

void app_main(void)
{
    // 初始化NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_event_group = xEventGroupCreate();
    onenet_dm_init();
    wifi_manager_init(wifi_state_callback);
    wifi_manager_connect("MY_AP","my666666");
    EventBits_t ev ;
    while (1)
    {
        ev = xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT,
                                 pdTRUE, pdFALSE, portMAX_DELAY);
        if(ev & WIFI_CONNECTED_BIT)
        {
            printf("WiFi connected, start onenet mqtt...\r\n");
            onenet_start();
        }
    }
}
