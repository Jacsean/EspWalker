/**
 * @file motor.c
 * @brief 电机驱动实现（ESP-IDF v6.1：ledc_timer_config / ledc_channel_config）。
 */
#include "motor.h"

#include <stdlib.h>
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_check.h"

static const char *TAG = "motor";

#define MOTOR_MAX_SPEED 1000
#define MOTOR_PWM_FREQ_HZ 20000

typedef struct {
    int in1_gpio;
    int in2_gpio;
    ledc_channel_t channel;
} motor_t;

static bool s_timer_inited = false;

esp_err_t motor_init(const motor_config_t *cfg, motor_handle_t *out)
{
    if (cfg == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* LEDC 定时器只需配置一次（4 路电机共用 LEDC_TIMER_0） */
    if (!s_timer_inited) {
        ledc_timer_config_t tcfg = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .duty_resolution = LEDC_TIMER_10_BIT,
            .timer_num = LEDC_TIMER_0,
            .freq_hz = MOTOR_PWM_FREQ_HZ,
            .clk_cfg = LEDC_AUTO_CLK,
        };
        esp_err_t err = ledc_timer_config(&tcfg);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "ledc_timer_config failed: %s", esp_err_to_name(err));
            return err;
        }
        s_timer_inited = true;
    }

    motor_t *m = calloc(1, sizeof(motor_t));
    if (m == NULL) {
        return ESP_ERR_NO_MEM;
    }
    m->in1_gpio = cfg->in1_gpio;
    m->in2_gpio = cfg->in2_gpio;
    m->channel = (ledc_channel_t)cfg->ledc_channel;

    gpio_config_t io = {
        .pin_bit_mask = (1ULL << cfg->in1_gpio) | (1ULL << cfg->in2_gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&io);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config failed: %s", esp_err_to_name(err));
        free(m);
        return err;
    }

    ledc_channel_config_t ccfg = {
        .gpio_num = cfg->pwm_gpio,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = m->channel,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
    };
    err = ledc_channel_config(&ccfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_channel_config failed: %s", esp_err_to_name(err));
        free(m);
        return err;
    }

    /* 初始：方向脚全低（滑行），占空比 0 */
    gpio_set_level(cfg->in1_gpio, 0);
    gpio_set_level(cfg->in2_gpio, 0);

    *out = m;
    return ESP_OK;
}

esp_err_t motor_set_speed(motor_handle_t h, int speed)
{
    if (h == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    motor_t *m = (motor_t *)h;

    if (speed > MOTOR_MAX_SPEED) {
        speed = MOTOR_MAX_SPEED;
    } else if (speed < -MOTOR_MAX_SPEED) {
        speed = -MOTOR_MAX_SPEED;
    }

    uint32_t duty_raw;
    if (speed > 0) {
        gpio_set_level(m->in1_gpio, 1);
        gpio_set_level(m->in2_gpio, 0);
        duty_raw = (uint32_t)speed;
    } else if (speed < 0) {
        gpio_set_level(m->in1_gpio, 0);
        gpio_set_level(m->in2_gpio, 1);
        duty_raw = (uint32_t)(-speed);
    } else {
        gpio_set_level(m->in1_gpio, 0);
        gpio_set_level(m->in2_gpio, 0);
        duty_raw = 0;
    }

    /* 10 bit 分辨率：0~1000 映射到 0~1023 */
    uint32_t pwm = (duty_raw * 1023U) / MOTOR_MAX_SPEED;
    esp_err_t err = ledc_set_duty(LEDC_LOW_SPEED_MODE, m->channel, pwm);
    if (err != ESP_OK) {
        return err;
    }
    return ledc_update_duty(LEDC_LOW_SPEED_MODE, m->channel);
}

esp_err_t motor_brake(motor_handle_t h)
{
    if (h == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    motor_t *m = (motor_t *)h;

    /* TB6612：IN1=IN2=1 为短接制动 */
    gpio_set_level(m->in1_gpio, 1);
    gpio_set_level(m->in2_gpio, 1);
    esp_err_t err = ledc_set_duty(LEDC_LOW_SPEED_MODE, m->channel, 0);
    if (err != ESP_OK) {
        return err;
    }
    return ledc_update_duty(LEDC_LOW_SPEED_MODE, m->channel);
}
