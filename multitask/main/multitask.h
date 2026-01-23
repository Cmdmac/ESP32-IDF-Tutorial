#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

// 1. 传感器数据结构体（队列传输的数据类型）
typedef struct {
    float temperature; // 温度
    float humidity;    // 湿度
    int sample_id;     // 采样编号
} sensor_data_t;

// 2. 全局通信/同步对象
QueueHandle_t sensor_queue;       // 采集→处理的队列
SemaphoreHandle_t data_mutex;    // 保护平均值的互斥锁
TaskHandle_t collect_task_handle;// 采集任务句柄（用于挂起/恢复）

// 3. 共享数据（需互斥锁保护）
static float temp_avg = 0.0f;
static float humi_avg = 0.0f;
static int sample_count = 0;

// ====================== 任务1：传感器数据采集 ======================
void sensor_collect_task(void *arg) {
    sensor_data_t data = {0};
    while (1) {
        // 模拟传感器数据采集（随机值，实际场景替换为硬件读取）
        data.temperature = 25.0f + (rand() % 100) / 10.0f; // 25.0~34.9℃
        data.humidity = 40.0f + (rand() % 300) / 10.0f;    // 40.0~69.9%
        data.sample_id = sample_count + 1;

        // 发送数据到队列（阻塞超时100ms，避免队列满导致卡死）
        if (xQueueSend(sensor_queue, &data, pdMS_TO_TICKS(100)) == pdPASS) {
            ESP_LOGD(TAG, "采集数据：ID=%d，温度=%.1f℃，湿度=%.1f%%",
                     data.sample_id, data.temperature, data.humidity);
        } else {
            ESP_LOGE(TAG, "队列满，采集数据发送失败！");
        }

        // 监控栈剩余空间（调试用，量产可删除）
        uint32_t free_stack = uxTaskGetStackHighWaterMark(NULL);
        if (free_stack < 512) { // 剩余栈<512字节，告警
            ESP_LOGW(TAG, "采集任务栈剩余不足：%d字节", free_stack);
        }

        vTaskDelay(pdMS_TO_TICKS(500)); // 500ms采集一次
    }
    vTaskDelete(NULL); // 任务退出（循环不会执行到这里）
}

// ====================== 任务2：数据处理（计算平均值） ======================
void data_process_task(void *arg) {
    sensor_data_t recv_data;
    while (1) {
        // 从队列接收数据（永久阻塞，直到有数据）
        if (xQueueReceive(sensor_queue, &recv_data, portMAX_DELAY) == pdPASS) {
            // 加互斥锁，保护共享的平均值变量
            xSemaphoreTake(data_mutex, portMAX_DELAY);
            
            // 计算平均值
            temp_avg = (temp_avg * sample_count + recv_data.temperature) / (sample_count + 1);
            humi_avg = (humi_avg * sample_count + recv_data.humidity) / (sample_count + 1);
            sample_count++;

            // 释放互斥锁
            xSemaphoreGive(data_mutex);

            ESP_LOGD(TAG, "处理数据：ID=%d，累计采样%d次，平均温度=%.1f℃，平均湿度=%.1f%%",
                     recv_data.sample_id, sample_count, temp_avg, humi_avg);
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // 短暂延时，释放CPU
    }
    vTaskDelete(NULL);
}

// ====================== 任务3：控制台打印+任务控制 ======================
void console_print_task(void *arg) {
    int suspend_flag = 0; // 模拟按键：0=正常，1=挂起采集任务
    while (1) {
        // 模拟“按键触发”：运行10秒后挂起采集任务，5秒后恢复
        if (sample_count > 20 && suspend_flag == 0) {
            ESP_LOGI(TAG, "模拟按键触发：挂起采集任务！");
            vTaskSuspend(collect_task_handle); // 挂起采集任务
            suspend_flag = 1;
        } else if (suspend_flag == 1 && sample_count > 30) {
            ESP_LOGI(TAG, "模拟按键触发：恢复采集任务！");
            vTaskResume(collect_task_handle);  // 恢复采集任务
            suspend_flag = 2; // 只触发一次
        }

        // 加互斥锁，读取共享的平均值
        xSemaphoreTake(data_mutex, portMAX_DELAY);
        ESP_LOGI(TAG, "===== 数据汇总 =====");
        ESP_LOGI(TAG, "累计采样：%d次", sample_count);
        ESP_LOGI(TAG, "平均温度：%.1f℃", temp_avg);
        ESP_LOGI(TAG, "平均湿度：%.1f%%", humi_avg);
        ESP_LOGI(TAG, "====================");
        xSemaphoreGive(data_mutex);

        vTaskDelay(pdMS_TO_TICKS(1000)); // 1秒打印一次
    }
    vTaskDelete(NULL);
}

// ====================== 主函数：创建任务/队列/互斥锁 ======================
void test_multi_task(void) {
    // 1. 创建队列：长度10，每个元素为sensor_data_t大小
    sensor_queue = xQueueCreate(10, sizeof(sensor_data_t));
    if (sensor_queue == NULL) {
        ESP_LOGE(TAG, "队列创建失败，程序退出！");
        return;
    }

    // 2. 创建互斥锁
    data_mutex = xSemaphoreCreateMutex();
    if (data_mutex == NULL) {
        ESP_LOGE(TAG, "互斥锁创建失败，程序退出！");
        return;
    }

    // 3. 创建采集任务（绑定CPU1，栈4096，优先级1）
    BaseType_t ret = xTaskCreatePinnedToCore(
        sensor_collect_task,  // 任务函数
        "CollectTask",        // 任务名称（仅调试）
        4096,                 // 栈大小（字节）
        NULL,                 // 任务入参
        1,                    // 优先级（低）
        &collect_task_handle, // 任务句柄（用于挂起/恢复）
        1                     // 绑定到CPU1（耗时任务优先CPU1）
    );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "采集任务创建失败！");
        return;
    }

    // 4. 创建处理任务（绑定CPU1，栈4096，优先级2）
    ret = xTaskCreatePinnedToCore(
        data_process_task,
        "ProcessTask",
        4096,
        NULL,
        2,                    // 优先级（中）
        NULL,
        1                     // 绑定到CPU1
    );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "处理任务创建失败！");
        return;
    }

    // 5. 创建打印任务（绑定CPU0，栈4096，优先级3）
    ret = xTaskCreatePinnedToCore(
        console_print_task,
        "PrintTask",
        4096,
        NULL,
        3,                    // 优先级（高）
        NULL,
        0                     // 绑定到CPU0（控制台/系统任务优先CPU0）
    );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "打印任务创建失败！");
        return;
    }

    ESP_LOGI(TAG, "所有任务创建完成，程序启动！");
    // app_main是临时任务，退出后不影响其他任务运行
}