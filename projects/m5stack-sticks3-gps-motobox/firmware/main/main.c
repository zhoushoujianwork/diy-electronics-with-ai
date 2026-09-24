#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "board.h"
#include "binding_announcement.h"
#include "binding_protocol.h"
#include "demo_status.h"
#include "driver/uart.h"
#include "esp_app_desc.h"
#include "esp_attr.h"
#include "esp_check.h"
#include "esp_crt_bundle.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "gnss_parser.h"
#include "mqtt_client.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "telemetry_payload.h"
#include "telemetry_queue.h"
#include "ui.h"

#define FIX_MAX_AGE_MS 5000
#define GNSS_ONLINE_MAX_AGE_MS 5000
#define BINDING_STATUS_POLL_MS 10000
#define WIFI_CONNECTED_BIT BIT0
#define GPS_TASK_STACK 4096
#define TELEMETRY_TASK_STACK 6144
#define HEARTBEAT_TASK_STACK 4096

static const char *TAG = "motobox_demo";
static portMUX_TYPE state_lock = portMUX_INITIALIZER_UNLOCKED;
static demo_status_t status;
static gnss_fix_t latest_fix;
static int64_t latest_fix_monotonic_ms;
static int64_t latest_nmea_monotonic_ms;
static EXT_RAM_BSS_ATTR telemetry_queue_t telemetry_queue;
static SemaphoreHandle_t queue_mutex;
static EventGroupHandle_t wifi_events;
static esp_mqtt_client_handle_t mqtt_client;
static TaskHandle_t gps_task_handle;
static TaskHandle_t telemetry_task_handle;
static TaskHandle_t heartbeat_task_handle;
static char device_id[24];
static char telemetry_topic[96];
static char binding_request_topic[96];
static char binding_response_topic[96];
static char binding_request_id[BINDING_REQUEST_ID_LENGTH + 1];
static int binding_subscribe_id;
static uint64_t next_sequence = 1;
static bool sntp_started;
static bool clock_trusted;
static bool binding_voice_ready;
static bool binding_code_requested;

static int64_t wall_time_ms(void)
{
    struct timeval value;
    gettimeofday(&value, NULL);
    return (int64_t)value.tv_sec * 1000 + value.tv_usec / 1000;
}

static bool wall_time_trusted(void)
{
    bool trusted;
    portENTER_CRITICAL(&state_lock);
    trusted = clock_trusted;
    portEXIT_CRITICAL(&state_lock);
    return trusted && wall_time_ms() >= 1735689600000LL; /* 2025-01-01 UTC */
}

static void mark_clock_trusted(const char *source, int64_t utc_ms)
{
    bool changed;
    portENTER_CRITICAL(&state_lock);
    changed = !clock_trusted;
    clock_trusted = true;
    portEXIT_CRITICAL(&state_lock);
    if (changed) {
        ESP_LOGI(TAG, "TIME_TRUSTED source=%s utc_ms=%" PRId64, source, utc_ms);
    }
}

static void snapshot_status(demo_status_t *out)
{
    portENTER_CRITICAL(&state_lock);
    *out = status;
    int64_t fix_ms = latest_fix_monotonic_ms;
    int64_t nmea_ms = latest_nmea_monotonic_ms;
    portEXIT_CRITICAL(&state_lock);
    int64_t age = fix_ms ? esp_timer_get_time() / 1000 - fix_ms : UINT32_MAX;
    out->fix_age_ms = age < 0 ? 0 : age > UINT32_MAX ? UINT32_MAX : (uint32_t)age;
    out->fix_valid = out->fix_valid && out->fix_age_ms <= FIX_MAX_AGE_MS;
    out->gnss_online = nmea_ms && esp_timer_get_time() / 1000 - nmea_ms <= GNSS_ONLINE_MAX_AGE_MS;
    out->time_trusted = wall_time_trusted();
    if (out->binding_ready && out->time_trusted && wall_time_ms() >= out->binding_expires_ms) {
        out->binding_ready = false;
    }
    if (out->wifi_connected) {
        wifi_ap_record_t ap;
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) out->wifi_rssi = ap.rssi;
    }
    if (xSemaphoreTake(queue_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        out->queue_depth = telemetry_queue_count(&telemetry_queue);
        out->queue_dropped = telemetry_queue_dropped(&telemetry_queue);
        xSemaphoreGive(queue_mutex);
    }
}

