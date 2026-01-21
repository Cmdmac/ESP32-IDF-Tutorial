#include <stdio.h>
#include "esp_partition.h"
#include "esp_log.h"

const char* TAG = "main";
void iterate_all_app_partitions(void) {
    // 创建迭代器：匹配所有APP类型的分区（子类型任意）
    esp_partition_iterator_t iter = esp_partition_find(
        ESP_PARTITION_TYPE_APP,
        ESP_PARTITION_SUBTYPE_ANY,
        NULL // 不限制名称
    );

    if (iter == NULL) {
        ESP_LOGE(TAG, "未找到APP分区");
        return;
    }

    // 遍历所有匹配的分区
    int count = 0;
    while (iter) {
        const esp_partition_t *part = esp_partition_get(iter);
        ESP_LOGI(TAG, "APP分区%d：名称=%s，地址=0x%08X，大小=%dKB",
                 count++, part->label, part->address, part->size/1024);
        // 迭代下一个分区
        iter = esp_partition_next(iter);
    }

    // 必须释放迭代器！
    esp_partition_iterator_release(iter);
}

void app_main(void)
{
    iterate_all_app_partitions();
}
