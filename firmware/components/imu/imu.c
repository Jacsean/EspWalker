/**
 * @file imu.c
 * @brief MPU6050/MPU9250 6 轴读取 + 互补滤波姿态解算。
 *
 * 寄存器（MPU6050 与 MPU9250 的加速度计/陀螺仪部分兼容）：
 *   PWR_MGMT_1(0x6B)=0x01 唤醒；ACCEL_CONFIG(0x1C)=0x08(±4g)；
 *   GYRO_CONFIG(0x1B)=0x08(±500dps)；数据从 0x3B 起 14 字节（A×6,T×2,G×6）。
 * 灵敏度：±4g → 8192 LSB/g；±500dps → 65.5 LSB/(deg/s)。
 */
#include "imu.h"

#include <math.h>
#include <string.h>
#include "driver/i2c_master.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "imu";

#define MPU_PWR_MGMT_1   0x6B
#define MPU_ACCEL_CONFIG 0x1C
#define MPU_GYRO_CONFIG  0x1B
#define MPU_ACCEL_XOUT_H 0x3B

#define ACCEL_SCALE 8192.0f   /* ±4g */
#define GYRO_SCALE  65.5f     /* ±500 dps */

static i2c_master_bus_handle_t s_bus = NULL;
static i2c_master_dev_handle_t s_dev = NULL;

/* 姿态解算状态（互补滤波） */
static float s_roll = 0.0f, s_pitch = 0.0f, s_yaw = 0.0f;
static int64_t s_last_us = 0;

static esp_err_t imu_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(s_dev, buf, sizeof(buf), 100);
}

static esp_err_t imu_read_regs(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, data, len, 100);
}

esp_err_t imu_init(const imu_config_t *cfg)
{
    if (cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = cfg->i2c_sda,
        .scl_io_num = cfg->i2c_scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_new_master_bus(&bus_cfg, &s_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_new_master_bus failed: %s", esp_err_to_name(err));
        return err;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = cfg->addr,
        .scl_speed_hz = 400000,
    };
    err = i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_master_bus_add_device failed: %s", esp_err_to_name(err));
        return err;
    }

    /* 唤醒：PWR_MGMT_1 = 0x01（时钟选 PLL X 轴陀螺） */
    err = imu_write_reg(MPU_PWR_MGMT_1, 0x01);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "wake failed: %s", esp_err_to_name(err));
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(50));

    /* 量程：±4g / ±500dps */
    ESP_RETURN_ON_ERROR(imu_write_reg(MPU_ACCEL_CONFIG, 0x08), TAG, "accel range");
    ESP_RETURN_ON_ERROR(imu_write_reg(MPU_GYRO_CONFIG, 0x08), TAG, "gyro range");

    s_roll = s_pitch = s_yaw = 0.0f;
    s_last_us = esp_timer_get_time();

    ESP_LOGI(TAG, "imu ready (addr=0x%02x)", cfg->addr);
    return ESP_OK;
}

esp_err_t imu_read(imu_data_t *out)
{
    if (out == NULL || s_dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t raw[14];
    esp_err_t err = imu_read_regs(MPU_ACCEL_XOUT_H, raw, sizeof(raw));
    if (err != ESP_OK) {
        return err;
    }

    /* 大端合成有符号 16 位 */
    int16_t ax = (int16_t)((raw[0] << 8) | raw[1]);
    int16_t ay = (int16_t)((raw[2] << 8) | raw[3]);
    int16_t az = (int16_t)((raw[4] << 8) | raw[5]);
    int16_t gx = (int16_t)((raw[8] << 8) | raw[9]);
    int16_t gy = (int16_t)((raw[10] << 8) | raw[11]);
    int16_t gz = (int16_t)((raw[12] << 8) | raw[13]);

    out->ax = ax / ACCEL_SCALE;
    out->ay = ay / ACCEL_SCALE;
    out->az = az / ACCEL_SCALE;
    out->gx = gx / GYRO_SCALE;
    out->gy = gy / GYRO_SCALE;
    out->gz = gz / GYRO_SCALE;

    /* 互补滤波：陀螺仪高速积分 + 加速度计低速修正 */
    int64_t now = esp_timer_get_time();
    float dt = (float)(now - s_last_us) / 1000000.0f;
    s_last_us = now;
    if (dt > 0.0f && dt < 1.0f) {
        float accel_roll = atan2f(out->ay, out->az) * 180.0f / (float)M_PI;
        float accel_pitch = atan2f(-out->ax, sqrtf(out->ay * out->ay + out->az * out->az))
                            * 180.0f / (float)M_PI;

        s_roll = 0.98f * (s_roll + out->gx * dt) + 0.02f * accel_roll;
        s_pitch = 0.98f * (s_pitch + out->gy * dt) + 0.02f * accel_pitch;
        s_yaw += out->gz * dt; /* 无磁力计，长期会漂移 */
    }

    out->roll = s_roll;
    out->pitch = s_pitch;
    out->yaw = s_yaw;
    return ESP_OK;
}

i2c_master_bus_handle_t imu_get_bus(void)
{
    return s_bus;
}
