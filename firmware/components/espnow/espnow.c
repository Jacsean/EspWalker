/**
 * @file espnow.c
 * @brief ESP-NOW 收发与自动配对实现。
 */
#include "espnow.h"

#include <string.h>
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "espnow";

static const uint8_t s_broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static bool s_master = false;
static bool s_peer_ready = false;
static uint8_t s_peer_mac[6] = {0};
static now_recv_cb_t s_on_recv = NULL;
static now_peer_cb_t s_on_peer = NULL;
static void *s_ctx = NULL;

static void add_peer_from(const uint8_t *mac)
{
    if (esp_now_is_peer_exist(mac)) {
        memcpy(s_peer_mac, mac, 6);
        s_peer_ready = true;
        return;
    }
    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = 0; /* 当前信道 */
    peer.ifidx = WIFI_IF_STA;
    if (esp_now_add_peer(&peer) == ESP_OK) {
        memcpy(s_peer_mac, mac, 6);
        s_peer_ready = true;
    }
}

static void recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    if (len != sizeof(now_msg_t)) {
        return;
    }
    const now_msg_t *msg = (const now_msg_t *)data;
    const uint8_t *src = info->src_addr;

    if (s_master) {
        /* 主控：收到从机任意帧 → 配对并回 ACK */
        if (!s_peer_ready) {
            add_peer_from(src);
            if (s_peer_ready) {
                now_msg_t ack = {.type = NOW_ACK, .ack = {.code = 0}};
                esp_now_send(s_peer_mac, (const uint8_t *)&ack, sizeof(ack));
                ESP_LOGI(TAG, "paired with slave %02x:%02x:%02x:%02x:%02x:%02x",
                         src[0], src[1], src[2], src[3], src[4], src[5]);
                if (s_on_peer != NULL) {
                    s_on_peer(s_ctx);
                }
            }
        }
    } else {
        /* 从机：收到主控 ACK → 完成配对 */
        if (msg->type == NOW_ACK && !s_peer_ready) {
            add_peer_from(src);
            if (s_peer_ready) {
                ESP_LOGI(TAG, "paired with master %02x:%02x:%02x:%02x:%02x:%02x",
                         src[0], src[1], src[2], src[3], src[4], src[5]);
                if (s_on_peer != NULL) {
                    s_on_peer(s_ctx);
                }
            }
        }
    }

    if (s_on_recv != NULL) {
        s_on_recv(src, msg, s_ctx);
    }
}

static void slave_pair_task(void *arg)
{
    (void)arg;
    now_msg_t hello = {.type = NOW_STATUS,
                       .status = {.dist_cm = -1, .moving = 0, .roll = 0, .pitch = 0, .yaw = 0}};

    /* 信道 1~13 轮流广播，直到主控（在路由器信道）收到并回 ACK */
    for (int ch = 1; ch <= 13 && !s_peer_ready; ch++) {
        esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
        for (int i = 0; i < 5 && !s_peer_ready; i++) {
            esp_now_send(s_broadcast_mac, (const uint8_t *)&hello, sizeof(hello));
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    ESP_LOGI(TAG, "slave pairing finished: %s", s_peer_ready ? "ok" : "timeout");
    vTaskDelete(NULL);
}

esp_err_t espnow_init(const now_config_t *cfg)
{
    if (cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    s_master = cfg->is_master;
    s_on_recv = cfg->on_recv;
    s_on_peer = cfg->on_peer;
    s_ctx = cfg->ctx;

    if (cfg->init_wifi) {
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&wcfg));
        ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
    }

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(recv_cb));

    if (!s_master) {
        /* 从机：添加广播 peer 供配对期广播 */
        esp_now_peer_info_t bpeer = {0};
        memcpy(bpeer.peer_addr, s_broadcast_mac, 6);
        bpeer.channel = 0;
        bpeer.ifidx = WIFI_IF_STA;
        if (esp_now_add_peer(&bpeer) != ESP_OK) {
            ESP_LOGW(TAG, "add broadcast peer failed");
        }
        xTaskCreate(slave_pair_task, "espnow_pair", 3072, NULL, 5, NULL);
    }

    ESP_LOGI(TAG, "espnow init (role=%s)", s_master ? "master" : "slave");
    return ESP_OK;
}

esp_err_t espnow_send(const now_msg_t *msg)
{
    if (msg == NULL || !s_peer_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = esp_now_send(s_peer_mac, (const uint8_t *)msg, sizeof(*msg));
    return err;
}

bool espnow_peer_ready(void)
{
    return s_peer_ready;
}
