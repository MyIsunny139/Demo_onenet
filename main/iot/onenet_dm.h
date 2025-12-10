#ifndef ONENET_DM_H
#define ONENET_DM_H
#include "cJSON.h"


void onenet_dm_init(void);

void onenet_property_handle(cJSON * property);

cJSON* onenet_property_upload(void);

cJSON* onenet_property_get_reply(void);
#endif // ONENET_DM_H