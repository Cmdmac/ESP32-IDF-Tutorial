#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

// ************************ 宏定义（可根据硬件修改）************************
#define PWM_GPIO_PIN        2       // 连接 LED 的 GPIO 引脚（可替换为 19、20（释放 JTAG 后）等普通 GPIO）
#define LEDC_TIMER          LEDC_TIMER_0  // 选择 LEDC 定时器 0
#define LEDC_CHANNEL        LEDC_CHANNEL_0  // 选择 LEDC 通道 0（与定时器绑定）
#define LEDC_FREQ_HZ        5000    // PWM 频率：5000Hz（避免 LED 闪烁，推荐 1k~10k Hz）
#define LEDC_TIMER_RESOLUTION  LEDC_TIMER_10_BIT  // 定时器分辨率：10 位（对应占空比范围 0~1023）
#define BREATH_DELAY_MS     10      // 呼吸灯渐变步长延时（越小，渐变越快，可修改调节流畅度）
// ***********************************************************************

static const char *TAG = "PWM_LED_BRIGHTNESS";

/**
 * @brief  LEDC PWM 初始化配置（定时器 + 通道，适配 ESP-IDF 5.5.1）
 */
void pwm_ledc_init(void)
{
    // 1. 配置 LEDC 定时器参数（用 0 替代 LEDC_MODE_DEFAULT，对应默认模式）
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_RESOLUTION,  // 定时器分辨率
        .freq_hz = LEDC_FREQ_HZ,                    // PWM 频率
        .speed_mode = LEDC_LOW_SPEED_MODE,                            // 【关键修正】直接使用 0 作为默认模式（IDF 5.5.1 兼容）
        .timer_num = LEDC_TIMER,                    // 绑定定时器
        .clk_cfg = LEDC_AUTO_CLK,                   // 自动选择时钟源
    };
    // 应用定时器配置
    esp_err_t ret = ledc_timer_config(&ledc_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC 定时器配置失败，错误码：%d", ret);
        return;
    }

    // 2. 配置 LEDC 通道参数（模式与定时器保持一致，同样使用 0）
    ledc_channel_config_t ledc_channel = {
        .channel = LEDC_CHANNEL,                    // 选择 LEDC 通道
        .duty = 0,                                  // 初始占空比：0（LED 最暗，熄灭状态）
        .gpio_num = PWM_GPIO_PIN,                   // 绑定 PWM 输出 GPIO 引脚
        .speed_mode = LEDC_LOW_SPEED_MODE,                            // 【关键修正】直接使用 0 作为默认模式，与定时器匹配
        .hpoint = 0,                                // 高电平起始点（默认 0 即可）
        .timer_sel = LEDC_TIMER,                    // 绑定上述配置的定时器
    };
    // 应用通道配置
    ret = ledc_channel_config(&ledc_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC 通道配置失败，错误码：%d", ret);
        return;
    }

    ESP_LOGI(TAG, "LEDC PWM 初始化完成，引脚：%d，频率：%d Hz，分辨率：%d 位",
             PWM_GPIO_PIN, LEDC_FREQ_HZ, LEDC_TIMER_RESOLUTION);
}

/**
 * @brief  呼吸灯任务（循环调节 PWM 占空比，控制 LED 亮度变化）
 */
void pwm_breath_led_task(void *arg)
{
    // 占空比最大值（由 10 位分辨率决定：2^10 - 1 = 1023）
    uint32_t max_duty = (1 << LEDC_TIMER_RESOLUTION) - 1;
    uint32_t current_duty = 0;  // 当前占空比

    while (1) {
        // 阶段 1：占空比从 0 增加到 max_duty（LED 由暗变亮）
        for (current_duty = 0; current_duty <= max_duty; current_duty += 1) {
            // 1. 设置新的占空比（模式参数同样使用 0）
            ledc_set_duty(0, LEDC_CHANNEL, current_duty);
            // 2. 更新占空比（使配置生效）
            ledc_update_duty(0, LEDC_CHANNEL);
            // 3. 延时，控制渐变速度
            vTaskDelay(pdMS_TO_TICKS(BREATH_DELAY_MS));
        }

        // 阶段 2：占空比从 max_duty 减少到 0（LED 由亮变暗）
        for (current_duty = max_duty; current_duty >= 0; current_duty -= 1) {
            // 1. 设置新的占空比（模式参数同样使用 0）
            ledc_set_duty(0, LEDC_CHANNEL, current_duty);
            // 2. 更新占空比（使配置生效）
            ledc_update_duty(0, LEDC_CHANNEL);
            // 3. 延时，控制渐变速度
            vTaskDelay(pdMS_TO_TICKS(BREATH_DELAY_MS));
        }
    }
}

void app_main(void)
{
    // 1. 初始化 LEDC PWM 配置
    pwm_ledc_init();

    // 2. 创建呼吸灯任务（栈大小 4096，优先级 5）
    xTaskCreate(pwm_breath_led_task, "pwm_breath_led_task", 4096, NULL, 5, NULL);
}