/**
 * @file camera.h
 * @brief 摄像头拍照（OV2640 / esp32-camera）。默认关闭，启用见组件 CMakeLists。
 */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 初始化摄像头（帧缓冲走 PSRAM）。 */
esp_err_t camera_init(void);

/**
 * @brief 抓一帧 JPEG。返回的缓冲在下次 capture 前有效，用完后无需手动释放。
 * @param buf  [out] JPEG 数据指针
 * @param len  [out] 数据长度
 */
esp_err_t camera_capture_jpeg(const uint8_t **buf, size_t *len);

/** @brief 释放摄像头资源。 */
esp_err_t camera_deinit(void);

#ifdef __cplusplus
}
#endif
