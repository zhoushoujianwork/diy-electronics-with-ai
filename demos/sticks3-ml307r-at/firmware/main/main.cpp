#include <algorithm>
#include <cstring>
#include <memory>
#include <string>

#include "at_modem.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "tcp.h"

namespace {
constexpr char TAG[] = "ml307_bringup";
constexpr uint32_t TASK_STACK = 12288;

unsigned free_stack(const char *name) {
    TaskHandle_t handle = xTaskGetHandle(name);
    return handle ? uxTaskGetStackHighWaterMark(handle) : 0;
}

void raw_at_probe() {
    uart_config_t config = {};
    config.baud_rate = 115200;
    config.data_bits = UART_DATA_8_BITS;
    config.parity = UART_PARITY_DISABLE;
    config.stop_bits = UART_STOP_BITS_1;
    config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    config.source_clk = UART_SCLK_DEFAULT;
    esp_err_t err = uart_param_config(UART_NUM_1, &config);
    if (err == ESP_OK) err = uart_set_pin(UART_NUM_1, GPIO_NUM_5, GPIO_NUM_6,
                                         UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err == ESP_OK) err = uart_driver_install(UART_NUM_1, 512, 0, 0, nullptr, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "RAW_AT_INIT_FAILED reason=%s", esp_err_to_name(err));
        return;
    }

    constexpr int rates[] = {115200, 921600, 460800, 230400, 57600, 38400, 19200, 9600};
    for (int rate : rates) {
        err = uart_set_baudrate(UART_NUM_1, rate);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "RAW_AT_BAUD_FAILED rate=%d reason=%s", rate, esp_err_to_name(err));
            continue;
        }
        for (int attempt = 1; attempt <= 2; ++attempt) {
            uart_flush_input(UART_NUM_1);
            int sent = uart_write_bytes(UART_NUM_1, "AT\r\n", 4);
            err = uart_wait_tx_done(UART_NUM_1, pdMS_TO_TICKS(200));
            uint8_t data[1024] = {};
            int received = std::max(0, uart_read_bytes(UART_NUM_1, data, sizeof(data),
                                                        pdMS_TO_TICKS(300)));
            bool ok = received >= 2 && std::search(data, data + received,
                        reinterpret_cast<const uint8_t *>("OK"),
                        reinterpret_cast<const uint8_t *>("OK") + 2) != data + received;
            int printable = std::count_if(data, data + received, [](uint8_t c) {
                return (c >= 32 && c <= 126) || c == '\r' || c == '\n';
            });
            int zeros = std::count(data, data + received, 0);
            int all_ones = std::count(data, data + received, 0xff);
            ESP_LOGI(TAG, "RAW_AT_PROBE rate=%d attempt=%d tx_bytes=%d rx_bytes=%d ok=%d printable=%d zero=%d ff=%d tx_done=%s",
                     rate, attempt, sent, received, ok, printable, zeros, all_ones, esp_err_to_name(err));
            if (ok) {
                uart_driver_delete(UART_NUM_1);
                return;
            }
        }
    }
    uart_driver_delete(UART_NUM_1);
}

void bringup_task(void *) {
    bool probed_raw_uart = false;
    uint32_t retry_ms = 2000;
    for (;;) {
        bool tcp_ok = false;
        auto detected = AtModem::Detect(GPIO_NUM_5, GPIO_NUM_6, GPIO_NUM_NC, 115200, 10000);
        if (!detected) {
            ESP_LOGW(TAG, "MODEM_DETECT_FAILED reason=%s", detected.error().ToString().c_str());
            if (!probed_raw_uart) {
                raw_at_probe();
                probed_raw_uart = true;
            }
        } else {
            auto modem = std::move(*detected);
            std::string model = modem->GetModuleRevision();
            ESP_LOGI(TAG, "MODEM_DETECTED model=%s uart=1 tx=5 rx=6", model.c_str());
            if (model.rfind("ML307R", 0) == 0) {
                NetworkStatus status = modem->WaitForNetworkReady(30000);
                if (status == NetworkStatus::Ready) {
                    int csq = modem->GetCsq();
                    ESP_LOGI(TAG, "CELL_REGISTERED carrier=%s csq=%d",
                             modem->GetCarrierName().c_str(), csq >= 0 && csq <= 31 ? csq : 99);
                    auto tcp = modem->CreateTcp(0);
                    auto connected = tcp->Connect(CONFIG_BRINGUP_TCP_HOST, CONFIG_BRINGUP_TCP_PORT);
                    if (connected) {
                        ESP_LOGI(TAG, "TCP_CONNECTED host=%s port=%d", CONFIG_BRINGUP_TCP_HOST,
                                 CONFIG_BRINGUP_TCP_PORT);
                        tcp_ok = true;
                        tcp->Disconnect();
                    } else {
                        ESP_LOGW(TAG, "TCP_CONNECT_FAILED reason=%s", connected.error().ToString().c_str());
                    }
                } else {
                    ESP_LOGW(TAG, "NETWORK_WAIT_FAILED state=%d", static_cast<int>(status));
                }
            } else {
                ESP_LOGE(TAG, "MODEM_UNSUPPORTED model=%s", model.c_str());
            }
        }
        ESP_LOGI(TAG, "STACK_FREE bringup=%u modem_receive=%u modem_event=%u",
                 (unsigned)uxTaskGetStackHighWaterMark(nullptr), free_stack("modem_receive"),
                 free_stack("modem_event"));
        vTaskDelay(pdMS_TO_TICKS(tcp_ok ? 30000 : retry_ms));
        retry_ms = tcp_ok ? 2000 : std::min<uint32_t>(retry_ms * 2, 60000);
    }
}
}  // namespace

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "BOOT reset_reason=%d", esp_reset_reason());
    BaseType_t ok = xTaskCreatePinnedToCore(bringup_task, "bringup", TASK_STACK, nullptr, 5, nullptr, 1);
    if (ok != pdPASS) ESP_LOGE(TAG, "TASK_START_FAILED name=bringup");
}
