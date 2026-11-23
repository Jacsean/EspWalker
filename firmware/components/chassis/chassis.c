/**
 * @file chassis.c
 * @brief 底盘运动学实现。
 *
 * 归一化速度模式下，各轮期望速度 = 归一化量组合，随后整体等比缩放
 * 使最大 |轮速| ≤ 1，再 ×1000 交给 motor_set_speed。
 */
#include "chassis.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"

static const char *TAG = "chassis";

#define SPEED_SCALE 1000

typedef struct {
    chassis_type_t type;
    int num_motors;
    motor_handle_t motors[4];
} chassis_t;

static float clamp1(float v)
{
    if (v > 1.0f) {
        return 1.0f;
    }
    if (v < -1.0f) {
        return -1.0f;
    }
    return v;
}

esp_err_t chassis_init(const chassis_config_t *cfg, chassis_handle_t *out)
{
    if (cfg == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (cfg->num_motors != 2 && cfg->num_motors != 4) {
        return ESP_ERR_INVALID_ARG;
    }

    chassis_t *c = calloc(1, sizeof(chassis_t));
    if (c == NULL) {
        return ESP_ERR_NO_MEM;
    }
    c->type = cfg->type;
    c->num_motors = cfg->num_motors;

    for (int i = 0; i < cfg->num_motors; i++) {
        esp_err_t err = motor_init(&cfg->motors[i], &c->motors[i]);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "motor_init[%d] failed: %s", i, esp_err_to_name(err));
            free(c);
            return err;
        }
    }

    *out = c;
    ESP_LOGI(TAG, "chassis ready: type=%d motors=%d", cfg->type, cfg->num_motors);
    return ESP_OK;
}

esp_err_t chassis_move(chassis_handle_t h, float vx, float vy, float omega)
{
    if (h == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    chassis_t *c = (chassis_t *)h;

    vx = clamp1(vx);
    vy = clamp1(vy);
    omega = clamp1(omega);

    float w[4] = {0};
    if (c->type == CHASSIS_MECANUM && c->num_motors == 4) {
        /* 麦克纳姆轮运动学（归一化几何系数=1，可标定） */
        w[0] = vx - vy - omega; /* 前左 */
        w[1] = vx + vy + omega; /* 前右 */
        w[2] = vx + vy - omega; /* 后左 */
        w[3] = vx - vy + omega; /* 后右 */
    } else {
        /* 两轮差速 / 履带：vL = vx - ω，vR = vx + ω */
        w[0] = vx - omega;
        w[1] = vx + omega;
    }

    /* 整体等比缩放，保证任一 |轮速| ≤ 1 */
    float peak = 1.0f;
    for (int i = 0; i < c->num_motors; i++) {
        float a = fabsf(w[i]);
        if (a > peak) {
            peak = a;
        }
    }
    if (peak > 1.0f) {
        for (int i = 0; i < c->num_motors; i++) {
            w[i] /= peak;
        }
    }

    for (int i = 0; i < c->num_motors; i++) {
        int speed = (int)(w[i] * SPEED_SCALE);
        esp_err_t err = motor_set_speed(c->motors[i], speed);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "motor_set_speed[%d] failed: %s", i, esp_err_to_name(err));
            return err;
        }
    }
    return ESP_OK;
}

esp_err_t chassis_stop(chassis_handle_t h)
{
    if (h == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    chassis_t *c = (chassis_t *)h;
    for (int i = 0; i < c->num_motors; i++) {
        esp_err_t err = motor_brake(c->motors[i]);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "motor_brake[%d] failed: %s", i, esp_err_to_name(err));
            return err;
        }
    }
    return ESP_OK;
}
