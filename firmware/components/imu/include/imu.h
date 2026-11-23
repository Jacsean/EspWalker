/**
 * @file imu.h
 * @brief 6 轴 IMU（MPU6050 / MPU9250，I2C）读取与姿态解算。
 *
 * MPU6050 与 MPU9250 的加速度计/陀螺仪寄存器兼容；本组件读取 6 轴原始数据，
 * 用互补滤波解算 roll/pitch，yaw 由陀螺仪积分得到（无磁力计会缓慢漂移）。
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int i2c_sda;    /**< I2C SDA 引脚 */
    int i2c_scl;    /**< I2C SCL 引脚 */
    uint8_t addr;   /**< I2C 地址（AD0 接地 = 0x68） */
} imu_config_t;

typedef struct {
    float ax, ay, az;          /**< 加速度（g） */
    float gx, gy, gz;          /**< 角速度（deg/s） */
    float roll, pitch, yaw;    /**< 姿态角（度） */
} imu_data_t;

/** @brief 初始化 IMU（唤醒 + 量程配置 + 姿态初值）。 */
esp_err_t imu_init(const imu_config_t *cfg);

/** @brief 读取一次数据并更新姿态角。 */
esp_err_t imu_read(imu_data_t *out);

/** @brief 返回 IMU 创建的 I2C 总线句柄（未初始化时返回 NULL），供 TOF 等外设复用。 */
i2c_master_bus_handle_t imu_get_bus(void);

#ifdef __cplusplus
}
#endif
