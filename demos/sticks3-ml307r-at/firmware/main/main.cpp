#include <algorithm>
#include <cstring>
#include <memory>
#include <string>

#include "at_modem.h"
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

void bringup_task(void *) {
    uint32_t retry_ms = 2000;
    for (;;) {
        auto detected = AtModem::Detect(GPIO_NUM_5, GPIO_NUM_6, GPIO_NUM_NC, 115200, 10000);
        if (!detected) {
            ESP_LOGW(TAG, "MODEM_DETECT_FAILED reason=%s", detected.error().ToString().c_str());
        } else {
            auto modem = std::move(*detected);
            std::string model = modem->GetModuleRevision();
            ESP_LOGI(TAG, "MODEM_DETECTED model=%s uart=1 tx=5 rx=6", model.c_str());
            if (model.rfind("ML307R", 0) == 0) {
                NetworkStatus status = modem->WaitForNetworkReady(30000);
                if (status == NetworkStatus::Ready) {
                    int csq = modem->GetCsq();
                    ESP_LOGI(TAG, "NETWORK_READY carrier=%s csq=%d",
                             modem->GetCarrierName().c_str(), csq >= 0 && csq <= 31 ? csq : 99);
                    auto tcp = modem->CreateTcp(0);
                    auto connected = tcp->Connect(CONFIG_BRINGUP_TCP_HOST, CONFIG_BRINGUP_TCP_PORT);
                    if (connected) {
                        ESP_LOGI(TAG, "TCP_CONNECTED host=%s port=%d", CONFIG_BRINGUP_TCP_HOST,
                                 CONFIG_BRINGUP_TCP_PORT);
                        tcp->Disconnect();
                    } else {
                        ESP_LOGW(TAG, "TCP_CONNECT_FAILED reason=%s", connected.error().ToString().c_str());
                    }
                    retry_ms = 2000;
                } else {
                    ESP_LOGW(TAG, "NETWORK_WAIT_FAILED state=%d", static_cast<int>(status));
                }
            } else {
                ESP_LOGE(TAG, "MODEM_UNSUPPORTED model=%s", model.c_str());
            }
            ESP_LOGI(TAG, "STACK_FREE bringup=%u modem_receive=%u modem_event=%u",
                     (unsigned)uxTaskGetStackHighWaterMark(nullptr), free_stack("modem_receive"),
                     free_stack("modem_event"));
        }
        vTaskDelay(pdMS_TO_TICKS(retry_ms));
        retry_ms = std::min<uint32_t>(retry_ms * 2, 60000);
    }
}
}  // namespace

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "BOOT reset_reason=%d", esp_reset_reason());
    BaseType_t ok = xTaskCreatePinnedToCore(bringup_task, "bringup", TASK_STACK, nullptr, 5, nullptr, 1);
    if (ok != pdPASS) ESP_LOGE(TAG, "TASK_START_FAILED name=bringup");
}
