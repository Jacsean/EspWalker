/**
 * @file cmd_dispatch.c
 * @brief 解析中心端下行指令（move/stop/capture/rec/play/led/config/query）并执行。
 */
#include "cmd_dispatch.h"

#include <stdio.h>
#include <string.h>
#include "cJSON.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app.h"
#include "board.h"
#include "camera.h"
#include "audio.h"
#include "comm.h"

static const char *TAG = "dispatch";

static esp_timer_handle_t s_move_timer = NULL;
static esp_timer_handle_t s_rec_timer = NULL;
static TaskHandle_t s_audio_task = NULL;
static volatile bool s_recording = false;
static int s_play_id = 0;

static int msg_id(cJSON *root)
{
    cJSON *j = cJSON_GetObjectItem(root, "id");
    return j != NULL ? j->valueint : 0;
}

static void send_ack(int id, int code, const char *msg)
{
    char buf[160];
    snprintf(buf, sizeof(buf), "{\"t\":\"ack\",\"id\":%d,\"code\":%d,\"msg\":\"%s\"}", id, code, msg);
    comm_send_json(buf);
}

static void send_status(int id)
{
    int dist = proximity_read_cm(g_prox);
    char buf[160];
    snprintf(buf, sizeof(buf), "{\"t\":\"status\",\"id\":%d,\"moving\":%s,\"dist_cm\":%d}",
             id, g_moving ? "true" : "false", dist);
    comm_send_json(buf);
}

static void move_timeout_cb(void *arg)
{
    (void)arg;
    g_moving = false;
    chassis_stop(g_chassis);
}

static void rec_timeout_cb(void *arg)
{
    (void)arg;
    s_recording = false;
}

static void audio_upload_task(void *arg)
{
    (void)arg;
    int16_t buf[512];
    for (;;) {
        if (s_recording) {
            size_t bytes = 0;
            if (audio_rec_read(buf, 512, &bytes) == ESP_OK && bytes > 0) {
                comm_send_bin(FRAME_PCM_UP, (const uint8_t *)buf, bytes);
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
}

static void handle_move(cJSON *root)
{
    cJSON *jvx = cJSON_GetObjectItem(root, "vx");
    cJSON *jvy = cJSON_GetObjectItem(root, "vy");
    cJSON *jom = cJSON_GetObjectItem(root, "omega");
    cJSON *jms = cJSON_GetObjectItem(root, "ms");

    float vx = jvx != NULL ? (float)jvx->valuedouble : 0.0f;
    float vy = jvy != NULL ? (float)jvy->valuedouble : 0.0f;
    float om = jom != NULL ? (float)jom->valuedouble : 0.0f;
    int ms = jms != NULL ? jms->valueint : 0;

    g_vx = vx;
    g_vy = vy;
    g_omega = om;
    g_moving = true;
    chassis_move(g_chassis, vx, vy, om);

    if (ms > 0) {
        if (s_move_timer == NULL) {
            esp_timer_create_args_t a = {.callback = move_timeout_cb, .name = "move_t"};
            esp_timer_create(&a, &s_move_timer);
        }
        esp_timer_stop(s_move_timer);
        esp_timer_start_once(s_move_timer, (uint64_t)ms * 1000);
    }
    send_ack(msg_id(root), 0, "ok");
}

static void handle_stop(cJSON *root)
{
    g_moving = false;
    chassis_stop(g_chassis);
    send_ack(msg_id(root), 0, "ok");
}

static void handle_capture(cJSON *root)
{
    app_capture_and_send();
    send_ack(msg_id(root), 0, "ok");
}

static void handle_rec(cJSON *root)
{
    cJSON *jon = cJSON_GetObjectItem(root, "on");
    cJSON *jms = cJSON_GetObjectItem(root, "ms");
    bool on = jon != NULL && cJSON_IsTrue(jon);
    int ms = jms != NULL ? jms->valueint : 0;

    s_recording = on;
    if (on) {
        if (s_audio_task == NULL) {
            xTaskCreate(audio_upload_task, "audio_up", 4096, NULL, 6, &s_audio_task);
        }
        if (ms > 0) {
            if (s_rec_timer == NULL) {
                esp_timer_create_args_t a = {.callback = rec_timeout_cb, .name = "rec_t"};
                esp_timer_create(&a, &s_rec_timer);
            }
            esp_timer_stop(s_rec_timer);
            esp_timer_start_once(s_rec_timer, (uint64_t)ms * 1000);
        }
    }
    send_ack(msg_id(root), 0, "ok");
}

static void handle_play(cJSON *root)
{
    /* 音频数据随后的 FRAME_PCM_DOWN 二进制帧到达，见 cmd_dispatch_on_bin */
    s_play_id = msg_id(root);
}

static void handle_led(cJSON *root)
{
    cJSON *jon = cJSON_GetObjectItem(root, "on");
    bool on = jon != NULL && cJSON_IsTrue(jon);
    gpio_set_level(BOARD_LED_GPIO, on ? 1 : 0);
    send_ack(msg_id(root), 0, "ok");
}

static void handle_config(cJSON *root)
{
    cJSON *js = cJSON_GetObjectItem(root, "prox_stop_cm");
    cJSON *jl = cJSON_GetObjectItem(root, "prox_slow_cm");
    if (js != NULL) {
        g_stop_cm = js->valueint;
    }
    if (jl != NULL) {
        g_slow_cm = jl->valueint;
    }
    send_ack(msg_id(root), 0, "ok");
}

static void handle_query(cJSON *root)
{
    send_status(msg_id(root));
}

void cmd_dispatch_on_json(const char *json, void *ctx)
{
    (void)ctx;
    cJSON *root = cJSON_Parse(json);
    if (root == NULL) {
        ESP_LOGW(TAG, "bad json: %s", json);
        return;
    }
    cJSON *t = cJSON_GetObjectItem(root, "t");
    if (t == NULL || t->valuestring == NULL) {
        cJSON_Delete(root);
        return;
    }

    if (strcmp(t->valuestring, "move") == 0) {
        handle_move(root);
    } else if (strcmp(t->valuestring, "stop") == 0) {
        handle_stop(root);
    } else if (strcmp(t->valuestring, "capture") == 0) {
        handle_capture(root);
    } else if (strcmp(t->valuestring, "rec") == 0) {
        handle_rec(root);
    } else if (strcmp(t->valuestring, "play") == 0) {
        handle_play(root);
    } else if (strcmp(t->valuestring, "led") == 0) {
        handle_led(root);
    } else if (strcmp(t->valuestring, "config") == 0) {
        handle_config(root);
    } else if (strcmp(t->valuestring, "query") == 0) {
        handle_query(root);
    } else {
        ESP_LOGW(TAG, "unknown msg type: %s", t->valuestring);
        send_ack(msg_id(root), 3, "unsupported");
    }
    cJSON_Delete(root);
}

void cmd_dispatch_on_bin(uint8_t type, const uint8_t *payload, size_t len, void *ctx)
{
    (void)ctx;
    if (type == FRAME_PCM_DOWN) {
        if (audio_play_write(payload, len) == ESP_OK) {
            send_ack(s_play_id, 0, "played");
        }
    } else {
        ESP_LOGW(TAG, "unexpected bin frame type 0x%02x", type);
    }
}

void cmd_dispatch_init(void)
{
#if CONFIG_APP_ENABLE_STATUS_LED
    gpio_config_t led = {
        .pin_bit_mask = 1ULL << BOARD_LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&led);
    gpio_set_level(BOARD_LED_GPIO, 0);
#endif
}
