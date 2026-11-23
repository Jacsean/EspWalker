/**
 * @file audio.h
 * @brief 语音采集（INMP441 I2S 录音）与播放（MAX98357A I2S 放音）。
 *        默认关闭，启用见组件 CMakeLists。
 */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int sample_rate;  /**< 采样率，如 16000 */
    int mic_ws;       /**< INMP441 WS */
    int mic_sck;      /**< INMP441 SCK */
    int mic_sd;       /**< INMP441 SD */
    int spk_bclk;     /**< MAX98357A BCLK */
    int spk_lrc;      /**< MAX98357A LRC */
    int spk_din;      /**< MAX98357A DIN */
} audio_config_t;

/** @brief 同时初始化录音与放音两路 I2S。 */
esp_err_t audio_init(const audio_config_t *cfg);

/**
 * @brief 读一段 PCM（16 bit 单声道）。阻塞最长约 100 ms。
 * @param out_bytes [out] 实际读到的字节数
 */
esp_err_t audio_rec_read(int16_t *buf, int max_samples, size_t *out_bytes);

/** @brief 播放一段 PCM，阻塞到写完。 */
esp_err_t audio_play_write(const uint8_t *buf, size_t len);

#ifdef __cplusplus
}
#endif
