/**
 * @file proximity.h
 * @brief 障碍物感知：超声测距（HC-SR04）+ 距离分级。
 *        如需 VL53L0X TOF，可在此接口后扩展 I2C 实现。
 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PROX_OK = 0,   /**< 安全区 */
    PROX_WARN = 1, /**< 减速区 */
    PROX_STOP = 2, /**< 停车区 */
} prox_level_t;

typedef struct {
    int trig_gpio; /**< 超声 Trig（-1 表示未接超声） */
    int echo_gpio; /**< 超声 Echo */
    int stop_cm;   /**< 停车阈值（cm） */
    int slow_cm;   /**< 减速阈值（cm） */
    int max_cm;    /**< 有效量程上限（cm） */
} proximity_config_t;

typedef void *proximity_handle_t;

esp_err_t proximity_init(const proximity_config_t *cfg, proximity_handle_t *out);

/** @brief 读一次距离（cm）；读不到返回 -1（超出量程/超时）。 */
int proximity_read_cm(proximity_handle_t h);

/** @brief 由距离判定分级。 */
prox_level_t proximity_level(proximity_handle_t h, int dist_cm);

#ifdef __cplusplus
}
#endif
