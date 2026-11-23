/**
 * @file comm.c
 * @brief TCP 客户端 + 帧解析状态机 + 自动重连。
 *
 * 同一流上混合两类帧：
 *  - 文本帧：一行 UTF-8 JSON，以 '\n' 结束；
 *  - 二进制帧：0xAA 0x57 | type | seq | len(4B 大端) | payload。
 */
#include "comm.h"

#include <string.h>
#include <stdlib.h>
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"

static const char *TAG = "comm";

#define RX_CHUNK 1024
#define JSON_BUF_SIZE 4096
#define MAX_PAYLOAD (512 * 1024)

static int s_sock = -1;
static char s_server_ip[32];
static int s_port = 0;
static comm_json_cb_t s_on_json = NULL;
static comm_bin_cb_t s_on_bin = NULL;
static void *s_ctx = NULL;
static SemaphoreHandle_t s_send_lock = NULL;

static esp_err_t comm_connect(void)
{
    s_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s_sock < 0) {
        ESP_LOGE(TAG, "socket failed");
        return ESP_FAIL;
    }

    struct timeval tv = {.tv_sec = 5, .tv_usec = 0};
    setsockopt(s_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(s_sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)s_port);
    addr.sin_addr.s_addr = inet_addr(s_server_ip);

    if (connect(s_sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        ESP_LOGW(TAG, "connect %s:%d failed", s_server_ip, s_port);
        close(s_sock);
        s_sock = -1;
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "connected to %s:%d", s_server_ip, s_port);
    return ESP_OK;
}

/* ---------- 帧解析状态机 ---------- */
typedef enum {
    S_JSON = 0,
    S_MAGIC1,
    S_HDR,
    S_PAYLOAD,
} rx_state_t;

static void comm_recv_task(void *arg)
{
    (void)arg;
    uint8_t chunk[RX_CHUNK];
    char json_buf[JSON_BUF_SIZE];
    size_t json_len = 0;

    uint8_t hdr[6];
    int hdr_pos = 0;
    uint8_t frame_type = 0;
    uint8_t *payload = NULL;
    size_t payload_len = 0;
    size_t payload_pos = 0;

    rx_state_t st = S_JSON;

    for (;;) {
        if (s_sock < 0) {
            if (comm_connect() != ESP_OK) {
                vTaskDelay(pdMS_TO_TICKS(2000));
                continue;
            }
        }

        int n = recv(s_sock, chunk, sizeof(chunk), 0);
        if (n <= 0) {
            ESP_LOGW(TAG, "recv closed (%d), reconnect", n);
            close(s_sock);
            s_sock = -1;
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        for (int i = 0; i < n; i++) {
            uint8_t b = chunk[i];
            switch (st) {
            case S_JSON:
                if (b == 0xAA) {
                    st = S_MAGIC1;
                } else if (b == '\n') {
                    if (json_len > 0) {
                        json_buf[json_len] = '\0';
                        if (s_on_json != NULL) {
                            s_on_json(json_buf, s_ctx);
                        }
                        json_len = 0;
                    }
                } else {
                    if (json_len < JSON_BUF_SIZE - 1) {
                        json_buf[json_len++] = (char)b;
                    }
                }
                break;

            case S_MAGIC1:
                if (b == 0x57) {
                    st = S_HDR;
                    hdr_pos = 0;
                } else {
                    /* 误判，退回两个字节到 JSON 缓冲 */
                    if (json_len < JSON_BUF_SIZE - 2) {
                        json_buf[json_len++] = (char)0xAA;
                        json_buf[json_len++] = (char)b;
                    }
                    st = S_JSON;
                }
                break;

            case S_HDR:
                hdr[hdr_pos++] = b;
                if (hdr_pos == 6) {
                    frame_type = hdr[0];
                    /* hdr[1] = seq，暂未使用 */
                    payload_len = ((size_t)hdr[2] << 24) | ((size_t)hdr[3] << 16) |
                                  ((size_t)hdr[4] << 8) | (size_t)hdr[5];
                    if (payload_len > MAX_PAYLOAD) {
                        ESP_LOGW(TAG, "oversized frame %u, drop", (unsigned)payload_len);
                        payload_len = 0;
                        st = S_JSON;
                        break;
                    }
                    payload = malloc(payload_len > 0 ? payload_len : 1);
                    if (payload == NULL) {
                        st = S_JSON;
                        break;
                    }
                    payload_pos = 0;
                    st = S_PAYLOAD;
                }
                break;

            case S_PAYLOAD:
                if (payload != NULL && payload_pos < payload_len) {
                    payload[payload_pos++] = b;
                }
                if (payload_pos >= payload_len) {
                    if (s_on_bin != NULL && payload != NULL) {
                        s_on_bin(frame_type, payload, payload_len, s_ctx);
                    }
                    free(payload);
                    payload = NULL;
                    payload_len = 0;
                    payload_pos = 0;
                    st = S_JSON;
                }
                break;
            }
        }
    }
}

static int send_all(const uint8_t *buf, size_t len)
{
    if (s_sock < 0) {
        return -1;
    }
    size_t sent = 0;
    while (sent < len) {
        int n = send(s_sock, buf + sent, len - sent, 0);
        if (n <= 0) {
            return -1;
        }
        sent += (size_t)n;
    }
    return 0;
}

esp_err_t comm_init(const comm_config_t *cfg)
{
    if (cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    strncpy(s_server_ip, cfg->server_ip, sizeof(s_server_ip) - 1);
    s_port = cfg->port;
    s_on_json = cfg->on_json;
    s_on_bin = cfg->on_bin;
    s_ctx = cfg->ctx;

    s_send_lock = xSemaphoreCreateMutex();
    if (s_send_lock == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = comm_connect();
    if (err != ESP_OK) {
        return err;
    }

    BaseType_t ret = xTaskCreate(comm_recv_task, "comm_rx", 6144, NULL, 5, NULL);
    if (ret != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t comm_send_json(const char *json)
{
    if (json == NULL || s_sock < 0) {
        return ESP_ERR_INVALID_STATE;
    }
    size_t len = strlen(json);
    xSemaphoreTake(s_send_lock, portMAX_DELAY);
    int r1 = send_all((const uint8_t *)json, len);
    int r2 = send_all((const uint8_t *)"\n", 1);
    xSemaphoreGive(s_send_lock);
    return (r1 == 0 && r2 == 0) ? ESP_OK : ESP_FAIL;
}

esp_err_t comm_send_bin(uint8_t type, const uint8_t *payload, size_t len)
{
    if (s_sock < 0) {
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t hdr[8];
    hdr[0] = 0xAA;
    hdr[1] = 0x57;
    hdr[2] = type;
    hdr[3] = 0; /* seq */
    hdr[4] = (uint8_t)((len >> 24) & 0xFF);
    hdr[5] = (uint8_t)((len >> 16) & 0xFF);
    hdr[6] = (uint8_t)((len >> 8) & 0xFF);
    hdr[7] = (uint8_t)(len & 0xFF);

    xSemaphoreTake(s_send_lock, portMAX_DELAY);
    int r = send_all(hdr, sizeof(hdr));
    if (r == 0 && len > 0) {
        r = send_all(payload, len);
    }
    xSemaphoreGive(s_send_lock);
    return r == 0 ? ESP_OK : ESP_FAIL;
}

bool comm_connected(void)
{
    return s_sock >= 0;
}