static void set_status_flag(bool *field, bool value)
{
    portENTER_CRITICAL(&state_lock);
    *field = value;
    portEXIT_CRITICAL(&state_lock);
}

static void set_clock_from_gnss(int64_t utc_ms)
{
    if (utc_ms < 1735689600000LL) return;
    int64_t difference = llabs(wall_time_ms() - utc_ms);
    bool synchronized = difference <= 5000;
    if (!wall_time_trusted() || difference > 5000) {
        struct timeval value = {.tv_sec = utc_ms / 1000, .tv_usec = (utc_ms % 1000) * 1000};
        if (settimeofday(&value, NULL) == 0) {
            synchronized = true;
            ESP_LOGI(TAG, "TIME_SYNC source=GNSS utc_ms=%" PRId64 " drift_ms=%" PRId64,
                     utc_ms, difference);
        }
    }
    if (synchronized) mark_clock_trusted("GNSS", utc_ms);
}

static void gps_task(void *unused)
{
    (void)unused;
    uart_config_t config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, 4096, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, STICKS3_GPS_UART_TX_GPIO,
                                 STICKS3_GPS_UART_RX_GPIO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    gnss_parser_t parser;
    gnss_parser_init(&parser);
    uint8_t bytes[256];
    ESP_LOGI(TAG, "GNSS_READY uart=1 baud=115200 rx=%d tx=%d", STICKS3_GPS_UART_RX_GPIO,
             STICKS3_GPS_UART_TX_GPIO);
    for (;;) {
        int count = uart_read_bytes(UART_NUM_1, bytes, sizeof(bytes), pdMS_TO_TICKS(1000));
        for (int i = 0; i < count; ++i) {
            gnss_fix_t fix;
            uint32_t before = parser.nmea_sentence_count;
            bool has_fix = gnss_parser_feed(&parser, (char)bytes[i], &fix);
            if (parser.nmea_sentence_count != before) {
                portENTER_CRITICAL(&state_lock);
                latest_nmea_monotonic_ms = esp_timer_get_time() / 1000;
                status.gnss_rmc_status = parser.rmc_status;
                status.gnss_gga_seen = parser.gga_seen;
                status.gnss_gga_quality = parser.gga_quality;
                status.satellites = parser.reported_satellites < 0 ? 0 : parser.reported_satellites;
                status.hdop = parser.reported_hdop;
                portEXIT_CRITICAL(&state_lock);
            }
            if (!has_fix) continue;
            set_clock_from_gnss(fix.utc_ms);
            portENTER_CRITICAL(&state_lock);
            latest_fix = fix;
            latest_fix_monotonic_ms = esp_timer_get_time() / 1000;
            status.fix_valid = fix.valid;
            status.satellites = fix.satellites;
            status.hdop = fix.hdop;
            status.speed_kmh = fix.speed_kmh;
            portEXIT_CRITICAL(&state_lock);
            ESP_LOGI(TAG, "GNSS_FIX lat=%.6f lon=%.6f sat=%d hdop=%.1f speed=%.1f",
                     fix.latitude, fix.longitude, fix.satellites, fix.hdop, fix.speed_kmh);
        }
    }
}

static void sntp_sync_callback(struct timeval *value)
{
    int64_t utc_ms = (int64_t)value->tv_sec * 1000 + value->tv_usec / 1000;
    if (utc_ms >= 1735689600000LL) {
        mark_clock_trusted("SNTP", utc_ms);
    }
}

