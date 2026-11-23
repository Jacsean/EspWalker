/**
 * @file proximity.c
 * @brief 超声测距实现：Trig 触发 10µs，Echo 高电平时间 → 距离。
 *        距离(cm) = 高电平(µs) × 0.017 ≈ µs × 17 / 1000。
 */
#include "proximity.h"

#include <stdlib.h>
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "esp_log.h"

static const char *TAG = "proximity";

typedef struct {
    int trig_gpio;
    int echo_gpio;
    int stop_cm;
    int slow_cm;
    int max_cm;
} proximity_t;

esp_err_t proximity_init(const proximity_config_t *cfg, proximity_handle_t *out)
{
    if (cfg == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    proximity_t *p = calloc(1, sizeof(proximity_t));
    if (p == NULL) {
        return ESP_ERR_NO_MEM;
    }
    p->trig_gpio = cfg->trig_gpio;
    p->echo_gpio = cfg->echo_gpio;
    p->stop_cm = cfg->stop_cm;
    p->slow_cm = cfg->slow_cm;
    p->max_cm = cfg->max_cm;

    gpio_config_t trig = {
        .pin_bit_mask = 1ULL << cfg->trig_gpio,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&trig);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "trig gpio_config failed: %s", esp_err_to_name(err));
        free(p);
        return err;
    }
    gpio_set_level(cfg->trig_gpio, 0);

    gpio_config_t echo = {
        .pin_bit_mask = 1ULL << cfg->echo_gpio,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    err = gpio_config(&echo);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "echo gpio_config failed: %s", esp_err_to_name(err));
        free(p);
        return err;
    }

    *out = p;
    ESP_LOGI(TAG, "proximity ready: trig=%d echo=%d stop=%dcm slow=%dcm",
             cfg->trig_gpio, cfg->echo_gpio, cfg->stop_cm, cfg->slow_cm);
    return ESP_OK;
}

static int ultrasonic_measure(proximity_t *p)
{
    /* 触发脉冲 */
    gpio_set_level(p->trig_gpio, 0);
    esp_rom_delay_us(2);
    gpio_set_level(p->trig_gpio, 1);
    esp_rom_delay_us(10);
    gpio_set_level(p->trig_gpio, 0);

    /* 等待 Echo 上升沿（超时 30 ms） */
    int64_t deadline = esp_timer_get_time() + 30000;
    while (gpio_get_level(p->echo_gpio) == 0) {
        if (esp_timer_get_time() > deadline) {
            return -1;
        }
    }
    int64_t t0 = esp_timer_get_time();

    /* 等待 Echo 下降沿（超时 30 ms） */
    deadline = t0 + 30000;
    while (gpio_get_level(p->echo_gpio) == 1) {
        if (esp_timer_get_time() > deadline) {
            return -1;
        }
    }
    int64_t t1 = esp_timer_get_time();

    int64_t us = t1 - t0;
    int cm = (int)(us * 17 / 1000);
    if (cm > p->max_cm) {
        cm = p->max_cm;
    }
    return cm;
}

int proximity_read_cm(proximity_handle_t h)
{
    if (h == NULL) {
        return -1;
    }
    return ultrasonic_measure((proximity_t *)h);
}

prox_level_t proximity_level(proximity_handle_t h, int dist_cm)
{
    if (h == NULL) {
        return PROX_OK;
    }
    proximity_t *p = (proximity_t *)h;
    if (dist_cm < 0) {
        return PROX_OK; /* 读不到（多为超出量程=远），按安全处理 */
    }
    if (dist_cm <= p->stop_cm) {
        return PROX_STOP;
    }
    if (dist_cm <= p->slow_cm) {
        return PROX_WARN;
    }
    return PROX_OK;
}
