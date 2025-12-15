#include "onenet_mqtt.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "string.h"
#include "stdio.h"
#include "stdint.h"
#include "onenet_token.h"
#include "cJSON.h"
#include "esp_wifi.h"
#include "onenet_dm.h"

#define TAG "ONENET_MQTT"

static esp_mqtt_client_handle_t mqtt_handle = NULL;

static void onenet_property_ack(const char* id,int code,const char* msg);

static void onenet_subscribe(void);

static esp_err_t onenet_property_get_ack(const char* id, int code, const char* msg);

static void onenet_ota_ack(const char* id,int code,const char* msg);


/**
 * mqtt连接事件处理函数
 * @param event 事件参数
 * @return 无
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
    {
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        
        onenet_subscribe();
        cJSON* property_js = onenet_property_upload(); //上传属性数据
        char * data = cJSON_PrintUnformatted(property_js); //转换为字符串
        onenet_post_property_data(data); //设备上线后，上传属性数据
        onenet_ota_upload_version(); //连接成功后上传固件版本号
        set_app_valid(1); //设置当前应用为有效,取消回滚
        free(data);
        cJSON_Delete(property_js);
        break;
    }
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);

        if(strstr(event->topic,"property/set")) //属性设置下行
        {
            cJSON* property_json = cJSON_Parse(event->data);
            cJSON* id_js = cJSON_GetObjectItem(property_json,"id");
            onenet_property_handle(property_json); //处理属性设置下行
            onenet_property_ack(cJSON_GetStringValue(id_js), 200, "success");
            cJSON_Delete(property_json);
        }

        else if(strstr(event->topic,"property/get")) //属性获取下行
        {
            ESP_LOGI(TAG, "Received property get request");
            cJSON* request_json = cJSON_Parse(event->data);
            cJSON* id_js = cJSON_GetObjectItem(request_json,"id");
            
            onenet_property_get_ack(cJSON_GetStringValue(id_js), 200, "success");
                
            cJSON_Delete(request_json);
        }
        else if(strstr(event->topic,"ota/inform")) //OTA更新下行
        {
            cJSON* ota_json = cJSON_Parse(event->data);
            cJSON* id_js = cJSON_GetObjectItem(ota_json,"id");
            onenet_ota_ack(cJSON_GetStringValue(id_js), 200, "success");
            cJSON_Delete(ota_json);
            // 处理OTA更新逻辑
            
            //开启OTA升级任务
            onenet_ota_start();

        }


        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}


esp_err_t onenet_start(void)
{
    esp_mqtt_client_config_t mqtt_config;
    memset(&mqtt_config, 0, sizeof(esp_mqtt_client_config_t));
    mqtt_config.broker.address.uri = "mqtt://mqtts.heclouds.com";
    mqtt_config.broker.address.port = 1883;

    mqtt_config.credentials.client_id = ONENET_DEVICE_NAME;
    mqtt_config.credentials.username = ONENET_PRODUCT_ID;
    
    static char token[256];
    dev_token_generate(token,SIG_METHOD_SHA256,2074816291,ONENET_PRODUCT_ID,ONENET_DEVICE_NAME,ONENET_PRODUCT_ACCESS_KEY);
    mqtt_config.credentials.authentication.password = token;

    mqtt_handle = esp_mqtt_client_init(&mqtt_config);
    esp_mqtt_client_register_event(mqtt_handle, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    return esp_mqtt_client_start(mqtt_handle);
}

static void onenet_property_ack(const char* id,int code,const char* msg)
{
    char topic[128];
    sprintf(topic,"$sys/%s/%s/thing/property/set_reply",ONENET_PRODUCT_ID,ONENET_DEVICE_NAME);
    cJSON* replay_js = cJSON_CreateObject();
    cJSON_AddStringToObject(replay_js,"id",id);
    cJSON_AddNumberToObject(replay_js,"code",code);
    cJSON_AddStringToObject(replay_js,"msg",msg);
    char* replay_str = cJSON_PrintUnformatted(replay_js);
    esp_mqtt_client_publish(mqtt_handle,topic,replay_str,strlen(replay_str),1,0);//发布属性设置回复
    cJSON_Delete(replay_js);
    free(replay_str);
}

static void onenet_ota_ack(const char* id,int code,const char* msg)
{
    char topic[128];
    sprintf(topic,"$sys/%s/%s/ota/inform_reply",ONENET_PRODUCT_ID,ONENET_DEVICE_NAME);
    cJSON* replay_js = cJSON_CreateObject();
    cJSON_AddStringToObject(replay_js,"id",id);
    cJSON_AddNumberToObject(replay_js,"code",code);
    cJSON_AddStringToObject(replay_js,"msg",msg);
    char* replay_str = cJSON_PrintUnformatted(replay_js);
    esp_mqtt_client_publish(mqtt_handle,topic,replay_str,strlen(replay_str),1,0);//发布属性设置回复
    cJSON_Delete(replay_js);
    free(replay_str);
}


static void onenet_subscribe(void)
{
    char topic[128]; 
    sprintf(topic,"$sys/%s/%s/thing/property/post/reply",ONENET_PRODUCT_ID,ONENET_DEVICE_NAME);
    esp_mqtt_client_subscribe_single(mqtt_handle,topic,1); //订阅上报属性回复主题

    sprintf(topic,"$sys/%s/%s/thing/property/set",ONENET_PRODUCT_ID,ONENET_DEVICE_NAME);
    esp_mqtt_client_subscribe_single(mqtt_handle,topic,1); //订阅属性设置下行主题

    sprintf(topic,"$sys/%s/%s/thing/property/get",ONENET_PRODUCT_ID,ONENET_DEVICE_NAME);
    esp_mqtt_client_subscribe_single(mqtt_handle,topic,1); //订阅属性获取下行主题

    sprintf(topic,"$sys/%s/%s/thing/property/get/reply",ONENET_PRODUCT_ID,ONENET_DEVICE_NAME);
    esp_mqtt_client_subscribe_single(mqtt_handle,topic,1); //订阅属性获取回复主题

    sprintf(topic,"$sys/%s/%s/ota/inform",ONENET_PRODUCT_ID,ONENET_DEVICE_NAME);
    esp_mqtt_client_subscribe_single(mqtt_handle,topic,1); //订阅OTA更新下行主题



}

esp_err_t onenet_post_property_data(const char* data)
{
    char topic[128];
    sprintf(topic,"$sys/%s/%s/thing/property/post",ONENET_PRODUCT_ID,ONENET_DEVICE_NAME);
    ESP_LOGI(TAG,"upload topic:%s,payload:%s",topic,data);
    esp_mqtt_client_publish(mqtt_handle,topic,data,strlen(data),1,0);//发布属性上报数据
    return ESP_OK;
}

static esp_err_t onenet_property_get_ack(const char* id, int code, const char* msg)
{
    // 构造回复消息
    cJSON* reply_js = cJSON_CreateObject();
    cJSON_AddStringToObject(reply_js, "id", id);
    cJSON_AddNumberToObject(reply_js, "code", code);
    cJSON_AddStringToObject(reply_js, "msg", msg);
    
    // 使用onenet_property_get_reply获取设备属性数据
    cJSON* data_js = onenet_property_get_reply();
    cJSON_AddItemToObject(reply_js, "data", data_js);
    
    char * data = cJSON_PrintUnformatted(reply_js);
    char topic[128];
    sprintf(topic,"$sys/%s/%s/thing/property/get_reply",ONENET_PRODUCT_ID,ONENET_DEVICE_NAME);
    
    ESP_LOGI(TAG,"Property get reply topic:%s, payload:%s",topic,data);
    esp_mqtt_client_publish(mqtt_handle,topic,data,strlen(data),1,0);//发布属性获取回复
    
    free(data);
    cJSON_Delete(reply_js);
    return ESP_OK;
}


