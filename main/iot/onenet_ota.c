#include "onenet_ota.h"
#include "esp_ota_ops.h"
#include "stdio.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "onenet_token.h"
#include "onenet_mqtt.h"
#include "string.h"
#include "cJSON.h"


#define TAG "ONENET_OTA"

#define ONENET_OTA_URL "http://iot-api.heclouds.com/fuse-ota"

#define TOKEN_VALIDITY_PERIOD 2074816291 //token有效期，单位秒

static char target_version[32] = {0};
static int task_id = 0;

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

#define OTA_BUFF_LEN 1024
static uint8_t ota_data_buffer[OTA_BUFF_LEN];
static int ota_data_size = 0;


esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
        {
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            printf("HTTP_EVENT_ON_DATA : %.*s\r\n", evt->data_len, (char*)evt->data);
            int copy_len = 0;
            if(evt->data_len >OTA_BUFF_LEN - ota_data_size) //
            {
                copy_len = OTA_BUFF_LEN - ota_data_size;
            }
            else
            {
                copy_len = evt->data_len;
            }
            memcpy(&ota_data_buffer[ota_data_size], evt->data, copy_len);
            ota_data_size += copy_len;
            break;
        }
            
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGI(TAG, "HTTP_EVENT_REDIRECT");
            break;
        default:
            break;
    }
    return ESP_OK;
}

/*
Content-Type: application/json

Authorization:version=2022-05-01&res=userid%2F112&et=1662515432&method=sha1&sign=Pd14JLeTo77e0FOpKN8bR1INPLA%3D 

host:iot-api.heclouds.com

Content-Length:20       长度会自己计算，不需要你做
*/

static esp_err_t onenet_ota_http_connect(const char *url,esp_http_client_method_t method,const char *payload)
{
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = _http_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    char *token = (char *)malloc(256);
    memset(token,0,256);
    dev_token_generate(token,SIG_METHOD_SHA256,TOKEN_VALIDITY_PERIOD,ONENET_PRODUCT_ID,NULL,ONENET_PRODUCT_ACCESS_KEY);
    ESP_LOGI(TAG,"OTA token:%s",token);
    // POST
    esp_http_client_set_method(client, method);
    esp_http_client_set_header(client,"Content-Type", "application/json");
    esp_http_client_set_header(client,"Authorization",token);
    esp_http_client_set_header(client,"host","iot-api.heclouds.com");
    
    if(payload)
    {
        ESP_LOGI(TAG, "Payload: %s", payload);
        esp_http_client_set_post_field(client, payload, strlen(payload));
    }
    memset(ota_data_buffer,0,sizeof(ota_data_buffer));
    ota_data_size = 0;
    esp_err_t err = esp_http_client_perform(client); //执行HTTP请求
    free(token);
    esp_http_client_cleanup(client);
    return err;
}



esp_err_t onenet_ota_upload_version(void) //
{
    char url[128] = {0};
    char version[128] = {0};
    const char * app_version = get_app_version();
    esp_err_t ret = ESP_FAIL;
    snprintf(url,sizeof(url),ONENET_OTA_URL"/%s/%s/version",ONENET_PRODUCT_ID,ONENET_DEVICE_NAME);
    snprintf(version,sizeof(version),"{\"s_version\":\"%s\", \"f_version\": \"%s\"}",app_version, app_version);
    if(onenet_ota_http_connect(url,HTTP_METHOD_POST,version) == ESP_OK)
    {
        cJSON *root = cJSON_Parse((const char*)ota_data_buffer); //解析JSON数据
        if(root)
        {
            cJSON *code_js = cJSON_GetObjectItem(root, "code");
            if(code_js && cJSON_GetNumberValue(code_js) == 0)
            {
                ESP_LOGE(TAG, "OTA version upload failed, code: %d", code_js->valueint);
                ret = ESP_OK;
                cJSON_Delete(root);
            }
            cJSON_Delete(root);
        }
    }
    if(ret != ESP_OK)
    {
        ESP_LOGE(TAG, "OTA version upload failed");
    }
    return ret;
}

