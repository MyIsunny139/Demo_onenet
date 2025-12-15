#ifndef _ONENET_MQTT_H_
#define _ONENET_MQTT_H_
#include "esp_err.h"
#include "mqtt_client.h"
#include "onenet_ota.h"
//产品ID
#define ONENET_PRODUCT_ID      "5x8qhBrKw5"

//产品密钥
#define ONENET_PRODUCT_ACCESS_KEY     "fYs9q1VPG7WbX2WHaU9dX29zuSu9D8dj5ynxGZM/pQc="

//设备名称
#define ONENET_DEVICE_NAME     "esp32led02"

esp_err_t onenet_start(void);

esp_err_t onenet_post_property_data(const char* data);

#endif /* _ONENET_MQTT_H_ */