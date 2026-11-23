/**
 * @file app.h
 * @brief 应用全局状态与跨模块接口。
 */
#pragma once

#include "chassis.h"
#include "proximity.h"
#include "imu.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 全局句柄（app_main.c 定义） */
extern chassis_handle_t g_chassis;
extern proximity_handle_t g_prox;

/* 当前运动意图（供避障降速/恢复使用） */
extern float g_vx, g_vy, g_omega;
extern bool g_moving;

/* 避障阈值（可由 config 指令动态调整） */
extern int g_stop_cm, g_slow_cm;

/* IMU 姿态与可用标志 */
extern imu_data_t g_imu;
extern bool g_imu_ok;

/** @brief 拍照并发往中心端（JPEG 二进制帧）。 */
void app_capture_and_send(void);

#ifdef __cplusplus
}
#endif
