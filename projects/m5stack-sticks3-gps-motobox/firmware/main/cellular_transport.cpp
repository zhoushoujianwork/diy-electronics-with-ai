#include "cellular_transport.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstring>
#include <memory>
#include <string>
#include <sys/time.h>

#include "at_modem.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/stream_buffer.h"
#include "freertos/task.h"
#include "mbedtls/ssl.h"
#include "mbedtls/net_sockets.h"
#include "sdkconfig.h"
#include "tcp.h"
#include "udp.h"
#include "esp_transport_ws.h"

namespace {
constexpr auto TAG = "cellular";
constexpr gpio_num_t MODEM_TX_PIN = GPIO_NUM_7;
constexpr gpio_num_t MODEM_RX_PIN = GPIO_NUM_4;
constexpr EventBits_t MODEM_READY = BIT0;
constexpr size_t RX_CAPACITY = 32768;  // Full TLS record plus AT URC bursts.
constexpr int64_t MIN_UTC_MS = 1735689600000LL;
constexpr uint32_t NTP_TO_UNIX = 2208988800U;
std::unique_ptr<AtModem> modem;
EventGroupHandle_t modem_events;
std::atomic<bool> network_online{false};
std::atomic<bool> pdp_online{false};
cellular_status_callback_t status_callback;
cellular_time_callback_t time_callback;
cellular_status_t current{};

struct TlsTransport {
    std::unique_ptr<Tcp> tcp;
    StreamBufferHandle_t rx = nullptr;
    mbedtls_ssl_config config;
    mbedtls_ssl_context ssl;
    std::atomic<bool> disconnected{true};
    std::atomic<bool> overflow{false};
    bool handshook = false;

    TlsTransport() {
        mbedtls_ssl_config_init(&config);
        mbedtls_ssl_init(&ssl);
        rx = xStreamBufferCreateWithCaps(RX_CAPACITY, 1, MALLOC_CAP_SPIRAM);
    }
    ~TlsTransport() {
        tcp.reset();
        if (rx) vStreamBufferDeleteWithCaps(rx);
        mbedtls_ssl_free(&ssl);
        mbedtls_ssl_config_free(&config);
    }
    void reset_tls() {
        tcp.reset();
        mbedtls_ssl_free(&ssl);
        mbedtls_ssl_config_free(&config);
        mbedtls_ssl_init(&ssl);
        mbedtls_ssl_config_init(&config);
        if (rx) xStreamBufferReset(rx);
        disconnected.store(true);
        overflow.store(false);
        handshook = false;
    }
};

TlsTransport *ctx(esp_transport_handle_t t) {
    return static_cast<TlsTransport *>(esp_transport_get_context_data(t));
}

void copy_field(char *out, size_t size, const std::string &value) {
    size_t count = std::min(size - 1, value.size());
    for (size_t i = 0; i < count; ++i) {
        unsigned char ch = static_cast<unsigned char>(value[i]);
        out[i] = std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.'
            ? static_cast<char>(ch) : '_';
    }
    out[count] = 0;
}

void publish_status() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    int64_t utc_ms = int64_t(tv.tv_sec) * 1000 + tv.tv_usec / 1000;
    current.sampled_ms = utc_ms >= MIN_UTC_MS ? utc_ms : 0;
    if (status_callback) status_callback(&current);
}

uint32_t read_be32(const char *p) {
    const auto *b = reinterpret_cast<const uint8_t *>(p);
    return (uint32_t(b[0]) << 24) | (uint32_t(b[1]) << 16) | (uint32_t(b[2]) << 8) | b[3];
}

void ntp_sync() {
    auto udp = modem->CreateUdp(1);
    if (!udp) return;
    char request[48] = {};
    request[0] = 0x23;  // NTPv4 client.
    esp_fill_random(request + 40, 8);
    const std::string nonce(request + 40, 8);
    udp->OnMessage([nonce](const std::string &reply) {
        if (reply.size() < 48 || (uint8_t(reply[0]) & 7) != 4 ||
            (uint8_t(reply[0]) >> 6) == 3 || uint8_t(reply[1]) == 0 ||
            uint8_t(reply[1]) > 15 || reply.compare(24, 8, nonce) != 0) return;
        uint32_t seconds = read_be32(reply.data() + 40);
        if (seconds <= NTP_TO_UNIX) return;
        int64_t utc_ms = (int64_t(seconds) - NTP_TO_UNIX) * 1000 +
                         ((uint64_t(read_be32(reply.data() + 44)) * 1000) >> 32);
        if (utc_ms < MIN_UTC_MS) return;
        struct timeval tv = {.tv_sec = static_cast<time_t>(utc_ms / 1000),
                             .tv_usec = static_cast<suseconds_t>((utc_ms % 1000) * 1000)};
        if (settimeofday(&tv, nullptr) == 0 && time_callback) time_callback(utc_ms);
    });
    if (auto result = udp->Connect(CONFIG_DEMO_NTP_HOST, 123); !result) {
        ESP_LOGW(TAG, "NTP_CONNECT_FAILED reason=%s", result.error().ToString().c_str());
        return;
    }
    if (udp->Send(std::string(request, sizeof(request))) != static_cast<int>(sizeof(request))) {
        ESP_LOGW(TAG, "NTP_SEND_FAILED");
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(3000));
}

