/**
 * @file espnow.h
 * @brief 双板 ESP-NOW 通信：自动配对 + 消息收发。
 *
 * 主控(master)=ES3N28P，从机(slave)=GOOUUU。从机信道扫描广播 hello，
 * 主控收到后 add_peer 并回 ACK，从机收到 ACK 完成配对（见 docs/08-dual-board.md）。
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NOW_CMD_MOVE    = 0x01, /**< 主→从：运动 vx/vy/omega + 时长 ms */
    NOW_CMD_STOP    = 0x02, /**< 主→从：停车 */
    NOW_CMD_CAPTURE = 0x03, /**< 主→从：拍照 */
    NOW_CMD_CONFIG  = 0x04, /**< 主→从：避障阈值 */
    NOW_STATUS      = 0x10, /**< 从→主：周期状态 */
    NOW_PROXIMITY   = 0x11, /**< 从→主：避障事件 */
    NOW_ACK         = 0x20, /**< 应答 */
} now_msg_type_t;

typedef struct __attribute__((packed)) {
    uint8_t type;
    union {
        struct { float vx, vy, omega; uint16_t ms; } move;
        struct { int16_t stop_cm, slow_cm; } config;
        struct { int16_t dist_cm; uint8_t moving; float roll, pitch, yaw; } status;
        struct { uint8_t level; int16_t dist_cm; } proximity;
        struct { uint8_t code; } ack;
    };
} now_msg_t;

/** @brief 收到一帧（mac 为发送方）。回调在 espnow 任务上下文，勿长时间阻塞。 */
typedef void (*now_recv_cb_t)(const uint8_t *mac, const now_msg_t *msg, void *ctx);

/** @brief 配对成功回调。 */
typedef void (*now_peer_cb_t)(void *ctx);

typedef struct {
    bool is_master;   /**< true=主控，false=从机 */
    bool init_wifi;   /**< true=espnow 内部初始化 Wi-Fi（motion 用）；master 由 wifi_sta 先 init */
    now_recv_cb_t on_recv;
    now_peer_cb_t on_peer;
    void *ctx;
} now_config_t;

esp_err_t espnow_init(const now_config_t *cfg);

/** @brief 发送一帧给已配对 peer。未配对时返回 ESP_ERR_INVALID_STATE。 */
esp_err_t espnow_send(const now_msg_t *msg);

bool espnow_peer_ready(void);

#ifdef __cplusplus
}
#endif
