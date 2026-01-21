#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

// 宏定义：LED 连接的 GPIO 引脚（可修改为任意普通 GPIO，如 19、20（释放 JTAG 后）、2、4 等）
#define LED_GPIO_PIN  2
static const char *TAG = "GPIO";
// 宏定义：按键连接的 GPIO 引脚（可修改为任意普通 GPIO）
#define KEY_GPIO_PIN  4

// 宏定义：按键连接的 GPIO 引脚
#define KEY_INTERUPT_GPIO_PIN  4
// 宏定义：中断优先级（0~3，数值越小优先级越高，避免与系统高优先级中断冲突）
#define KEY_INTR_PRIO  1

/**
 * @brief  GPIO 输出模式初始化配置
 */
void gpio_output_init(void)
{
    // 1. 定义 GPIO 配置结构体
    gpio_config_t io_conf = {0};

    // 2. 配置参数设置
    io_conf.intr_type = GPIO_INTR_DISABLE;  // 禁用中断（输出模式无需中断）
    io_conf.mode = GPIO_MODE_OUTPUT;        // 配置为输出模式
    io_conf.pin_bit_mask = (1ULL << LED_GPIO_PIN);  // 选中要配置的 GPIO 引脚
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;  // 禁用下拉电阻
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;      // 禁用上拉电阻（推挽输出无需上下拉）

    // 3. 应用 GPIO 配置（生效）
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO 输出模式配置失败，错误码：%d", ret);
        return;
    }

    // 4. 初始化电平：设置为低电平（LED 熄灭，根据硬件接线调整）
    gpio_set_level(LED_GPIO_PIN, 0);
    ESP_LOGI(TAG, "GPIO 输出模式初始化完成，引脚：%d", LED_GPIO_PIN);
}

/**
 * @brief  GPIO 输入模式初始化配置
 */
void gpio_input_init(void)
{
    // 1. 定义 GPIO 配置结构体
    gpio_config_t io_conf = {0};

    // 2. 配置参数设置
    io_conf.intr_type = GPIO_INTR_DISABLE;  // 禁用中断（轮询模式无需中断）
    io_conf.mode = GPIO_MODE_INPUT;         // 配置为输入模式
    io_conf.pin_bit_mask = (1ULL << KEY_GPIO_PIN);  // 选中要配置的 GPIO 引脚
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;  // 禁用下拉电阻
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;       // 启用上拉电阻（按键另一端接 GND）

    // 3. 应用 GPIO 配置（生效）
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO 输入模式配置失败，错误码：%d", ret);
        return;
    }

    ESP_LOGI(TAG, "GPIO 输入模式初始化完成，引脚：%d", KEY_GPIO_PIN);
}

/**
 * @brief  按键状态检测任务（FreeRTOS 任务，轮询方式）
 */
void key_detect_task(void *arg)
{
    uint32_t key_level = 0;  // 存储按键电平状态（0：按下，1：松开，因上拉电阻+按键接GND）
    uint32_t last_key_level = 1;  // 存储上一次按键电平状态，初始化为松开状态

    while (1) {
        // 1. 读取 GPIO 输入电平
        key_level = gpio_get_level(KEY_GPIO_PIN);

        // 2. 检测按键状态变化（消抖：仅当电平变化时打印日志，避免重复输出）
        if (key_level != last_key_level) {
            if (key_level == 0) {
                ESP_LOGI(TAG, "按键按下（电平：%d）", key_level);
            } else {
                ESP_LOGI(TAG, "按键松开（电平：%d）", key_level);
            }
            last_key_level = key_level;  // 更新上一次按键电平状态
        }

        // 3. 轮询间隔：20ms（平衡响应速度和 CPU 占用，简易消抖）
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/**
 * @brief  LED 闪烁
 */
void led_blink_task()
{
    while (1) {
        // 1. 设置 GPIO 为高电平（LED 点亮）
        gpio_set_level(LED_GPIO_PIN, 1);
        ESP_LOGI(TAG, "LED 点亮");
        vTaskDelay(pdMS_TO_TICKS(1000));  // 延时 1 秒

        // 2. 设置 GPIO 为低电平（LED 熄灭）
        gpio_set_level(LED_GPIO_PIN, 0);
        ESP_LOGI(TAG, "LED 熄灭");
        vTaskDelay(pdMS_TO_TICKS(1000));  // 延时 1 秒
    }
}


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

    // 6. 创建二值信号量（用于中断与任务同步）
    key_semaphore = xSemaphoreCreateBinary();
    if (key_semaphore == NULL) {
        ESP_LOGE(TAG, "信号量创建失败");
        return;
    }

    ESP_LOGI(TAG, "GPIO 中断模式初始化完成，引脚：%d（下降沿触发）", KEY_INTERUPT_GPIO_PIN);
}

/**
 * @brief  按键中断处理任务（在任务中处理具体逻辑，避免中断上下文耗时操作）
 */
void key_interrupt_task(void *arg)
{
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
}

void app_main(void)
{
    // 初始化 GPIO 输出模式
    gpio_output_init();

    led_blink_task();
}