static void start_sntp_once(void)
{
    if (sntp_started) return;
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    config.sync_cb = sntp_sync_callback;
    if (esp_netif_sntp_init(&config) == ESP_OK) {
        sntp_started = true;
        ESP_LOGI(TAG, "SNTP_STARTED server=pool.ntp.org");
    }
}

static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_err_t err = esp_wifi_connect();
        if (err != ESP_OK) ESP_LOGE(TAG, "WIFI_CONNECT_FAILED err=%s", esp_err_to_name(err));
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *event = data;
        portENTER_CRITICAL(&state_lock);
        status.wifi_connected = false;
        status.wifi_ip[0] = 0;
        status.wifi_rssi = 0;
        portEXIT_CRITICAL(&state_lock);
        xEventGroupClearBits(wifi_events, WIFI_CONNECTED_BIT);
        esp_err_t err = esp_wifi_connect();
        ESP_LOGW(TAG, "WIFI_DISCONNECTED reason=%d reconnect_err=%s", event->reason,
                 esp_err_to_name(err));
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = data;
        char ip[16];
        snprintf(ip, sizeof(ip), IPSTR, IP2STR(&event->ip_info.ip));
        wifi_ap_record_t ap;
        int rssi = esp_wifi_sta_get_ap_info(&ap) == ESP_OK ? ap.rssi : 0;
        portENTER_CRITICAL(&state_lock);
        status.wifi_connected = true;
        strcpy(status.wifi_ip, ip);
        status.wifi_rssi = rssi;
        portEXIT_CRITICAL(&state_lock);
        xEventGroupSetBits(wifi_events, WIFI_CONNECTED_BIT);
        start_sntp_once();
        ESP_LOGI(TAG, "WIFI_CONNECTED ip=%s rssi=%d", ip, rssi);
    }
}

static esp_err_t wifi_init(void)
{
    wifi_events = xEventGroupCreate();
    ESP_RETURN_ON_FALSE(wifi_events, ESP_ERR_NO_MEM, TAG, "create Wi-Fi event group");
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif init");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "event loop");
    ESP_RETURN_ON_FALSE(esp_netif_create_default_wifi_sta(), ESP_ERR_NO_MEM, TAG, "STA netif");
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init), TAG, "Wi-Fi init");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event, NULL), TAG,
                        "Wi-Fi event handler");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event, NULL), TAG,
                        "IP event handler");
    wifi_config_t config = {0};
    strncpy((char *)config.sta.ssid, CONFIG_DEMO_WIFI_SSID, sizeof(config.sta.ssid) - 1);
    strncpy((char *)config.sta.password, CONFIG_DEMO_WIFI_PASSWORD, sizeof(config.sta.password) - 1);
    config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    config.sta.pmf_cfg.capable = true;
    config.sta.pmf_cfg.required = false;
    portENTER_CRITICAL(&state_lock);
    strncpy(status.wifi_ssid, CONFIG_DEMO_WIFI_SSID, sizeof(status.wifi_ssid) - 1);
    portEXIT_CRITICAL(&state_lock);
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "Wi-Fi STA mode");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &config), TAG, "Wi-Fi config");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "Wi-Fi start");
    return ESP_OK;
}

static bool publish_binding_request(bool status_only)
{
    char payload[160];
    size_t length = status_only ?
        binding_status_request_build(payload, sizeof(payload), device_id, binding_request_id) :
        binding_request_build(payload, sizeof(payload), device_id, binding_request_id);
    int message_id = length ? esp_mqtt_client_publish(mqtt_client, binding_request_topic,
                                                       payload, (int)length, 1, 0) : -1;
    if (message_id < 0) {
        ESP_LOGE(TAG, "BINDING_%s_REQUEST_FAILED msg_id=%d", status_only ? "STATUS" : "CODE",
                 message_id);
        return false;
    }
    ESP_LOGI(TAG, "BINDING_%s_REQUESTED msg_id=%d", status_only ? "STATUS" : "CODE",
             message_id);
    return true;
}

