#include "onenet_dm.h"
#include "led_ws2812.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "string.h"
#include "led_ws2812.h"
static ws2812_strip_handle_t ws2812_handele = NULL;

static int led_brightness = 0;
static int led_status = 0;

static int ws2812_red = 0;
static int ws2812_green = 0;
static int ws2812_blue = 0;


void onenet_dm_init(void)
{
    ws2812_init(GPIO_NUM_48,1, &ws2812_handele); //初始化一个WS2812

    //配置LEDC定时器
    ledc_timer_config_t led_timer =
    {
        .clk_cfg = LEDC_AUTO_CLK,
        .duty_resolution = LEDC_TIMER_12_BIT,
        .freq_hz = 5000,
        .timer_num = LEDC_TIMER_0,
    };
    ledc_timer_config(&led_timer);

    //配置pwm通道
    ledc_channel_config_t led_channel =
    {
        .channel    = LEDC_CHANNEL_0,
        .duty       = 0,
        .gpio_num   = GPIO_NUM_2,
        .timer_sel  = LEDC_TIMER_0
    };
    ledc_channel_config(&led_channel);

    ledc_fade_func_install(0); //安装ledc渐变函数
}
/*
{
  "id": "123",
  "version": "1.0",
  "params": {
    "Brightness":"50",
    "LightSwitch":"1",
    "RGBColor":
    {
        "R":"255",
        "G":"0",
        "B":"0",
    }
  }
}
*/
void onenet_property_handle(cJSON * property) //处理属性设置下行
{
    cJSON * param_js = cJSON_GetObjectItem(property, "params");
    if(param_js == NULL)
        return;
    cJSON* name_js = param_js->child; //第一个属性
    while(name_js)
    {
        if(strcmp(name_js->string,"Brightness") == 0) //亮度属性
        {
            led_brightness = (int)cJSON_GetNumberValue(name_js);
            int duty = (led_brightness * 4095) / 100; //计算占空比
            ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty, 0); //设置占空比
        }
        if(strcmp(name_js->string,"LightSwitch") == 0) //开关属性
        {
            if(cJSON_IsTrue(name_js))
            {
                led_status = 1;
                ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 4095, 0); //设置占空比
            }
            else
            {
                led_status = 0;
                ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0, 0); //关闭LED
            }
        }
        if(strcmp(name_js->string,"RGBColor") == 0) //颜色属性
        {
            ws2812_red = (int)cJSON_GetNumberValue(cJSON_GetObjectItem(name_js, "Red")) ;
            ws2812_green = (int)cJSON_GetNumberValue(cJSON_GetObjectItem(name_js, "Green")) ;
            ws2812_blue = (int)cJSON_GetNumberValue(cJSON_GetObjectItem(name_js, "Blue")) ;
            ws2812_write( ws2812_handele, 0, ws2812_red, ws2812_green, ws2812_blue); //设置LED颜色
        }
        name_js = name_js->next;
    }
}
/*
{
  "id": "123",
  "version": "1.0",
  "params": {
    "Brightness": {
      "value": "50",
    },
    "LightSwitch": {
      "value": true,
    },
    "RGBColor": {
        "value": {
        "Red":"255",
        "Green":"0",
        "Blue":"0",
        }   
    }
  }
}
*/


cJSON* onenet_property_upload(void) //上传属性数据
{
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "id", "123");
    cJSON_AddStringToObject(root, "version", "1.0");
    cJSON* params_js    =   cJSON_AddObjectToObject(root, "params");

    //亮度属性
    cJSON* brightness_js =  cJSON_AddObjectToObject(params_js, "Brightness");
    cJSON_AddNumberToObject(brightness_js, "value", led_brightness);

    //开关属性
    cJSON* lightswitch_js = cJSON_AddObjectToObject(params_js, "LightSwitch");
    cJSON_AddBoolToObject(lightswitch_js, "value", led_status ? true : false);

    //颜色属性
    cJSON* color_js   =   cJSON_AddObjectToObject(params_js, "RGBColor");
    cJSON* color_value_js = cJSON_AddObjectToObject(color_js, "value");
    cJSON_AddNumberToObject(color_value_js, "Red", ws2812_red);
    cJSON_AddNumberToObject(color_value_js, "Green", ws2812_green);
    cJSON_AddNumberToObject(color_value_js, "Blue", ws2812_blue);
    return root;
}


cJSON* onenet_property_get_reply(void) //构造属性获取回复数据
{
    cJSON* data_js = cJSON_CreateObject();
    
    // 亮度属性
    cJSON_AddNumberToObject(data_js, "Brightness", led_brightness);
    
    // 开关属性
    cJSON_AddBoolToObject(data_js, "LightSwitch", led_status ? true : false);
    
    // RGB颜色属性
    cJSON* color_js = cJSON_CreateObject();
    cJSON_AddNumberToObject(color_js, "Red", ws2812_red);
    cJSON_AddNumberToObject(color_js, "Green", ws2812_green);
    cJSON_AddNumberToObject(color_js, "Blue", ws2812_blue);
    cJSON_AddItemToObject(data_js, "RGBColor", color_js);
    
    return data_js;
}

