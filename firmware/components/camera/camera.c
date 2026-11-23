/**
 * @file camera.c
 * @brief 摄像头实现（需 espressif/esp32-camera）。
 *
 * 兼容 OV2640（200 万）与 OV3660（300 万）：两者均为 DVP 接口、引脚一致，
 * esp32-camera 启动时经 I2C 自动探测 sensor id 并加载对应驱动；需在 menuconfig
 * 的 esp32-camera 组件里启用 OV2640_SUPPORT / OV3660_SUPPORT。
 */
#include "camera.h"

#include "esp_camera.h"
#include "esp_log.h"

static const char *TAG = "camera";

static camera_fb_t *s_fb = NULL;

/* ESP32-S3 DVP 推荐引脚（OV2640 / OV3660 通用，与 esp32-camera 官方示例一致） */
static const camera_config_t s_cam_cfg = {
    .pin_pwdn = -1,
    .pin_reset = -1,
    .pin_xclk = 15,
    .pin_sccb_sda = 8,
    .pin_sccb_scl = 9,
    .pin_d7 = 39,
    .pin_d6 = 40,
    .pin_d5 = 41,
    .pin_d4 = 42,
    .pin_d3 = 43,
    .pin_d2 = 44,
    .pin_d1 = 45,
    .pin_d0 = 46,
    .pin_vsync = 6,
    .pin_href = 7,
    .pin_pclk = 13,
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_1,
    .ledc_channel = LEDC_CHANNEL_1,
    .pixel_format = PIXFORMAT_JPEG,
#if CONFIG_APP_CAMERA_SENSOR_OV3660
    .frame_size = FRAMESIZE_UXGA, /* 1600x1200，OV3660 原生支持 */
#else
    .frame_size = FRAMESIZE_XGA, /* 1024x768，OV2640 / auto 均稳 */
#endif
    .jpeg_quality = 12,
    .fb_count = 2,
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_LATEST,
};

esp_err_t camera_init(void)
{
    esp_err_t err = esp_camera_init(&s_cam_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_camera_init failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "camera ready");
    return ESP_OK;
}

esp_err_t camera_capture_jpeg(const uint8_t **buf, size_t *len)
{
    if (buf == NULL || len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_fb != NULL) {
        esp_camera_fb_return(s_fb);
        s_fb = NULL;
    }
    s_fb = esp_camera_fb_get();
    if (s_fb == NULL) {
        ESP_LOGE(TAG, "frame buffer get failed");
        return ESP_FAIL;
    }
    *buf = s_fb->buf;
    *len = s_fb->len;
    return ESP_OK;
}

esp_err_t camera_deinit(void)
{
    if (s_fb != NULL) {
        esp_camera_fb_return(s_fb);
        s_fb = NULL;
    }
    return esp_camera_deinit();
}