/*
检测升级任务，esp32通过发送下面这一堆来获取升级任务信息：
（第2到4已在onenet_ota_http_connect里面生成过了）

GET http://iot-api.heclouds.com/fuse-ota/{pro_id}/{dev_name}/check?type=1&version=1.2
Content-Type: application/json

Authorization:version=2022-05-01&res=userid%2F112&et=1662515432&method=sha1&sign=Pd14JLeTo77e0FOpKN8bR1INPLA%3D

host:iot-api.heclouds.com

Content-Length:20

发送完后，回复如下：
（只需要关注code，data里面的target，tid）
{
	"code": 0,
	"msg": "succ",
	"request_id": "**********",
	"data": {
		"target": "1.2", // 升级任务的目标版本
		"tid": 12, //任务ID
		"size": 123, //文件大小
		"md5": "dfkdajkfd", //升级文件的md5
		"status": 1 | 2 | 3, //1 ：待升级， 2 ：下载中， 3 ：升级中
		"type": 1 | 2 // 1:完整包，2：差分包  
	}
}
*/

esp_err_t onenet_ota_check_task(const char* type,const char* version)
{
    char url[128] = {0};
    const char * app_version = get_app_version();
    esp_err_t ret = ESP_FAIL;
    snprintf(url,sizeof(url),ONENET_OTA_URL"/%s/%s/check?type=%s&version=%s",ONENET_PRODUCT_ID,ONENET_DEVICE_NAME,type,version);
    if(ESP_OK == onenet_ota_http_connect(url,HTTP_METHOD_GET,NULL))
    {
        cJSON *root = cJSON_Parse((const char*)ota_data_buffer); //解析JSON数据
        if(root)
        {
            cJSON *code_js = cJSON_GetObjectItem(root, "code");
            cJSON *data_js = cJSON_GetObjectItem(root, "data");
            cJSON *target_js = cJSON_GetObjectItem(root, "target");
            cJSON *tid_js = cJSON_GetObjectItem(root, "tid");

            if(code_js && cJSON_GetNumberValue(code_js) == 0)
            {
                if(data_js && target_js && tid_js)
                {
                    snprintf(target_version,sizeof(target_version),"%s",cJSON_GetStringValue(target_js));
                    task_id = cJSON_GetNumberValue(tid_js);
                    ESP_LOGI(TAG, "OTA new version available: %s, download url: %s, tid: %s", target_version, download_url, tid);
                    ret = ESP_OK;
                }
            }
            else
            {
                ESP_LOGE(TAG, "OTA check task failed");
            }
            cJSON_Delete(root);
        }
    }
    return ret;
}


/*
设备可以根据升级包的下载情况，上报下载进度和下载结果。
下面是具体内容

POST http://iot-api.heclouds.com/fuse-ota/{pro_id}/{dev_name}/{tid}/status


Content-Type: application/json

Authorization:version=2022-05-01&res=userid%2F112&et=1662515432&method=sha1&sign=Pd14JLeTo77e0FOpKN8bR1INPLA%3D 

host:iot-api.heclouds.com

Content-Length:20


{"step":10} 

回复：
{
	"code": 0,
	"msg": "succ",
	"request_id": "**********"
}
这里依然只关注code字段，0表示成功，非0表示失败。
*/


esp_err_t onenet_ota_download_upload(int tid,int step)//上报升级状态（固件下载进度百分比）
{
    char url[128] = {0};
    char payload[64] = {0};
    esp_err_t ret = ESP_FAIL;
    snprintf(url,sizeof(url),ONENET_OTA_URL"/%s/%s/%s/status",ONENET_PRODUCT_ID,ONENET_DEVICE_NAME,tid);
    snprintf(payload,sizeof(payload),"{\"step\":%d}",step);
    if(onenet_ota_http_connect(url,HTTP_METHOD_POST,payload) = ESP_OK)
    {
        cJSON *root = cJSON_Parse((const char*)ota_data_buffer); //解析JSON数据
        if(root)
        {
            cJSON *code_js = cJSON_GetObjectItem(root, "code");//
            if(code_js && cJSON_GetNumberValue(code_js) == 0)
            {
                ESP_LOGE(TAG, "OTA version upload code: %d", code_js->valueint); 
                ret = ESP_OK;
                cJSON_Delete(root);
            }
            cJSON_Delete(root);
        }
    }
    if(ret != ESP_OK)
    {
        ESP_LOGE(TAG, "OTA version upload failed");
    }
    return ret;
}

