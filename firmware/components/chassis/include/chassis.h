/**
 * @file chassis.h
 * @brief 运动底盘抽象：把 (vx, vy, ω) 归一化速度映射到各轮转速，
 *        支持两轮差速 / 履带 / 麦克纳姆四轮三种底座。
 */
#pragma once

#include "esp_err.h"
#include "motor.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CHASSIS_DIFF2 = 0, /**< 两轮差速 + 尾轮 */
    CHASSIS_TRACK = 1, /**< 履带（与差速同运动学） */
    CHASSIS_MECANUM = 2, /**< 麦克纳姆四轮（全向） */
} chassis_type_t;

typedef struct {
    chassis_type_t type;
    int num_motors;           /**< 2 或 4 */
    motor_config_t motors[4]; /**< 顺序：差速=[左,右]；麦轮=[前左,前右,后左,后右] */
    float geometry[2];        /**< [0]=轮距/纵向半轴, [1]=横向半轴（麦轮用），米 */
} chassis_config_t;

typedef void *chassis_handle_t;

esp_err_t chassis_init(const chassis_config_t *cfg, chassis_handle_t *out);

/**
 * @brief 下发运动指令。vx/vy/ω ∈ [-1,1]（归一化），
 *        vx 前后、vy 左右（仅麦轮有效）、ω 逆时针旋转为正。
 */
esp_err_t chassis_move(chassis_handle_t h, float vx, float vy, float omega);

/** @brief 立即制动停车。 */
esp_err_t chassis_stop(chassis_handle_t h);

#ifdef __cplusplus
}
#endif