void modem_task(void *) {
    uint32_t backoff_ms = 2000;
    for (;;) {
        if (!modem) {
            auto result = AtModem::Detect(MODEM_TX_PIN, MODEM_RX_PIN, GPIO_NUM_NC, 115200, 10000);
            if (!result) {
                copy_field(current.error, sizeof(current.error), result.error().ToString());
                publish_status();
                ESP_LOGW(TAG, "MODEM_DETECT_FAILED reason=%s", current.error);
                vTaskDelay(pdMS_TO_TICKS(backoff_ms));
                backoff_ms = std::min<uint32_t>(backoff_ms * 2, 60000);
                current.reconnects++;
                continue;
            }
            modem = std::move(*result);
            modem->OnNetworkStateChanged([](bool ready) {
                network_online.store(ready);
                if (!ready) {
                    pdp_online.store(false);
                    xEventGroupClearBits(modem_events, MODEM_READY);
                }
            });
            modem->GetAtUart()->RegisterUrcCallback([](const std::string &command,
                                                        const std::vector<AtArgumentValue> &args) {
                if (command != "MIPCALL" || args.size() < 2 ||
                    args[0].type != AtArgumentValue::Type::Int ||
                    args[1].type != AtArgumentValue::Type::Int) return;
                bool ready = args[1].int_value == 1 &&
                             args.size() >= 3 &&
                             args[2].type == AtArgumentValue::Type::String &&
                             !args[2].string_value.empty() && args[2].string_value != "0.0.0.0";
                pdp_online.store(ready);
                if (!ready) xEventGroupClearBits(modem_events, MODEM_READY);
            });
            copy_field(current.model, sizeof(current.model), modem->GetModuleRevision());
            ESP_LOGI(TAG, "MODEM_DETECTED model=%s uart=1 tx=%d rx=%d", current.model,
                     static_cast<int>(MODEM_TX_PIN), static_cast<int>(MODEM_RX_PIN));
        }
        if (strncmp(current.model, "ML307R", 6) != 0) {
            copy_field(current.error, sizeof(current.error), "unexpected_modem");
            publish_status();
            ESP_LOGE(TAG, "MODEM_UNSUPPORTED model=%s", current.model);
            vTaskDelay(pdMS_TO_TICKS(60000));
            continue;
        }
        pdp_online.store(false);
        NetworkStatus network = modem->WaitForNetworkReady(30000);
        current.sim_ready = modem->pin_ready();
        current.registered = network == NetworkStatus::Ready;
        current.data_ready = false;
        current.csq = network == NetworkStatus::Ready ? modem->GetCsq() : 99;
        if (current.csq < 0 || current.csq > 31) current.csq = 99;
        if (network != NetworkStatus::Ready) {
            snprintf(current.error, sizeof(current.error), "network_%d", static_cast<int>(network));
            xEventGroupClearBits(modem_events, MODEM_READY);
            publish_status();
            ESP_LOGW(TAG, "NETWORK_WAIT_FAILED state=%d retry_ms=%u", static_cast<int>(network), backoff_ms);
            vTaskDelay(pdMS_TO_TICKS(backoff_ms));
            backoff_ms = std::min<uint32_t>(backoff_ms * 2, 60000);
            current.reconnects++;
            continue;
        }
        copy_field(current.carrier, sizeof(current.carrier), modem->GetCarrierName());
        for (int attempt = 0; attempt < 4 && !pdp_online.load(); ++attempt) {
            static_cast<void>(modem->GetAtUart()->SendCommand("AT+MIPCALL?"));
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        current.data_ready = pdp_online.load();
        if (!current.data_ready) {
            copy_field(current.error, sizeof(current.error), "pdp_not_ready");
            xEventGroupClearBits(modem_events, MODEM_READY);
            publish_status();
            ESP_LOGW(TAG, "PDP_NOT_READY retry_ms=%u", backoff_ms);
            vTaskDelay(pdMS_TO_TICKS(backoff_ms));
            backoff_ms = std::min<uint32_t>(backoff_ms * 2, 60000);
            current.reconnects++;
            continue;
        }
        current.error[0] = 0;
        xEventGroupSetBits(modem_events, MODEM_READY);
        backoff_ms = 2000;
        publish_status();
        ESP_LOGI(TAG, "NETWORK_READY carrier=%s csq=%d", current.carrier, current.csq);
        ntp_sync();
        for (int i = 0; i < 30; ++i) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            if (!network_online.load() || !pdp_online.load()) break;
        }
        pdp_online.store(false);
        static_cast<void>(modem->GetAtUart()->SendCommand("AT+MIPCALL?"));
        vTaskDelay(pdMS_TO_TICKS(100));
        if (!network_online.load() || !pdp_online.load()) {
            current.registered = network_online.load();
            current.data_ready = false;
            current.csq = current.registered ? modem->GetCsq() : 99;
            if (current.csq < 0 || current.csq > 31) current.csq = 99;
            copy_field(current.error, sizeof(current.error),
                       current.registered ? "pdp_lost" : "network_lost");
            xEventGroupClearBits(modem_events, MODEM_READY);
            publish_status();
            ESP_LOGW(TAG, "DATA_LOST reason=%s", current.error);
        } else {
            current.data_ready = true;
            int csq = modem->GetCsq();
            current.csq = csq >= 0 && csq <= 31 ? csq : 99;
            publish_status();
        }
    }
}

