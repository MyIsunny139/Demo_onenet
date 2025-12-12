#ifndef __ONENET_OTA_H_
#define __ONENET_OTA_H

const char* get_app_version(void);

void set_app_valid(int valid);

esp_err_t onenet_ota_upload_version(void);

#endif