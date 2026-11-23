/**
 * @file app_main.c
 * @brief Esp32Walker 主程序，按 CONFIG_APP_ROLE 分支：
 *   motion = GOOUUU 运动板（ESP-NOW 从机 + 电机/IMU/避障/摄像头）
 *   master = ES3N28P 主控板（Wi-Fi TCP 连 PC + ESP-NOW 主控中转）
 */
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "driver/gpio.h"

#include "app.h"
#include "board.h"
#include "wifi_sta.h"
#include "comm.h"
#include "camera.h"
#include "espnow.h"
#include "cJSON.h"

static const char *TAG = "app";

/* ---------- 全局状态（app.h 声明） ---------- */
chassis_handle_t g_chassis = NULL;
proximity_handle_t g_prox = NULL;
float g_vx = 0.0f, g_vy = 0.0f, g_omega = 0.0f;
bool g_moving = false;
int g_stop_cm = CONFIG_APP_PROX_STOP_CM;
int g_slow_cm = CONFIG_APP_PROX_SLOW_CM;
imu_data_t g_imu = {0};
bool g_imu_ok = false;

void app_capture_and_send(void)
{
    const uint8_t *jpeg = NULL;
    size_t len = 0;
    if (camera_capture_jpeg(&jpeg, &len) == ESP_OK && len > 0) {
        comm_send_bin(FRAME_JPEG, jpeg, len);
    } else {
        ESP_LOGW(TAG, "capture failed");
    }
}

static void chassis_setup(void)
{
    chassis_config_t cc = {0};
    const char *ct = CONFIG_APP_CHASSIS_TYPE;

    if (strcmp(ct, "mecanum") == 0) {
        cc.type = CHASSIS_MECANUM;
        cc.num_motors = 4;
        cc.motors[0] = (motor_config_t){BOARD_MOTOR1_PWM, BOARD_MOTOR1_IN1, BOARD_MOTOR1_IN2, 0};
        cc.motors[1] = (motor_config_t){BOARD_MOTOR2_PWM, BOARD_MOTOR2_IN1, BOARD_MOTOR2_IN2, 1};
        cc.motors[2] = (motor_config_t){BOARD_MOTOR3_PWM, BOARD_MOTOR3_IN1, BOARD_MOTOR3_IN2, 2};
        cc.motors[3] = (motor_config_t){BOARD_MOTOR4_PWM, BOARD_MOTOR4_IN1, BOARD_MOTOR4_IN2, 3};
    } else {
        cc.type = (strcmp(ct, "track") == 0) ? CHASSIS_TRACK : CHASSIS_DIFF2;
        cc.num_motors = 2;
        cc.motors[0] = (motor_config_t){BOARD_MOTOR1_PWM, BOARD_MOTOR1_IN1, BOARD_MOTOR1_IN2, 0};
        cc.motors[1] = (motor_config_t){BOARD_MOTOR2_PWM, BOARD_MOTOR2_IN1, BOARD_MOTOR2_IN2, 1};
    }
    cc.geometry[0] = 0.12f;
    cc.geometry[1] = 0.10f;
    ESP_ERROR_CHECK(chassis_init(&cc, &g_chassis));
}

static void proximity_setup(void)
{
    proximity_config_t pc = {
        .sensor = (prox_sensor_t)BOARD_PROX_SENSOR,
        .trig_gpio = BOARD_ULTRASONIC_TRIG,
        .echo_gpio = BOARD_ULTRASONIC_ECHO,
        .i2c_sda = BOARD_TOF_SDA,
        .i2c_scl = BOARD_TOF_SCL,
        .i2c_addr = 0x29,
        .stop_cm = g_stop_cm,
        .slow_cm = g_slow_cm,
        .max_cm = BOARD_PROX_MAX_CM,
    };
    ESP_ERROR_CHECK(proximity_init(&pc, &g_prox));
}

static void imu_setup(void)
{
#if CONFIG_APP_ENABLE_IMU
    imu_config_t ic = {
        .i2c_sda = BOARD_IMU_SDA,
        .i2c_scl = BOARD_IMU_SCL,
        .addr = (uint8_t)CONFIG_APP_IMU_ADDR,
    };
    if (imu_init(&ic) == ESP_OK) {
        g_imu_ok = true;
    } else {
        ESP_LOGW(TAG, "IMU init failed (continue without IMU)");
    }
#endif
}

/* ================= motion 角色 ================= */
#if CONFIG_APP_ROLE_MOTION

static bool s_slowed = false;
static esp_timer_handle_t s_move_timer = NULL;

static void move_timeout_cb(void *arg)
{
    (void)arg;
    g_moving = false;
    s_slowed = false;
    chassis_stop(g_chassis);
}

