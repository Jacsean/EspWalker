/**
 * @file proximity.h
 * @brief 障碍物感知：超声（HC-SR04）/ TOF（VL53L0X）测距 + 距离分级。
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PROX_OK = 0,   /**< 安全区 */
    PROX_WARN = 1, /**< 减速区 */
    PROX_STOP = 2, /**< 停车区 */
} prox_level_t;

typedef enum {
    PROX_SENSOR_ULTRASONIC = 0, /**< HC-SR04 超声（GPIO） */
    PROX_SENSOR_TOF = 1,        /**< VL53L0X TOF（I2C） */
} prox_sensor_t;

typedef struct {
    prox_sensor_t sensor; /**< 传感器类型 */
    int trig_gpio;        /**< 超声 Trig（超声用） */
    int echo_gpio;        /**< 超声 Echo（超声用） */
    int i2c_sda;          /**< TOF I2C SDA（TOF 用） */
    int i2c_scl;          /**< TOF I2C SCL（TOF 用） */
    uint8_t i2c_addr;     /**< TOF I2C 地址（0x29） */
    int stop_cm;          /**< 停车阈值（cm） */
    int slow_cm;          /**< 减速阈值（cm） */
    int max_cm;           /**< 有效量程上限（cm） */
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