int random_bytes(void *, unsigned char *output, size_t size) {
    esp_fill_random(output, size);
    return 0;
}

int bio_send(void *opaque, const unsigned char *data, size_t size) {
    auto *c = static_cast<TlsTransport *>(opaque);
    if (!c->tcp || c->disconnected.load()) return MBEDTLS_ERR_NET_CONN_RESET;
    int sent = c->tcp->Send(std::string(reinterpret_cast<const char *>(data), size));
    return sent <= 0 ? MBEDTLS_ERR_NET_SEND_FAILED : sent;
}

int bio_recv(void *opaque, unsigned char *data, size_t size) {
    auto *c = static_cast<TlsTransport *>(opaque);
    size_t count = xStreamBufferReceive(c->rx, data, size, pdMS_TO_TICKS(200));
    if (count) return static_cast<int>(count);
    if (c->disconnected.load() || c->overflow.load()) return MBEDTLS_ERR_NET_CONN_RESET;
    return MBEDTLS_ERR_SSL_WANT_READ;
}

int close_tls(esp_transport_handle_t t) {
    ctx(t)->reset_tls();
    return 0;
}

int connect_tls(esp_transport_handle_t t, const char *host, int port, int timeout_ms) {
    auto *c = ctx(t);
    c->reset_tls();
    if (!c->rx || !host || port <= 0 || port > 65535) return -1;
    if (!(xEventGroupWaitBits(modem_events, MODEM_READY, pdFALSE, pdTRUE,
                              pdMS_TO_TICKS(timeout_ms)) & MODEM_READY)) return -1;
    c->tcp = modem->CreateTcp(0);
    c->tcp->OnStream([c](const std::string &data) {
        if (xStreamBufferSend(c->rx, data.data(), data.size(), 0) != data.size()) c->overflow.store(true);
    });
    c->tcp->OnDisconnected([c]() { c->disconnected.store(true); });
    c->disconnected.store(false);
    if (auto result = c->tcp->Connect(host, port); !result) {
        ESP_LOGE(TAG, "TCP_CONNECT_FAILED reason=%s", result.error().ToString().c_str());
        c->reset_tls();
        return -1;
    }
    if (c->disconnected.load()) {
        ESP_LOGE(TAG, "TCP_DISCONNECTED_DURING_CONNECT");
        c->reset_tls();
        return -1;
    }
    int rc = mbedtls_ssl_config_defaults(&c->config, MBEDTLS_SSL_IS_CLIENT,
                                          MBEDTLS_SSL_TRANSPORT_STREAM,
                                          MBEDTLS_SSL_PRESET_DEFAULT);
    if (!rc) {
        mbedtls_ssl_conf_authmode(&c->config, MBEDTLS_SSL_VERIFY_REQUIRED);
        mbedtls_ssl_conf_rng(&c->config, random_bytes, nullptr);
        rc = esp_crt_bundle_attach(&c->config) == ESP_OK ? 0 : -1;
    }
    if (!rc) rc = mbedtls_ssl_setup(&c->ssl, &c->config);
    if (!rc) rc = mbedtls_ssl_set_hostname(&c->ssl, host);
    if (!rc) mbedtls_ssl_set_bio(&c->ssl, c, bio_send, bio_recv, nullptr);
    int64_t deadline = esp_timer_get_time() / 1000 + std::max(timeout_ms, 15000);
    while (!rc && esp_timer_get_time() / 1000 < deadline) {
        rc = mbedtls_ssl_handshake(&c->ssl);
        if (rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE) rc = 0;
        else if (rc == 0) break;
    }
    if (rc || !mbedtls_ssl_is_handshake_over(&c->ssl) ||
        mbedtls_ssl_get_verify_result(&c->ssl) != 0 ||
        esp_timer_get_time() / 1000 >= deadline) {
        ESP_LOGE(TAG, "TLS_FAILED code=%d verify=0x%lx", rc,
                 static_cast<unsigned long>(mbedtls_ssl_get_verify_result(&c->ssl)));
        c->reset_tls();
        return -1;
    }
    c->handshook = true;
    ESP_LOGI(TAG, "TLS_VERIFIED host=%s", host);
    return 1;
}