static void motion_on_recv(const uint8_t *mac, const now_msg_t *msg, void *ctx)
{
    (void)mac;
    (void)ctx;
    switch (msg->type) {
    case NOW_CMD_MOVE:
        g_vx = msg->move.vx;
        g_vy = msg->move.vy;
        g_omega = msg->move.omega;
        g_moving = true;
        chassis_move(g_chassis, g_vx, g_vy, g_omega);
        if (msg->move.ms > 0) {
            if (s_move_timer == NULL) {
                esp_timer_create_args_t a = {.callback = move_timeout_cb, .name = "move_t"};
                esp_timer_create(&a, &s_move_timer);
            }
            esp_timer_stop(s_move_timer);
            esp_timer_start_once(s_move_timer, (uint64_t)msg->move.ms * 1000);
        }
        break;
    case NOW_CMD_STOP:
        g_moving = false;
        s_slowed = false;
        chassis_stop(g_chassis);
        break;
    case NOW_CMD_CONFIG:
        g_stop_cm = msg->config.stop_cm;
        g_slow_cm = msg->config.slow_cm;
        ESP_LOGI(TAG, "config stop=%d slow=%d", g_stop_cm, g_slow_cm);
        break;
    case NOW_CMD_CAPTURE:
        /* 图传第二阶段实现，先回 ACK 占位 */
        ESP_LOGW(TAG, "capture not implemented yet");
        break;
    default:
        break;
    }
}

static void motion_sensor_task(void *arg)
{
    (void)arg;
    int last_level = -1;
    int status_cnt = 0;
    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(200));

        int dist = proximity_read_cm(g_prox);

        if (g_imu_ok) {
            imu_read(&g_imu);
            if (g_moving && (fabsf(g_imu.roll) > 60.0f || fabsf(g_imu.pitch) > 60.0f)) {
                g_moving = false;
                s_slowed = false;
                chassis_stop(g_chassis);
                ESP_LOGW(TAG, "tilt detected, stop");
            }
        }

        int level = PROX_OK;
        if (dist >= 0 && dist <= g_stop_cm) {
            level = PROX_STOP;
        } else if (dist >= 0 && dist <= g_slow_cm) {
            level = PROX_WARN;
        }

        if (g_moving) {
            if (level == PROX_STOP) {
                g_moving = false;
                s_slowed = false;
                chassis_stop(g_chassis);
            } else if (level == PROX_WARN) {
                s_slowed = true;
                chassis_move(g_chassis, g_vx * 0.4f, g_vy * 0.4f, g_omega * 0.4f);
            } else if (s_slowed) {
                s_slowed = false;
                chassis_move(g_chassis, g_vx, g_vy, g_omega);
            }
        }

        if (level != last_level) {
            last_level = level;
            now_msg_t m = {.type = NOW_PROXIMITY,
                           .proximity = {.level = (uint8_t)level, .dist_cm = (int16_t)dist}};
            espnow_send(&m);
        }

        if (++status_cnt >= 5) { /* 1 s 一次 */
            status_cnt = 0;
            now_msg_t m = {.type = NOW_STATUS,
                           .status = {.dist_cm = (int16_t)dist,
                                      .moving = (uint8_t)g_moving,
                                      .roll = g_imu.roll,
                                      .pitch = g_imu.pitch,
                                      .yaw = g_imu.yaw}};
            espnow_send(&m);
        }
    }
}

static void motion_run(void)
{
    chassis_setup();
    imu_setup();        /* 先初始化 IMU（创建 I2C 总线），供 TOF 复用 */
    proximity_setup();

    now_config_t nc = {
        .is_master = false,
        .init_wifi = true,
        .on_recv = motion_on_recv,
        .on_peer = NULL,
        .ctx = NULL,
    };
    ESP_ERROR_CHECK(espnow_init(&nc));

    xTaskCreate(motion_sensor_task, "motion", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "motion ready");
}

#else /* ================= master 角色 ================= */

/* 从机状态缓存（espnow 收到后更新） */
static int m_dist_cm = -1;
static bool m_moving = false;
static float m_roll = 0.0f, m_pitch = 0.0f, m_yaw = 0.0f;

static void master_send_status(int id)
{
    char buf[224];
    snprintf(buf, sizeof(buf),
             "{\"t\":\"status\",\"id\":%d,\"moving\":%s,\"dist_cm\":%d,"
             "\"roll\":%.1f,\"pitch\":%.1f,\"yaw\":%.1f}",
             id, m_moving ? "true" : "false", m_dist_cm, m_roll, m_pitch, m_yaw);
    comm_send_json(buf);
}

