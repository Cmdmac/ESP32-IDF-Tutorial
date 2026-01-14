/* 必要的头文件包含 */
#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* 定义日志标签（规范：大写字符串，方便后续筛选日志） */
static const char *TAG = "LOG_DEMO";

/**
 * @brief 程序入口函数
 */
void app_main(void)
{
    // 1. 错误等级日志（ESP_LOGE）：最高优先级，用于打印致命错误、系统异常
    ESP_LOGE(TAG, "这是【错误等级】日志 - 用于标识严重故障，如硬件初始化失败、内存溢出");

    // 2. 警告等级日志（ESP_LOGW）：次高优先级，用于打印潜在风险、非致命异常
    ESP_LOGW(TAG, "这是【警告等级】日志 - 用于标识潜在问题，如参数不合理、资源不足");

    // 3. 信息等级日志（ESP_LOGI）：默认输出等级，用于打印正常业务流程、关键状态
    ESP_LOGI(TAG, "这是【信息等级】日志 - 用于标识正常运行状态，如程序启动成功、任务创建完成");

    // 4. 调试等级日志（ESP_LOGD）：默认不输出，用于开发阶段打印调试细节、变量值
    ESP_LOGD(TAG, "这是【调试等级】日志 - 调试用，当前系统tick值：%lu", xTaskGetTickCount());

    // 5. 详细等级日志（ESP_LOGV）：最低优先级，默认不输出，用于打印极细粒度的调试信息
    ESP_LOGV(TAG, "这是【详细等级】日志 - 极细粒度调试，当前任务栈剩余大小：%u", uxTaskGetStackHighWaterMark(NULL));

    // 1. 基本整数类型（int/short/long，对应占位符 %d）
    int device_id = 10086;
    short port_num = 8080;
    long loop_count = 0;
    char *device_name = "ESP32-C3-Sensor";
    char wifi_ssid[] = "Home_WiFi_2.4G";
    ESP_LOGI(TAG, "基本整数输出 - 设备ID：%d，端口号：%d，初始计数：%ld, 设备名称：%s，连接WiFi：%s", 
             device_id, port_num, loop_count, device_name, wifi_ssid);

    printf("silicon revision v%d.%d, ", 10, 3);

    // 循环打印信息日志，方便在串口观察持续输出效果
    int loop_count = 0;
    while (1)
    {
        ESP_LOGI(TAG, "循环日志输出 - 第 %d 次（每隔1秒打印一次）", ++loop_count);
        // FreeRTOS 延时1秒（不阻塞系统，保证日志输出稳定）
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}