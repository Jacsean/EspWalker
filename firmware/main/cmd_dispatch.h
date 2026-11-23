/**
 * @file cmd_dispatch.h
 * @brief 中心端指令分发（comm 回调入口）。
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void cmd_dispatch_init(void);
void cmd_dispatch_on_json(const char *json, void *ctx);
void cmd_dispatch_on_bin(uint8_t type, const uint8_t *payload, size_t len, void *ctx);

#ifdef __cplusplus
}
#endif