int read_tls(esp_transport_handle_t t, char *buffer, int len, int timeout_ms) {
    auto *c = ctx(t);
    if (!c->handshook) return -1;
    int64_t deadline = esp_timer_get_time() / 1000 + timeout_ms;
    int rc;
    do {
        rc = mbedtls_ssl_read(&c->ssl, reinterpret_cast<unsigned char *>(buffer), len);
    } while ((rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE) &&
             esp_timer_get_time() / 1000 < deadline);
    return rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE ? 0 : rc;
}

int write_tls(esp_transport_handle_t t, const char *buffer, int len, int timeout_ms) {
    auto *c = ctx(t);
    if (!c->handshook) return -1;
    int64_t deadline = esp_timer_get_time() / 1000 + timeout_ms;
    int rc;
    do {
        rc = mbedtls_ssl_write(&c->ssl, reinterpret_cast<const unsigned char *>(buffer), len);
    } while ((rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE) &&
             esp_timer_get_time() / 1000 < deadline);
    return rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE ? -1 : rc;
}

int poll_read(esp_transport_handle_t t, int timeout_ms) {
    auto *c = ctx(t);
    int64_t deadline = esp_timer_get_time() / 1000 + timeout_ms;
    do {
        if (mbedtls_ssl_get_bytes_avail(&c->ssl) || xStreamBufferBytesAvailable(c->rx)) return 1;
        if (c->disconnected.load() || c->overflow.load()) return -1;
        vTaskDelay(pdMS_TO_TICKS(20));
    } while (esp_timer_get_time() / 1000 < deadline);
    return 0;
}

int poll_write(esp_transport_handle_t t, int) {
    auto *c = ctx(t);
    return c->handshook && !c->disconnected.load() && !c->overflow.load() ? 1 : -1;
}

int destroy_tls(esp_transport_handle_t t) {
    delete ctx(t);
    return 0;
}
}  // namespace

extern "C" esp_err_t cellular_start(cellular_status_callback_t status_cb,
                                      cellular_time_callback_t time_cb) {
    current.csq = 99;
    status_callback = status_cb;
    time_callback = time_cb;
    publish_status();
    modem_events = xEventGroupCreate();
    if (!modem_events) return ESP_ERR_NO_MEM;
    BaseType_t ok = xTaskCreatePinnedToCore(modem_task, "cellular", 12288, nullptr, 5, nullptr, 1);
    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

extern "C" esp_transport_handle_t cellular_mqtt_transport_create(const char *uri) {
    if (!uri || (strncmp(uri, "mqtts://", 8) != 0 && strncmp(uri, "wss://", 6) != 0)) {
        return nullptr;
    }
    esp_transport_handle_t handle = esp_transport_init();
    if (!handle) return nullptr;
    auto *context = new TlsTransport;
    if (!context || !context->rx) {
        delete context;
        esp_transport_destroy(handle);
        return nullptr;
    }
    esp_transport_set_context_data(handle, context);
    esp_transport_set_func(handle, connect_tls, read_tls, write_tls, close_tls,
                           poll_read, poll_write, destroy_tls);
    esp_transport_set_default_port(handle, 8883);
    if (strncmp(uri, "mqtts://", 8) == 0) return handle;

    esp_transport_handle_t ws = esp_transport_ws_init(handle);
    if (!ws) {
        esp_transport_destroy(handle);
        return nullptr;
    }
    const char *path = strchr(uri + 6, '/');
    esp_transport_ws_set_path(ws, path ? path : "/");
    if (esp_transport_ws_set_subprotocol(ws, "mqtt") != ESP_OK) {
        esp_transport_destroy(ws);
        esp_transport_destroy(handle);
        return nullptr;
    }
    esp_transport_set_default_port(ws, 443);
    // ESP-MQTT keeps the custom handle for its lifetime. The WebSocket handle
    // uses the TLS handle as its parent; both live until the device restarts.
    return ws;
}
