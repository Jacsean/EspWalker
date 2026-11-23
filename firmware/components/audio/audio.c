/**
 * @file audio.c
 * @brief I2S 标准模式录音/放音（ESP-IDF v6.1：driver/i2s_std.h）。
 */
#include "audio.h"

#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "audio";

static i2s_chan_handle_t s_rx = NULL;
static i2s_chan_handle_t s_tx = NULL;

esp_err_t audio_init(const audio_config_t *cfg)
{
    if (cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* --- 录音：INMP441 为从机，ESP32 提供 WS/SCK --- */
    i2s_chan_config_t rx_chan = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_RETURN_ON_ERROR(i2s_new_channel(&rx_chan, NULL, &s_rx), TAG, "new rx chan");

    i2s_std_config_t rx_std = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG((uint32_t)cfg->sample_rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = cfg->mic_sck,
            .ws = cfg->mic_ws,
            .din = cfg->mic_sd,
            .dout = I2S_GPIO_UNUSED,
            .invert_flags = {.mclk_inv = false, .bclk_inv = false, .ws_inv = false},
        },
    };
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(s_rx, &rx_std), TAG, "init rx");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(s_rx), TAG, "enable rx");

    /* --- 放音：MAX98357A 为从机 --- */
    i2s_chan_config_t tx_chan = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    ESP_RETURN_ON_ERROR(i2s_new_channel(&tx_chan, &s_tx, NULL), TAG, "new tx chan");

    i2s_std_config_t tx_std = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG((uint32_t)cfg->sample_rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = cfg->spk_bclk,
            .ws = cfg->spk_lrc,
            .din = I2S_GPIO_UNUSED,
            .dout = cfg->spk_din,
            .invert_flags = {.mclk_inv = false, .bclk_inv = false, .ws_inv = false},
        },
    };
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(s_tx, &tx_std), TAG, "init tx");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(s_tx), TAG, "enable tx");

    ESP_LOGI(TAG, "audio ready: rate=%d", cfg->sample_rate);
    return ESP_OK;
}

esp_err_t audio_rec_read(int16_t *buf, int max_samples, size_t *out_bytes)
{
    if (s_rx == NULL || buf == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    size_t bytes_read = 0;
    esp_err_t err = i2s_channel_read(s_rx, buf, (size_t)max_samples * sizeof(int16_t),
                                     &bytes_read, pdMS_TO_TICKS(100));
    if (out_bytes != NULL) {
        *out_bytes = bytes_read;
    }
    return err;
}

esp_err_t audio_play_write(const uint8_t *buf, size_t len)
{
    if (s_tx == NULL || buf == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    size_t written = 0;
    esp_err_t err = i2s_channel_write(s_tx, buf, len, &written, portMAX_DELAY);
    return (err == ESP_OK && written == len) ? ESP_OK : ESP_FAIL;
}