static void mqtt_event(void *args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)args;
    (void)base;
    esp_mqtt_event_handle_t event = event_data;
    if (event_id == MQTT_EVENT_CONNECTED) {
        set_status_flag(&status.mqtt_connected, true);
        binding_subscribe_id = esp_mqtt_client_subscribe(mqtt_client, binding_response_topic, 1);
        ESP_LOGI(TAG, "MQTT_CONNECTED uri=%s", CONFIG_DEMO_MQTT_URI);
    } else if (event_id == MQTT_EVENT_DISCONNECTED) {
        portENTER_CRITICAL(&state_lock);
        status.mqtt_connected = false;
        status.binding_known = false;
        binding_code_requested = false;
        portEXIT_CRITICAL(&state_lock);
        xSemaphoreTake(queue_mutex, portMAX_DELAY);
        telemetry_queue_retry_inflight(&telemetry_queue);
        xSemaphoreGive(queue_mutex);
        ESP_LOGW(TAG, "MQTT_DISCONNECTED");
    } else if (event_id == MQTT_EVENT_SUBSCRIBED && event->msg_id == binding_subscribe_id) {
        publish_binding_request(true);
    } else if (event_id == MQTT_EVENT_DATA) {
        size_t expected_topic_length = strlen(binding_response_topic);
        bool matching_topic = event->topic_len == (int)expected_topic_length &&
                              memcmp(event->topic, binding_response_topic, expected_topic_length) == 0;
        if (matching_topic && event->data_len == event->total_data_len) {
            bool bound;
            if (binding_status_parse(event->data, (size_t)event->data_len, device_id,
                                     binding_request_id, &bound)) {
                bool was_bound;
                bool needs_code;
                bool code_requested;
                portENTER_CRITICAL(&state_lock);
                was_bound = status.binding_known && status.binding_bound;
                status.binding_known = true;
                status.binding_bound = bound;
                if (bound || was_bound) {
                    status.binding_ready = false;
                    status.binding_code[0] = 0;
                }
                needs_code = !bound && !status.binding_ready;
                if (bound) binding_code_requested = false;
                code_requested = binding_code_requested;
                portEXIT_CRITICAL(&state_lock);
                ESP_LOGI(TAG, "BINDING_STATUS bound=%d", bound);
                if (needs_code && !code_requested) {
                    portENTER_CRITICAL(&state_lock);
                    binding_code_requested = true;
                    portEXIT_CRITICAL(&state_lock);
                    if (!publish_binding_request(false)) {
                        portENTER_CRITICAL(&state_lock);
                        binding_code_requested = false;
                        portEXIT_CRITICAL(&state_lock);
                    }
                }
            } else {
                binding_response_t response;
                if (!binding_response_parse(event->data, (size_t)event->data_len, device_id,
                                       binding_request_id, &response)) {
                    ESP_LOGW(TAG, "BINDING_RESPONSE_REJECTED");
                    return;
                }
                bool changed;
                portENTER_CRITICAL(&state_lock);
                if (!binding_code_requested) {
                    portEXIT_CRITICAL(&state_lock);
                    ESP_LOGI(TAG, "BINDING_CODE_IGNORED reason=status_probe");
                    return;
                }
                if (status.binding_known && status.binding_bound) {
                    portEXIT_CRITICAL(&state_lock);
                    ESP_LOGW(TAG, "BINDING_CODE_IGNORED reason=already_bound");
                    return;
                }
                changed = !status.binding_ready || strcmp(status.binding_code, response.code) != 0;
                status.binding_ready = true;
                memcpy(status.binding_code, response.code, sizeof(status.binding_code));
                status.binding_expires_ms = response.expires_ms;
                binding_code_requested = true;
                portEXIT_CRITICAL(&state_lock);
                if (changed) ESP_LOGI(TAG, "BINDING_CODE_READY expires_ms=%" PRId64, response.expires_ms);
                if (changed && binding_voice_ready) {
                    esp_err_t voice_err = binding_announcement_enqueue(response.code);
                    if (voice_err != ESP_OK) {
                        ESP_LOGE(TAG, "BINDING_VOICE_QUEUE_FAILED err=%s", esp_err_to_name(voice_err));
                    }
                }
            }
        }
    } else if (event_id == MQTT_EVENT_PUBLISHED) {
        xSemaphoreTake(queue_mutex, portMAX_DELAY);
        telemetry_item_t *head = telemetry_queue_head(&telemetry_queue);
        uint64_t sequence = head ? head->sequence : 0;
        if (telemetry_queue_ack(&telemetry_queue, event->msg_id)) {
            ESP_LOGI(TAG, "MQTT_ACK seq=%" PRIu64 " msg_id=%d", sequence, event->msg_id);
        }
        xSemaphoreGive(queue_mutex);
    } else if (event_id == MQTT_EVENT_ERROR) {
        ESP_LOGE(TAG, "MQTT_ERROR type=%d", event->error_handle ? event->error_handle->error_type : -1);
    }
}

