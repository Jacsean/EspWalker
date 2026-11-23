/**
 * @file wifi_sta.h
 * @brief Wi-Fi STA 连接。
 */
#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t wifi_sta_init(const char *ssid, const char *pass);
bool wifi_sta_is_connected(void);

#ifdef __cplusplus
}
#endif