static void master_on_now(const uint8_t *mac, const now_msg_t *msg, void *ctx)
{
    (void)mac;
    (void)ctx;
    switch (msg->type) {
    case NOW_STATUS:
        m_dist_cm = msg->status.dist_cm;
        m_moving = msg->status.moving != 0;
        m_roll = msg->status.roll;
        m_pitch = msg->status.pitch;
        m_yaw = msg->status.yaw;
        break;
    case NOW_PROXIMITY: {
        const char *lv = msg->proximity.level == PROX_STOP ? "stop"
                         : (msg->proximity.level == PROX_WARN ? "warn" : "ok");
        char buf[160];
        snprintf(buf, sizeof(buf),
                 "{\"t\":\"proximity\",\"id\":0,\"zone\":\"front\",\"dist_cm\":%d,\"level\":\"%s\"}",
                 msg->proximity.dist_cm, lv);
        comm_send_json(buf);
        break;
    }
    default:
        break;
    }
}

static void master_on_json(const char *json, void *ctx)
{
    (void)ctx;
    cJSON *root = cJSON_Parse(json);
    if (root == NULL) {
        return;
    }
    cJSON *t = cJSON_GetObjectItem(root, "t");
    cJSON *jid = cJSON_GetObjectItem(root, "id");
    int id = jid != NULL ? jid->valueint : 0;

    if (t != NULL && t->valuestring != NULL) {
        now_msg_t m = {0};
        if (strcmp(t->valuestring, "move") == 0) {
            cJSON *j;
            m.type = NOW_CMD_MOVE;
            j = cJSON_GetObjectItem(root, "vx");    m.move.vx = j != NULL ? (float)j->valuedouble : 0.0f;
            j = cJSON_GetObjectItem(root, "vy");    m.move.vy = j != NULL ? (float)j->valuedouble : 0.0f;
            j = cJSON_GetObjectItem(root, "omega"); m.move.omega = j != NULL ? (float)j->valuedouble : 0.0f;
            j = cJSON_GetObjectItem(root, "ms");    m.move.ms = j != NULL ? (uint16_t)j->valueint : 0;
            espnow_send(&m);
        } else if (strcmp(t->valuestring, "stop") == 0) {
            m.type = NOW_CMD_STOP;
            espnow_send(&m);
        } else if (strcmp(t->valuestring, "capture") == 0) {
            m.type = NOW_CMD_CAPTURE;
            espnow_send(&m);
        } else if (strcmp(t->valuestring, "config") == 0) {
            cJSON *j;
            m.type = NOW_CMD_CONFIG;
            j = cJSON_GetObjectItem(root, "prox_stop_cm");
            m.config.stop_cm = j != NULL ? (int16_t)j->valueint : (int16_t)CONFIG_APP_PROX_STOP_CM;
            j = cJSON_GetObjectItem(root, "prox_slow_cm");
            m.config.slow_cm = j != NULL ? (int16_t)j->valueint : (int16_t)CONFIG_APP_PROX_SLOW_CM;
            espnow_send(&m);
        } else if (strcmp(t->valuestring, "query") == 0) {
            master_send_status(id);
        } else if (strcmp(t->valuestring, "led") == 0) {
            cJSON *j = cJSON_GetObjectItem(root, "on");
            gpio_set_level(BOARD_LED_GPIO, (j != NULL && cJSON_IsTrue(j)) ? 1 : 0);
        }

        char buf[64];
        snprintf(buf, sizeof(buf), "{\"t\":\"ack\",\"id\":%d,\"code\":0,\"msg\":\"ok\"}", id);
        comm_send_json(buf);
    }
    cJSON_Delete(root);
}

static void master_run(void)
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

    esp_err_t err = wifi_sta_init(CONFIG_APP_WIFI_SSID, CONFIG_APP_WIFI_PASS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "wifi init failed, restart");
        esp_restart();
    }

    comm_config_t cfg = {
        .server_ip = CONFIG_APP_SERVER_IP,
        .port = CONFIG_APP_SERVER_PORT,
        .on_json = master_on_json,
        .on_bin = NULL,
        .ctx = NULL,
    };
    err = comm_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "center connect failed, restart");
        esp_restart();
    }

    now_config_t nc = {
        .is_master = true,
        .init_wifi = false, /* Wi-Fi 已由 wifi_sta_init 完成 */
        .on_recv = master_on_now,
        .on_peer = NULL,
        .ctx = NULL,
    };
    ESP_ERROR_CHECK(espnow_init(&nc));

    char hello[192];
    snprintf(hello, sizeof(hello),
             "{\"t\":\"hello\",\"id\":1,\"fw\":\"esp32walker/0.2\",\"board\":\"es3n28p-master\","
             "\"chassis\":\"bridge\",\"cap\":[]}");
    comm_send_json(hello);

    ESP_LOGI(TAG, "master ready");
}

#endif /* CONFIG_APP_ROLE_MOTION */

void app_main(void)
{
#if CONFIG_APP_ROLE_MOTION
    ESP_LOGI(TAG, "Esp32Walker boot (role=motion, chassis=%s)", CONFIG_APP_CHASSIS_TYPE);
    motion_run();
#else
    ESP_LOGI(TAG, "Esp32Walker boot (role=master)");
    master_run();
#endif
}