static esp_err_t mqtt_init(void)
{
    esp_mqtt_client_config_t config = {
        .broker.address.uri = CONFIG_DEMO_MQTT_URI,
        .broker.verification.crt_bundle_attach = esp_crt_bundle_attach,
        .credentials.client_id = device_id,
        .credentials.username = CONFIG_DEMO_MQTT_USERNAME,
        .credentials.authentication.password = CONFIG_DEMO_MQTT_PASSWORD,
        .network.reconnect_timeout_ms = 5000,
        .session.protocol_ver = MQTT_PROTOCOL_V_3_1_1,
        .session.keepalive = 30,
    };
    mqtt_client = esp_mqtt_client_init(&config);
    ESP_RETURN_ON_FALSE(mqtt_client, ESP_ERR_NO_MEM, TAG, "MQTT client init");
    ESP_RETURN_ON_ERROR(esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event, NULL), TAG,
                        "MQTT event handler");
    return esp_mqtt_client_start(mqtt_client);
}

static size_t build_payload(char *buffer, size_t capacity, uint64_t sequence, int64_t now_ms)
{
    demo_status_t current;
    gnss_fix_t fix;
    int64_t fix_monotonic;
    int64_t nmea_monotonic;
    portENTER_CRITICAL(&state_lock);
    current = status;
    fix = latest_fix;
    fix_monotonic = latest_fix_monotonic_ms;
    nmea_monotonic = latest_nmea_monotonic_ms;
    portEXIT_CRITICAL(&state_lock);
    int64_t monotonic_now = esp_timer_get_time() / 1000;
    uint32_t age = fix_monotonic ? (uint32_t)(monotonic_now - fix_monotonic) : UINT32_MAX;
    bool fresh = current.fix_valid && age <= FIX_MAX_AGE_MS;
    bool gnss_online = nmea_monotonic && monotonic_now - nmea_monotonic <= GNSS_ONLINE_MAX_AGE_MS;
    telemetry_payload_input_t input = {
        .device_id = device_id,
        .firmware = esp_app_get_description()->version,
        .now_ms = now_ms,
        .sequence = sequence,
        .uptime_s = (uint32_t)(esp_timer_get_time() / 1000000),
        .free_heap = (uint32_t)esp_get_free_heap_size(),
        .wifi_connected = current.wifi_connected,
        .gnss_online = gnss_online,
        .gnss_rmc_status = current.gnss_rmc_status,
        .gnss_gga_quality = current.gnss_gga_seen ? current.gnss_gga_quality : -1,
        .gnss_satellites = current.satellites,
        .gnss_hdop = current.hdop,
        .has_fix = fresh,
        .fix_age_ms = age,
        .fix = fix,
    };
    return telemetry_payload_build(buffer, capacity, &input);
}

