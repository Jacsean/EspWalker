/**
 * @file camera_stub.c
 * @brief 摄像头未启用时的空实现（避免引入 esp32-camera 依赖，离线可构建）。
 */
#include "camera.h"

#include "esp_log.h"

static const char *TAG = "camera";

esp_err_t camera_init(void)
{
    ESP_LOGW(TAG, "camera disabled (set CONFIG_APP_ENABLE_CAMERA=y to enable)");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t camera_capture_jpeg(const uint8_t **buf, size_t *len)
{
    (void)buf;
    (void)len;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t camera_deinit(void)
{
    return ESP_OK;
}
