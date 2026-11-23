/**
 * @file proximity.c
 * @brief 超声（HC-SR04）与 TOF（VL53L0X）测距实现。
 */
#include "proximity.h"
#include "imu.h"

#include <stdlib.h>
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "proximity";

typedef struct {
    prox_sensor_t sensor;
    int trig_gpio;
    int echo_gpio;
    i2c_master_dev_handle_t tof_dev;
    int stop_cm;
    int slow_cm;
    int max_cm;
} proximity_t;

/* ---------- 超声 ---------- */
static int ultrasonic_measure(proximity_t *p)
{
    gpio_set_level(p->trig_gpio, 0);
    esp_rom_delay_us(2);
    gpio_set_level(p->trig_gpio, 1);
    esp_rom_delay_us(10);
    gpio_set_level(p->trig_gpio, 0);

    int64_t deadline = esp_timer_get_time() + 30000;
    while (gpio_get_level(p->echo_gpio) == 0) {
        if (esp_timer_get_time() > deadline) {
            return -1;
        }
    }
    int64_t t0 = esp_timer_get_time();
    deadline = t0 + 30000;
    while (gpio_get_level(p->echo_gpio) == 1) {
        if (esp_timer_get_time() > deadline) {
            return -1;
        }
    }
    int64_t t1 = esp_timer_get_time();
    int cm = (int)((t1 - t0) * 17 / 1000);
    if (cm > p->max_cm) {
        cm = p->max_cm;
    }
    return cm;
}

/* ---------- TOF（VL53L0X，I2C） ---------- */
static esp_err_t tof_write(proximity_t *p, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(p->tof_dev, buf, sizeof(buf), 100);
}

static esp_err_t tof_read(proximity_t *p, uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(p->tof_dev, &reg, 1, data, len, 100);
}

static esp_err_t vl53l0x_init(proximity_t *p, int sda, int scl, uint8_t addr)
{
    /* 复用 IMU 的 I2C 总线（同引脚 47/48）；无则自建 */
    i2c_master_bus_handle_t bus = imu_get_bus();
    if (bus == NULL) {
        i2c_master_bus_config_t bcfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = sda,
            .scl_io_num = scl,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .flags.enable_internal_pullup = true,
        };
        esp_err_t err = i2c_new_master_bus(&bcfg, &bus);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "i2c bus failed: %s", esp_err_to_name(err));
            return err;
        }
    }

    i2c_device_config_t dcfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 400000,
    };
    esp_err_t err = i2c_master_bus_add_device(bus, &dcfg, &p->tof_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "add tof device failed: %s", esp_err_to_name(err));
        return err;
    }

    /* 精简初始化序列（进入默认测距模式，量程约 1.2 m） */
    static const uint8_t seq[][2] = {
        {0x88, 0x00}, {0x80, 0x01}, {0xFF, 0x01}, {0x00, 0x00},
        {0x91, 0x3C}, {0x00, 0x01}, {0xFF, 0x00}, {0x80, 0x00},
    };
    for (size_t i = 0; i < sizeof(seq) / sizeof(seq[0]); i++) {
        err = tof_write(p, seq[i][0], seq[i][1]);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "tof init seq failed");
            return err;
        }
    }
    return ESP_OK;
}

static int vl53l0x_read_mm(proximity_t *p)
{
    /* 触发单次测距 */
    if (tof_write(p, 0x00, 0x01) != ESP_OK) {
        return -1;
    }
    /* 轮询完成位（0x13 bit0） */
    uint8_t st = 0;
    for (int i = 0; i < 50; i++) {
        tof_read(p, 0x13, &st, 1);
        if (st & 0x01) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    /* 读 12 字节结果，距离 mm 在 buf[10..11] */
    uint8_t buf[12] = {0};
    tof_read(p, 0x14, buf, sizeof(buf));
    uint16_t mm = ((uint16_t)buf[10] << 8) | buf[11];
    tof_write(p, 0x0B, 0x01); /* 清中断 */
    return (int)mm;
}

/* ---------- 对外接口 ---------- */
esp_err_t proximity_init(const proximity_config_t *cfg, proximity_handle_t *out)
{
    if (cfg == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    proximity_t *p = calloc(1, sizeof(proximity_t));
    if (p == NULL) {
        return ESP_ERR_NO_MEM;
    }
    p->sensor = cfg->sensor;
    p->trig_gpio = cfg->trig_gpio;
    p->echo_gpio = cfg->echo_gpio;
    p->stop_cm = cfg->stop_cm;
    p->slow_cm = cfg->slow_cm;
    p->max_cm = cfg->max_cm;

    if (cfg->sensor == PROX_SENSOR_TOF) {
        esp_err_t err = vl53l0x_init(p, cfg->i2c_sda, cfg->i2c_scl, cfg->i2c_addr);
        if (err != ESP_OK) {
            free(p);
            return err;
        }
    } else {
        gpio_config_t trig = {
            .pin_bit_mask = 1ULL << cfg->trig_gpio,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        esp_err_t err = gpio_config(&trig);
        if (err != ESP_OK) {
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
            free(p);
            return err;
        }
    }

    *out = p;
    ESP_LOGI(TAG, "proximity ready: sensor=%d stop=%d slow=%d",
             cfg->sensor, cfg->stop_cm, cfg->slow_cm);
    return ESP_OK;
}

int proximity_read_cm(proximity_handle_t h)
{
    if (h == NULL) {
        return -1;
    }
    proximity_t *p = (proximity_t *)h;

    if (p->sensor == PROX_SENSOR_TOF) {
        int mm = vl53l0x_read_mm(p);
        if (mm <= 0) {
            return -1;
        }
        int cm = mm / 10;
        if (cm > p->max_cm) {
            cm = p->max_cm;
        }
        return cm;
    }
    return ultrasonic_measure(p);
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
