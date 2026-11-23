/**
 * @file audio_stub.c
 * @brief 语音未启用时的空实现。
 */
#include "audio.h"

#include "esp_log.h"

static const char *TAG = "audio";

esp_err_t audio_init(const audio_config_t *cfg)
{
    (void)cfg;
    ESP_LOGW(TAG, "audio disabled (set CONFIG_APP_ENABLE_AUDIO=y to enable)");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t audio_rec_read(int16_t *buf, int max_samples, size_t *out_bytes)
{
    (void)buf;
    (void)max_samples;
    if (out_bytes != NULL) {
        *out_bytes = 0;
    }
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t audio_play_write(const uint8_t *buf, size_t len)
{
    (void)buf;
    (void)len;
    return ESP_ERR_NOT_SUPPORTED;
}
