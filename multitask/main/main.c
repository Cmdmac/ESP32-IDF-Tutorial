#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"

// ====================== 全局配置与变量 ======================
static const char *TAG = "MultiTask";
#include "isr.h"
#include "multitask.h"

void app_main() {
    // test_isr_task();
    // test_multi_task();
}