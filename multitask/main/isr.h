#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

// 宏定义：按键连接的 GPIO 引脚
#define KEY_INTERUPT_GPIO_PIN  4
// 宏定义：中断优先级（0~3，数值越小优先级越高，避免与系统高优先级中断冲突）
#define KEY_INTR_PRIO  1

// 全局信号量（用于中断与任务间的同步，避免在中断上下文执行耗时操作）
SemaphoreHandle_t key_semaphore = NULL;

/**
 * @brief  GPIO 中断回调函数（中断上下文，需简洁高效，禁止耗时操作）
 * @note   1. 不能使用 ESP_LOGI/ESP_LOGE 等日志函数（可能导致死锁）
 *         2. 不能使用 vTaskDelay 等 FreeRTOS 延时函数
 *         3. 优先使用信号量/队列与任务同步
 */
static void gpio_isr_handler(void *arg)
{
    // 给信号量发通知，唤醒任务处理具体逻辑
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (key_semaphore != NULL) {
        xSemaphoreGiveFromISR(key_semaphore, &xHigherPriorityTaskWoken);
        // 若唤醒了更高优先级的任务，触发任务切换
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/**
 * @brief  GPIO 中断模式初始化配置
 */
void gpio_interrupt_init(void)
{
    // 1. 定义 GPIO 配置结构体
    gpio_config_t io_conf = {0};

    // 2. 配置参数设置
    io_conf.intr_type = GPIO_INTR_NEGEDGE;  // 配置为下降沿触发中断（按键按下时，电平从 1 变 0）
    io_conf.mode = GPIO_MODE_INPUT;         // 配置为输入模式
    io_conf.pin_bit_mask = (1ULL << KEY_INTERUPT_GPIO_PIN);  // 选中要配置的 GPIO 引脚
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;  // 禁用下拉电阻
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;       // 启用上拉电阻（按键另一端接 GND）

    // 3. 应用 GPIO 配置（生效）
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO 中断模式配置失败，错误码：%d", ret);
        return;
    }

    // 4. 安装 GPIO 中断服务（默认配置，共享中断）
    gpio_install_isr_service(0);

    // 5. 为指定 GPIO 注册中断回调函数
    ret = gpio_isr_handler_add(KEY_INTERUPT_GPIO_PIN, gpio_isr_handler, (void *)KEY_INTERUPT_GPIO_PIN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO 中断回调函数注册失败，错误码：%d", ret);
        return;
    }

    ESP_LOGI(TAG, "GPIO 中断模式初始化完成，引脚：%d（下降沿触发）", KEY_INTERUPT_GPIO_PIN);
}

/**
 * @brief  按键中断处理任务（在任务中处理具体逻辑，避免中断上下文耗时操作）
 */
void key_interrupt_task(void *arg)
{   
    ESP_LOGI(TAG, "arg=%s", (char*)arg);
    while (1) {
        // 1. 等待信号量（无限等待，直到中断触发）
        if (xSemaphoreTake(key_semaphore, portMAX_DELAY) == pdTRUE) {
            // 2. 处理按键逻辑（此处可添加消抖、功能执行等操作）
            ESP_LOGI(TAG, "检测到按键中断，执行按键处理逻辑");
            
            // 3. 简易消抖（延时 50ms，避免电平抖动导致重复触发）
            vTaskDelay(pdMS_TO_TICKS(50));
            
            // 4. 验证按键当前状态（可选，进一步确认按键按下）
            if (gpio_get_level(KEY_INTERUPT_GPIO_PIN) == 0) {
                ESP_LOGI(TAG, "按键确认按下，执行具体功能");
            }
        }
    }
    vTaskDelelte(NULL);
}

void test_isr_task(void)
{
    // 初始化 GPIO 输出模式
    gpio_interrupt_init();

    key_semaphore = xSemaphoreCreateBinary();
    if (key_semaphore == NULL) {
        ESP_LOGE(TAG, "信号量创建失败");
        return;
    }
    
    TaskHandle_t isr_task_handle;// 采集任务句柄（用于挂起/恢复）

    char* args = "isr_task";
    BaseType_t ret = xTaskCreatePinnedToCore(
        key_interrupt_task,  // 任务函数
        "ISRTask",        // 任务名称（仅调试）
        4096,                 // 栈大小（字节）
        args,                 // 任务入参
        1,                    // 优先级（低）
        &isr_task_handle, // 任务句柄（用于挂起/恢复）
        1                     // 绑定到CPU1（耗时任务优先CPU1）
    );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "采集任务创建失败！");
        return;
    }

}