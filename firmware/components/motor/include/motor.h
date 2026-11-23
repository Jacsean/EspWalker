/**
 * @file motor.h
 * @brief 单路直流电机驱动（TB6612FNG / DRV8833 等 H 桥，1 PWM + 2 方向）。
 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int pwm_gpio;     /**< 速度 PWM 引脚（接 H 桥 PWMA/PWMB） */
    int in1_gpio;     /**< 方向输入 1（AIN1/BIN1） */
    int in2_gpio;     /**< 方向输入 2（AIN2/BIN2） */
    int ledc_channel; /**< LEDC 通道 0~7，4 路电机取 0~3 */
} motor_config_t;

typedef void *motor_handle_t;

/**
 * @brief 初始化一路电机（PWM 频率 20 kHz、10 bit 分辨率）。
 */
esp_err_t motor_init(const motor_config_t *cfg, motor_handle_t *out);

/**
 * @brief 设置转速，范围 [-1000, 1000]，负值反转。
 */
esp_err_t motor_set_speed(motor_handle_t h, int speed);

/**
 * @brief 短接制动（快速停车）。
 */
esp_err_t motor_brake(motor_handle_t h);

#ifdef __cplusplus
}
#endif