static void telemetry_task(void *unused)
{
    (void)unused;
    int64_t next_sample_ms = 0;
    char payload[TELEMETRY_PAYLOAD_MAX];
    int64_t next_binding_poll_ms = esp_timer_get_time() / 1000 + BINDING_STATUS_POLL_MS;
    for (;;) {
        int64_t now_mono = esp_timer_get_time() / 1000;
        int64_t now_wall = wall_time_ms();
        bool mqtt_up;
        bool code_expired = false;
        portENTER_CRITICAL(&state_lock);
        mqtt_up = status.mqtt_connected;
        if (status.binding_ready && clock_trusted && now_wall >= status.binding_expires_ms) {
            status.binding_ready = false;
            status.binding_code[0] = 0;
            binding_code_requested = false;
            code_expired = true;
        }
        portEXIT_CRITICAL(&state_lock);
        if (code_expired) ESP_LOGI(TAG, "BINDING_CODE_EXPIRED");
        if (mqtt_up && now_mono >= next_binding_poll_ms) {
            portENTER_CRITICAL(&state_lock);
            if (!status.binding_ready && !status.binding_bound) binding_code_requested = false;
            portEXIT_CRITICAL(&state_lock);
            publish_binding_request(true);
            next_binding_poll_ms = now_mono + BINDING_STATUS_POLL_MS;
        }
        if (wall_time_trusted() && now_mono >= next_sample_ms) {
            uint64_t sequence = next_sequence++;
            size_t length = build_payload(payload, sizeof(payload), sequence, wall_time_ms());
            if (length && xSemaphoreTake(queue_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                telemetry_queue_push(&telemetry_queue, sequence, payload);
                size_t depth = telemetry_queue_count(&telemetry_queue);
                uint32_t dropped = telemetry_queue_dropped(&telemetry_queue);
                xSemaphoreGive(queue_mutex);
                ESP_LOGI(TAG, "TELEMETRY_QUEUED seq=%" PRIu64 " bytes=%u depth=%u dropped=%u",
                         sequence, (unsigned)length, (unsigned)depth, dropped);
            }
            next_sample_ms = now_mono + CONFIG_DEMO_PUBLISH_INTERVAL_MS;
        }

        bool connected;
        portENTER_CRITICAL(&state_lock);
        connected = status.mqtt_connected;
        portEXIT_CRITICAL(&state_lock);
        if (connected && xSemaphoreTake(queue_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            telemetry_item_t *item = telemetry_queue_head(&telemetry_queue);
            if (item && !item->in_flight) {
                int id = esp_mqtt_client_publish(mqtt_client, telemetry_topic, item->payload, 0, 1, 0);
                if (telemetry_queue_mark_published(&telemetry_queue, id)) {
                    ESP_LOGI(TAG, "MQTT_PUBLISH seq=%" PRIu64 " msg_id=%d qos=1 retain=0",
                             item->sequence, id);
                }
            }
            xSemaphoreGive(queue_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void heartbeat_task(void *unused)
{
    (void)unused;
    int64_t next_log_ms = 0;
    for (;;) {
        demo_status_t current;
        snapshot_status(&current);
        ui_set_status(&current);
        int64_t now_ms = esp_timer_get_time() / 1000;
        if (now_ms >= next_log_ms) {
            ESP_LOGI(TAG,
            "HEARTBEAT gps_online=%d gps_fix=%d rmc=%c gga_q=%d sat=%d hdop=%.1f fix_age_ms=%u wifi=%d ip=%s rssi=%d mqtt=%d binding_known=%d bound=%d utc=%d queue=%u dropped=%u "
            "stack_gps=%u stack_telemetry=%u stack_ui=%u stack_voice=%u stack_heartbeat=%u heap=%u",
            current.gnss_online, current.fix_valid,
            current.gnss_rmc_status ? current.gnss_rmc_status : '?',
            current.gnss_gga_seen ? current.gnss_gga_quality : -1,
            current.satellites, current.hdop, current.fix_age_ms,
            current.wifi_connected, current.wifi_connected ? current.wifi_ip : "-", current.wifi_rssi,
            current.mqtt_connected, current.binding_known, current.binding_bound, current.time_trusted,
            current.queue_depth, current.queue_dropped,
            (unsigned)uxTaskGetStackHighWaterMark(gps_task_handle),
            (unsigned)uxTaskGetStackHighWaterMark(telemetry_task_handle),
            ui_stack_high_water_mark(), binding_announcement_stack_high_water_mark(),
            (unsigned)uxTaskGetStackHighWaterMark(NULL),
            (unsigned)esp_get_free_heap_size());
            next_log_ms = now_ms + 10000;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void make_device_id(void)
{
    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));
    snprintf(device_id, sizeof(device_id), "BOX-%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    snprintf(telemetry_topic, sizeof(telemetry_topic), "vehicle/v1/%s/telemetry", device_id);
    snprintf(binding_request_topic, sizeof(binding_request_topic),
             "vehicle/v1/%s/binding/request", device_id);
    snprintf(binding_response_topic, sizeof(binding_response_topic),
             "vehicle/v1/%s/binding/response", device_id);
    uint8_t request_bytes[16];
    esp_fill_random(request_bytes, sizeof(request_bytes));
    for (size_t i = 0; i < sizeof(request_bytes); ++i) {
        snprintf(binding_request_id + i * 2, 3, "%02x", request_bytes[i]);
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    make_device_id();
    ESP_LOGI(TAG, "BOOT device_id=%s model=m5stack-sticks3-gps firmware=%s reset_reason=%d",
             device_id, esp_app_get_description()->version, esp_reset_reason());
    ESP_ERROR_CHECK(board_power_init());
    esp_err_t voice_err = binding_announcement_init();
    if (voice_err == ESP_OK) {
        binding_voice_ready = true;
    } else {
        ESP_LOGE(TAG, "BINDING_VOICE_UNAVAILABLE err=%s", esp_err_to_name(voice_err));
    }
    ESP_ERROR_CHECK(ui_init(device_id));

    queue_mutex = xSemaphoreCreateMutex();
    ESP_ERROR_CHECK(queue_mutex ? ESP_OK : ESP_ERR_NO_MEM);
    telemetry_queue_init(&telemetry_queue);

    if (strlen(CONFIG_DEMO_WIFI_SSID) == 0 || strlen(CONFIG_DEMO_MQTT_USERNAME) == 0 ||
        strlen(CONFIG_DEMO_MQTT_PASSWORD) == 0 ||
        (strncmp(CONFIG_DEMO_MQTT_URI, "mqtts://", 8) != 0 &&
         strncmp(CONFIG_DEMO_MQTT_URI, "wss://", 6) != 0)) {
        ESP_LOGE(TAG, "CONFIG_INVALID require Wi-Fi, device MQTT credentials, and mqtts:// or wss:// URI");
        return;
    }

    ESP_ERROR_CHECK(wifi_init());
    ESP_ERROR_CHECK(mqtt_init());
    BaseType_t ok;
    ok = xTaskCreatePinnedToCore(gps_task, "gps", GPS_TASK_STACK, NULL, 7, &gps_task_handle, 1);
    ESP_ERROR_CHECK(ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ok = xTaskCreatePinnedToCore(telemetry_task, "telemetry", TELEMETRY_TASK_STACK, NULL, 6,
                                 &telemetry_task_handle, 0);
    ESP_ERROR_CHECK(ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ok = xTaskCreatePinnedToCore(heartbeat_task, "heartbeat", HEARTBEAT_TASK_STACK, NULL, 3,
                                 &heartbeat_task_handle, 0);
    ESP_ERROR_CHECK(ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}
