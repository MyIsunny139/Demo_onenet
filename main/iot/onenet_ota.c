#include "onenet_ota.h"
#include "esp_ota_ops.h"
#include "stdio.h"
#include "esp_log.h"
const char* get_app_version(void) //获取当前应用版本号
{
    static char app_version[32] = {0};
    if(app_version[0] != 0)
    {
        const esp_partition_t * running =  esp_ota_get_running_partition(); //获取当前运行分区
        esp_app_desc_t app_desc;
        esp_ota_get_partition_description(running ,&app_desc);  //获取分区描述信息
        snprintf(app_version, sizeof(app_version), "%s", app_desc.version); //获取版本号
    }
    return app_version;
}

void set_app_valid(int valid) //
{
    const esp_partition_t * running =  esp_ota_get_running_partition(); //获取当前运行分区
    esp_ota_img_states_t ota_state;
    //获取当前分区OTA状态
    if(esp_ota_get_state_partition(running, &ota_state)== ESP_OK)
    {
        if(ota_state == ESP_OTA_IMG_PENDING_VERIFY) //如果当前状态是待验证且需要设置为有效
        {
            if(valid)
            {
                esp_ota_mark_app_valid_cancel_rollback(); //设置当前分区为有效，取消回滚
                ESP_LOGE("Set app valid success\r\n");
            }
            else
            {
                esp_ota_mark_app_invalid_rollback_and_reboot(); //设置当前分区为无效，回滚到上一个分区并重启
                ESP_LOGE("Set app invalid and reboot\r\n");
            }
        }
    }
}

