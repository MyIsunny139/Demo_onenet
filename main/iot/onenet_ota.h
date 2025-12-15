#ifndef __ONENET_OTA_H_
#define __ONENET_OTA_H_
#include "esp_err.h"

const char* get_app_version(void);

void set_app_valid(int valid);

esp_err_t onenet_ota_upload_version(void);

void onenet_ota_start(void);

#endif