/**
 * @file comm.h
 * @brief 中心端 TCP 通信：JSON 行 + 二进制帧，自动重连。
 *        帧格式见 docs/03-protocol.md。
 */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FRAME_JPEG = 0x01,     /**< JPEG 图片（上行） */
    FRAME_PCM_UP = 0x02,   /**< PCM 音频（上行） */
    FRAME_PCM_DOWN = 0x03, /**< PCM 音频（下行，播放） */
} frame_type_t;

/** @brief 收到一行 JSON（\0 结尾）。回调运行在 comm 收包任务上下文，勿长时间阻塞。 */
typedef void (*comm_json_cb_t)(const char *json, void *ctx);

/** @brief 收到二进制帧。payload 仅在本回调内有效。 */
typedef void (*comm_bin_cb_t)(uint8_t type, const uint8_t *payload, size_t len, void *ctx);

typedef struct {
    const char *server_ip;
    int port;
    comm_json_cb_t on_json;
    comm_bin_cb_t on_bin;
    void *ctx;
} comm_config_t;

/** @brief 连接中心端并启动收包任务。连接失败返回错误（调用方可重启重试）。 */
esp_err_t comm_init(const comm_config_t *cfg);

/** @brief 发送一行 JSON（自动补 \n）。 */
esp_err_t comm_send_json(const char *json);

/** @brief 发送二进制帧。 */
esp_err_t comm_send_bin(uint8_t type, const uint8_t *payload, size_t len);

/** @brief 是否已连接。 */
bool comm_connected(void);

#ifdef __cplusplus
}
#endif
