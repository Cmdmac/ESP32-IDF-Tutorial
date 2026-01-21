/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "touch_element/touch_button.h"
#include "esp_log.h"

static const char *TAG = "Touch Button Example";

/* Button event handler task */
static void button_handler_task(void *arg)
{
    (void) arg; //Unused
    touch_elem_message_t element_message;
    while (1) {
        /* Waiting for touch element messages */
        touch_element_message_receive(&element_message, portMAX_DELAY);
        if (element_message.element_type != TOUCH_ELEM_TYPE_BUTTON) {
            continue;
        }
        /* Decode message */
        const touch_button_message_t *button_message = touch_button_get_message(&element_message);
        if (button_message->event == TOUCH_BUTTON_EVT_ON_PRESS) {
            ESP_LOGI(TAG, "Button[%d] Press", (int)element_message.arg);
        } else if (button_message->event == TOUCH_BUTTON_EVT_ON_RELEASE) {
            ESP_LOGI(TAG, "Button[%d] Release", (int)element_message.arg);
        } else if (button_message->event == TOUCH_BUTTON_EVT_ON_LONGPRESS) {
            ESP_LOGI(TAG, "Button[%d] LongPress", (int)element_message.arg);
        }
    }
}

//触摸事件回调函数
static void button_handler(touch_button_handle_t out_handle, touch_button_message_t *out_message, void *arg)
{
    (void) out_handle; //Unused
    if (out_message->event == TOUCH_BUTTON_EVT_ON_PRESS) {
        ESP_LOGI(TAG, "Button[%d] Press", (int)arg);
    } else if (out_message->event == TOUCH_BUTTON_EVT_ON_RELEASE) {
        ESP_LOGI(TAG, "Button[%d] Release", (int)arg);
    } else if (out_message->event == TOUCH_BUTTON_EVT_ON_LONGPRESS) {
        ESP_LOGI(TAG, "Button[%d] LongPress", (int)arg);
    }
}

void app_main(void)
{
    
    /* 初始化触摸组件库 */
    touch_elem_global_config_t global_config = TOUCH_ELEM_GLOBAL_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(touch_element_install(&global_config));
    ESP_LOGI(TAG, "触摸组件已安装");

    touch_button_global_config_t button_global_config = TOUCH_BUTTON_GLOBAL_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(touch_button_install(&button_global_config));
    ESP_LOGI(TAG, "Touch button installed");

    touch_button_handle_t button_handle;
    touch_pad_t touch_channel = TOUCH_PAD_NUM1;
    float channel_sens = 0.1F;
    touch_button_config_t button_config = {
            .channel_num = touch_channel,
            .channel_sens = channel_sens
        };

    /* 创建触摸按钮 */
    ESP_ERROR_CHECK(touch_button_create(&button_config, &button_handle));
    /* 订阅触摸事件如(按下, 松开, 长按) */
    ESP_ERROR_CHECK(touch_button_subscribe_event(button_handle,
                                                 TOUCH_ELEM_EVENT_ON_PRESS | TOUCH_ELEM_EVENT_ON_RELEASE | TOUCH_ELEM_EVENT_ON_LONGPRESS,
                                                 (void *)touch_channel));

    /* 设置触摸处理方式为回调方式 */
    ESP_ERROR_CHECK(touch_button_set_dispatch_method(button_handle, TOUCH_ELEM_DISP_CALLBACK));
    /* 注册消息回调函数 */
    ESP_ERROR_CHECK(touch_button_set_callback(button_handle, button_handler));

    /* 设置长按时间阈值 */
    ESP_ERROR_CHECK(touch_button_set_longpress(button_handle, 2000));

    ESP_LOGI(TAG, "触摸按钮已创建");

    touch_element_start();
    ESP_LOGI(TAG, "开始检测触摸事件");

    // // 设置处理方式为事件
    // ESP_ERROR_CHECK(touch_button_set_dispatch_method(button_handle[i], TOUCH_ELEM_DISP_EVENT));
    // // 启动循环处理
    // button_handler_task(NULL);
}