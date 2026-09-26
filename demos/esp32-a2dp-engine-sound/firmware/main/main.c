#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "controls.h"
#include "engine_voice.h"
#include "portal.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_a2dp_api.h"
#include "esp_app_desc.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "a2dp_engine";
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static ev_control_t desired = {.profile=2, .volume=.20f};
static bool connected, streaming, boot_pressed, want_tasks;
static bool link_busy;
static uint32_t errors, dropped_events, callback_hwm, pcm_callbacks, pcm_bytes, underrun_bytes;
static uint32_t inquiry_results, inquiry_named;
static uint32_t render_max_us, generated, nonzero_blocks;
static int rpm_now, peak_now;
static ev_phase_t phase_now;
static TaskHandle_t synth_handle, bt_handle, console_handle, heartbeat_handle;

/* Static PCM/engine/task-status storage: no large arrays on task stacks.
 * synth 6144 B: ev_render + scalar local state + transition logging.
 * manager 6144 B: small event + BT API dispatch + error formatting.
 * main 8192 B: initialization/NVS + 160-byte command + parser/printf;
 * AP PIN update adds NVS write/commit and restart (measure in that path).
 * heartbeat 4096 B: snapshots + logging; task-status array is static.
 * BTC/BTU 6144 B each: GAP/A2DP callback, local name buffers (<=313 B),
 * inquiry counters/logging and protocol call paths. Offline scan previously
 * left >=4544 B; remeasure after this change and again with SBC load.
 * IDF's own encoder task stack is measured along with these at runtime.
 * Sizes are budgets; actual loaded high-water marks must be recorded.
 */
static ev_engine_t engine;
static int16_t mono[EV_BLOCK];
enum { RING_BYTES=16384, QUEUE_TARGET=8192 };
static uint8_t pcm_ring[RING_BYTES];
static size_t ring_read, ring_write, ring_used;
static TaskStatus_t task_status[32];

enum { EV_READY, EV_DISCOVERY, EV_CONNECTION, EV_AUDIO, EV_ACK, EV_SELECT };
typedef struct { int kind, value, cmd; uint8_t bda[6]; } bt_event_t;
static QueueHandle_t events;

static bool checked(const char *op, esp_err_t rc) {
    if (rc == ESP_OK) return true;
    portENTER_CRITICAL(&lock); ++errors; portEXIT_CRITICAL(&lock);
    ESP_LOGE(TAG, "%s failed: %s (0x%x)", op, esp_err_to_name(rc), (unsigned)rc);
    return false;
}

static void post(bt_event_t e) {
    if (xQueueSend(events, &e, 0) != pdTRUE) {
        portENTER_CRITICAL(&lock); ++dropped_events; portEXIT_CRITICAL(&lock);
    }
}

static bool portal_connect_selected(const uint8_t address[6]) {
    bool busy;
    portENTER_CRITICAL(&lock); busy=connected || link_busy; portEXIT_CRITICAL(&lock);
    if(busy) return false;
    bt_event_t e={.kind=EV_SELECT};
    memcpy(e.bda,address,6);
    return xQueueSend(events,&e,0)==pdTRUE;
}

/* Called by the SBC encoder task: copy only, never synthesize, log or wait. */
static int32_t audio_data(uint8_t *data, int32_t len) {
    if (!data || len <= 0) return 0;
    memset(data, 0, len);
    unsigned hwm = uxTaskGetStackHighWaterMark(NULL);
    portENTER_CRITICAL(&lock);
    size_t take = (size_t)len < ring_used ? (size_t)len : ring_used;
    take &= ~(size_t)3;
    size_t first = take < RING_BYTES-ring_read ? take : RING_BYTES-ring_read;
    memcpy(data, pcm_ring+ring_read, first);
    memcpy(data+first, pcm_ring, take-first);
    ring_read = (ring_read+take)%RING_BYTES; ring_used -= take;
    ++pcm_callbacks; pcm_bytes += len; underrun_bytes += (size_t)len-take;
    if (!callback_hwm || hwm < callback_hwm) callback_hwm = hwm;
    portEXIT_CRITICAL(&lock);
    return len;
}

static void gap_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *p) {
    if (event == ESP_BT_GAP_DISC_RES_EVT) {
        char name[ESP_BT_GAP_MAX_BDNAME_LEN+1] = {0};
        portENTER_CRITICAL(&lock); ++inquiry_results; portEXIT_CRITICAL(&lock);
        int rssi=-127, name_rank=0;
        for (int i=0; i<p->disc_res.num_prop; ++i) {
            esp_bt_gap_dev_prop_t *prop = &p->disc_res.prop[i];
            const uint8_t *s = NULL; size_t n = 0; int rank=0;
            if (prop->type == ESP_BT_GAP_DEV_PROP_RSSI && prop->len) rssi=*(int8_t *)prop->val;
            if (prop->type == ESP_BT_GAP_DEV_PROP_BDNAME && prop->len > 0) {
                s = prop->val; n = prop->len; rank=2;
            } else if (prop->type == ESP_BT_GAP_DEV_PROP_EIR) {
                uint8_t count = 0;
                s = esp_bt_gap_resolve_eir_data(prop->val, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &count);
                if (s) rank=2;
                else { s = esp_bt_gap_resolve_eir_data(prop->val, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &count); rank=1; }
                n = count;
            }
            if (s && n && rank>=name_rank) {
                if (n >= sizeof(name)) n = sizeof(name)-1;
                memcpy(name, s, n); name[n] = 0;
                name_rank=rank;
            }
        }
        portENTER_CRITICAL(&lock);
        if (name[0]) ++inquiry_named;
        portEXIT_CRITICAL(&lock);
        portal_record_device(p->disc_res.bda,name,rssi);
        /* List every discoverable Classic device; only a page selection may connect. */
    } else if (event == ESP_BT_GAP_DISC_STATE_CHANGED_EVT) {
        if (p->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED) {
            portENTER_CRITICAL(&lock);
            inquiry_results=inquiry_named=0;
            portEXIT_CRITICAL(&lock);
        } else {
            uint32_t results,named;
            portENTER_CRITICAL(&lock);
            results=inquiry_results; named=inquiry_named;
            portEXIT_CRITICAL(&lock);
            ESP_LOGI(TAG,"INQUIRY results=%" PRIu32 " named=%" PRIu32,results,named);
        }
        post((bt_event_t){.kind=EV_DISCOVERY, .value=p->disc_st_chg.state});
    } else if (event == ESP_BT_GAP_AUTH_CMPL_EVT) {
        ESP_LOGI(TAG, "AUTH status=%d", p->auth_cmpl.stat);
    } else if (event == ESP_BT_GAP_CFM_REQ_EVT) {
        checked("ssp_confirm", esp_bt_gap_ssp_confirm_reply(p->cfm_req.bda, true));
    } else if (event == ESP_BT_GAP_PIN_REQ_EVT) {
        esp_bt_pin_code_t pin = {'0','0','0','0'};
        checked("pin_reply", esp_bt_gap_pin_reply(p->pin_req.bda, !p->pin_req.min_16_digit, 4, pin));
    }
}

static void a2dp_callback(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *p) {
    switch (event) {
    case ESP_A2D_PROF_STATE_EVT:
        if (p->a2d_prof_stat.init_state == ESP_A2D_INIT_SUCCESS) post((bt_event_t){.kind=EV_READY});
        else ESP_LOGE(TAG, "A2DP_PROFILE state=%d", p->a2d_prof_stat.init_state);
        break;
    case ESP_A2D_CONNECTION_STATE_EVT:
        post((bt_event_t){.kind=EV_CONNECTION, .value=p->conn_stat.state}); break;
    case ESP_A2D_AUDIO_STATE_EVT:
        post((bt_event_t){.kind=EV_AUDIO, .value=p->audio_stat.state}); break;
    case ESP_A2D_MEDIA_CTRL_ACK_EVT:
        post((bt_event_t){.kind=EV_ACK, .cmd=p->media_ctrl_stat.cmd, .value=p->media_ctrl_stat.status}); break;
    case ESP_A2D_AUDIO_CFG_EVT:
        ESP_LOGI(TAG, "CODEC type=%u frequency_mask=%u channel_mask=%u", p->audio_cfg.mcc.type,
                 p->audio_cfg.mcc.cie.sbc_info.samp_freq, p->audio_cfg.mcc.cie.sbc_info.ch_mode);
        break;
    case ESP_A2D_REPORT_SNK_DELAY_VALUE_EVT:
        ESP_LOGI(TAG, "SINK_DELAY tenths_ms=%u (receiver report, not measured end-to-end)",
                 p->a2d_report_delay_value_stat.delay_value); break;
    default: break;
    }
}

static void bluetooth_task(void *unused) {
    (void)unused;
    bool ready=false, discovering=false, found=false, connecting=false;
    uint8_t peer[6]={0};
    int64_t next=0, attempt_started=0;
    for (;;) {
        bt_event_t e;
        if (xQueueReceive(events, &e, pdMS_TO_TICKS(100)) == pdTRUE) {
            switch (e.kind) {
            case EV_READY: {
                /* IDF 5.5.2's internal SBC source already advertises 44.1 kHz.
                 * Custom SEP registration is handled ONLY with external codec
                 * mode; its API can return ESP_OK but dispatch an unhandled event
                 * in internal codec mode. Keep the matching default endpoint. */
                ready=true;
                ESP_LOGI(TAG, "A2DP_READY source PCM=44100Hz/16bit/stereo");
                break;
            }
            case EV_DISCOVERY:
                discovering=e.value==ESP_BT_GAP_DISCOVERY_STARTED;
                portal_set_state(discovering,connected,streaming);
                ESP_LOGI(TAG,"STATE_TRANSITION: discovery -> %s", discovering?"SCANNING":"STOPPED");
                if (!discovering) next=esp_timer_get_time()+(found?0:1000000);
                break;
            case EV_CONNECTION: {
                bool online=e.value==ESP_A2D_CONNECTION_STATE_CONNECTED;
                connecting=e.value==ESP_A2D_CONNECTION_STATE_CONNECTING || e.value==ESP_A2D_CONNECTION_STATE_DISCONNECTING;
                if(e.value==ESP_A2D_CONNECTION_STATE_CONNECTED ||
                   e.value==ESP_A2D_CONNECTION_STATE_DISCONNECTED) found=false;
                portENTER_CRITICAL(&lock);
                connected=online; link_busy=connecting;
                if (!online) { streaming=false; ring_read=ring_write=ring_used=0; desired.running=false; desired.throttle=0; desired.rpm=0; }
                portEXIT_CRITICAL(&lock);
                portal_set_state(discovering,online,false);
                ESP_LOGI(TAG,"STATE_TRANSITION: A2DP connection=%d",e.value);
                next=esp_timer_get_time()+(online?500000:5000000);
                break;
            }
            case EV_AUDIO:
                portENTER_CRITICAL(&lock); streaming=e.value==ESP_A2D_AUDIO_STATE_STARTED; portEXIT_CRITICAL(&lock);
                portal_set_state(discovering,connected,streaming);
                ESP_LOGI(TAG,"STATE_TRANSITION: AUDIO state=%d",e.value); break;
            case EV_ACK:
                ESP_LOGI(TAG,"MEDIA_ACK cmd=%d status=%d",e.cmd,e.value);
                if (e.cmd==ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY && e.value==ESP_A2D_MEDIA_CTRL_ACK_SUCCESS)
                    checked("media_start",esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START));
                break;
            case EV_SELECT:
                if (!connected && !connecting) {
                    memcpy(peer,e.bda,6); found=true;
                    ESP_LOGI(TAG,"USER_SELECT: manual Bluetooth target accepted");
                    if(discovering) checked("cancel_discovery",esp_bt_gap_cancel_discovery());
                    else next=esp_timer_get_time();
                }
                break;
            default: break;
            }
        }
        int64_t now=esp_timer_get_time();
        bool online, active;
        portENTER_CRITICAL(&lock); online=connected; active=streaming; portEXIT_CRITICAL(&lock);
        if (connecting && now-attempt_started>30000000) {
            ESP_LOGW(TAG,"CONNECT_TIMEOUT: requesting disconnect before retry");
            checked("connect_timeout",esp_a2d_source_disconnect(peer));
            attempt_started=now;
        }
        if (!ready || now<next || discovering || connecting) continue;
        if (online) {
            if (!active) checked("media_check",esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY));
            next=now+2000000;
        } else if (found) {
            ESP_LOGI(TAG,"STATE_TRANSITION: A2DP -> CONNECTING");
            portENTER_CRITICAL(&lock); link_busy=true; portEXIT_CRITICAL(&lock);
            connecting=checked("connect",esp_a2d_source_connect(peer));
            if(!connecting) { portENTER_CRITICAL(&lock); link_busy=false; portEXIT_CRITICAL(&lock); }
            attempt_started=now; next=now+5000000;
        } else if (portal_scan_ready()) {
            discovering=checked("discovery",esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY,2,0));
            next=now+5000000;
        } else {
            /* Let Wi-Fi association finish before Bluetooth inquiry uses the shared radio. */
            next=now+500000;
        }
    }
}

static void synth_task(void *unused) {
    (void)unused; ev_init(&engine);
    ev_phase_t prior=EV_PHASE_OFF;
    int64_t offline_due=0;
    for (;;) {
        ev_control_t c; bool online; size_t buffered;
        portENTER_CRITICAL(&lock); c=desired; online=connected; buffered=ring_used; portEXIT_CRITICAL(&lock);
        if ((online && buffered>=QUEUE_TARGET) || (!online && esp_timer_get_time()<offline_due)) {
            vTaskDelay(pdMS_TO_TICKS(1)); continue;
        }
        int64_t before=esp_timer_get_time();
        ev_set_control(&engine,&c); ev_render(&engine,mono,EV_BLOCK);
        uint32_t elapsed=esp_timer_get_time()-before;
        int peak=0;
        for (unsigned i=0;i<EV_BLOCK;i++) { int a=abs(mono[i]); if(a>peak) peak=a; }
        portENTER_CRITICAL(&lock);
        if (online && connected && RING_BYTES-ring_used>=EV_BLOCK*4) {
            for (unsigned i=0;i<EV_BLOCK;i++) {
                uint16_t sample=(uint16_t)mono[i];
                pcm_ring[ring_write]=sample; pcm_ring[ring_write+1]=sample>>8;
                pcm_ring[ring_write+2]=sample; pcm_ring[ring_write+3]=sample>>8;
                ring_write=(ring_write+4)%RING_BYTES;
            }
            ring_used+=EV_BLOCK*4;
        } else if (!connected) ring_read=ring_write=ring_used=0;
        generated+=EV_BLOCK; if(peak) ++nonzero_blocks;
        rpm_now=(int)engine.rpm; peak_now=peak; phase_now=engine.phase;
        if(elapsed>render_max_us) render_max_us=elapsed;
        portEXIT_CRITICAL(&lock);
        if (engine.phase!=prior) {
            ESP_LOGI(TAG,"STATE_TRANSITION: ENGINE %s -> %s",ev_phase_name(prior),ev_phase_name(engine.phase));
            prior=engine.phase;
        }
        offline_due=before+(int64_t)EV_BLOCK*1000000/EV_RATE;
        /* Yield also while filling: never starve the core's idle task. */
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

static void heartbeat_task(void *unused) {
    (void)unused;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        uint32_t gen,nz,cb,bytes,under,max_us,err,drop,cb_hwm; int rpm,peak; bool online,active,pressed,tasks;
        size_t queued; ev_control_t c; ev_phase_t phase;
        portENTER_CRITICAL(&lock);
        gen=generated; nz=nonzero_blocks; cb=pcm_callbacks; bytes=pcm_bytes; under=underrun_bytes;
        max_us=render_max_us; err=errors; drop=dropped_events; cb_hwm=callback_hwm;
        rpm=rpm_now; peak=peak_now; online=connected; active=streaming; pressed=boot_pressed;
        queued=ring_used; c=desired; phase=phase_now; tasks=want_tasks; want_tasks=false;
        portEXIT_CRITICAL(&lock);
        ESP_LOGI(TAG,"HEARTBEAT ms=%" PRId64 " connected=%d streaming=%d boot=%d running=%d throttle=%d rpm=%d phase=%s frames=%" PRIu32 " nonzero=%" PRIu32 " peak=%d",
                 esp_timer_get_time()/1000,online,active,pressed,c.running,(int)(c.throttle*100),rpm,ev_phase_name(phase),gen,nz,peak);
        ESP_LOGI(TAG,"AUDIO_STATS callbacks=%" PRIu32 " bytes=%" PRIu32 " underrun_bytes=%" PRIu32 " queued=%u render_max_us=%" PRIu32 " errors=%" PRIu32 " dropped=%" PRIu32 " heap=%" PRIu32,
                 cb,bytes,under,(unsigned)queued,max_us,err,drop,esp_get_free_heap_size());
        ESP_LOGI(TAG,"STACK synth=%u manager=%u console=%u heartbeat=%u callback=%" PRIu32 " bytes",
                 (unsigned)uxTaskGetStackHighWaterMark(synth_handle),(unsigned)uxTaskGetStackHighWaterMark(bt_handle),
                 (unsigned)uxTaskGetStackHighWaterMark(console_handle),(unsigned)uxTaskGetStackHighWaterMark(NULL),cb_hwm);
        if(tasks) {
            unsigned n=uxTaskGetSystemState(task_status,32,NULL);
            for(unsigned i=0;i<n;i++) ESP_LOGI(TAG,"TASK name=%s hwm=%u",task_status[i].pcTaskName,(unsigned)task_status[i].usStackHighWaterMark);
        }
    }
}

static void command(char *line, int64_t *idle_deadline) {
    if(!strcmp(line,"tasks") || !strcmp(line,"status")) {
        portENTER_CRITICAL(&lock); want_tasks=true; portEXIT_CRITICAL(&lock); return;
    }
    if(!strncmp(line,"ap_pin",6)) {
        const char *pin=line+6;
        if(*pin++!=' ' || strlen(pin)!=8) {
            ESP_LOGW(TAG,"CMD ap_pin rejected: expected eight digits"); return;
        }
        for(int i=0;i<8;i++) if(pin[i]<'0' || pin[i]>'9') {
            ESP_LOGW(TAG,"CMD ap_pin rejected: expected eight digits"); return;
        }
        nvs_handle_t nvs;
        if(!checked("ap_pin_nvs_open",nvs_open("a2dp_demo",NVS_READWRITE,&nvs))) return;
        bool ok=checked("ap_pin_nvs_set",nvs_set_str(nvs,"ap_pin",pin)) &&
                checked("ap_pin_nvs_commit",nvs_commit(nvs));
        nvs_close(nvs);
        if(ok) {
            ESP_LOGI(TAG,"CMD ap_pin saved; console_stack_min=%u bytes; restarting",
                     (unsigned)uxTaskGetStackHighWaterMark(NULL));
            vTaskDelay(pdMS_TO_TICKS(100));
            esp_restart();
        }
        return;
    }
    ev_control_t c;
    portENTER_CRITICAL(&lock); c=desired; portEXIT_CRITICAL(&lock);
    int rc=ev_command(&c,line);
    /* Never expose the shared bootloader command on this prototype. */
    if(rc!=0 && rc!=1 && rc!=2) { ESP_LOGW(TAG,"CMD rejected: %s",line); return; }
    if(rc==0) {
        portENTER_CRITICAL(&lock); desired=c; portEXIT_CRITICAL(&lock);
        *idle_deadline=c.running && c.throttle==0 ? esp_timer_get_time()+3000000 : 0;
    }
    ESP_LOGI(TAG,"CMD %s result=OK",line);
}

static void create_task(TaskFunction_t fn,const char *name,unsigned stack,unsigned priority,TaskHandle_t *handle,int core) {
    if(xTaskCreatePinnedToCore(fn,name,stack,NULL,priority,handle,core)!=pdPASS) {
        ESP_LOGE(TAG,"TASK_START_FAILED name=%s stack=%u",name,stack); abort();
    }
}

void app_main(void) {
    console_handle=xTaskGetCurrentTaskHandle();
    ESP_LOGI(TAG,"BOOT version=%s idf=%s reset_reason=%d sample_rate=%d boot_gpio=0 active_low=1",
             esp_app_get_description()->version,esp_get_idf_version(),esp_reset_reason(),EV_RATE);
    /* Preserve pre-existing NVS on failure; do not silently erase user data. */
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_LOGI(TAG,"PAIRING_MODE=manual; USB power; hold BOOT=throttle, release=idle then stop after 3s");
    gpio_config_t gpio={.pin_bit_mask=1ULL<<GPIO_NUM_0,.mode=GPIO_MODE_INPUT,.pull_up_en=GPIO_PULLUP_ENABLE};
    ESP_ERROR_CHECK(gpio_config(&gpio));
    uart_config_t uart={.baud_rate=115200,.data_bits=UART_DATA_8_BITS,.parity=UART_PARITY_DISABLE,
        .stop_bits=UART_STOP_BITS_1,.flow_ctrl=UART_HW_FLOWCTRL_DISABLE,.source_clk=UART_SCLK_DEFAULT};
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_0,&uart));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0,512,0,0,NULL,0));
    events=xQueueCreate(24,sizeof(bt_event_t)); configASSERT(events);
    create_task(synth_task,"engine_synth",6144,5,&synth_handle,1);
    create_task(bluetooth_task,"bt_manager",6144,4,&bt_handle,0);
    create_task(heartbeat_task,"heartbeat",4096,2,&heartbeat_handle,1);
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));
    esp_bt_controller_config_t cfg=BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT));
    ESP_ERROR_CHECK(esp_bluedroid_init()); ESP_ERROR_CHECK(esp_bluedroid_enable());
    ESP_ERROR_CHECK(esp_bt_gap_register_callback(gap_callback));
    ESP_ERROR_CHECK(esp_bt_gap_set_device_name("EV Engine Prototype"));
    esp_bt_io_cap_t iocap=ESP_BT_IO_CAP_NONE;
    ESP_ERROR_CHECK(esp_bt_gap_set_security_param(ESP_BT_SP_IOCAP_MODE,&iocap,sizeof(iocap)));
    esp_bt_pin_code_t pin={0};
    ESP_ERROR_CHECK(esp_bt_gap_set_pin(ESP_BT_PIN_TYPE_VARIABLE,0,pin));
    ESP_ERROR_CHECK(esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE,ESP_BT_NON_DISCOVERABLE));
    ESP_ERROR_CHECK(esp_a2d_register_callback(a2dp_callback));
    ESP_ERROR_CHECK(esp_a2d_source_register_data_callback(audio_data));
    ESP_ERROR_CHECK(esp_a2d_source_init());
    checked("portal_start",portal_start(portal_connect_selected));
    char line[160]; size_t used=0; bool overflow=false;
    boot_button_t button={0}; int64_t idle_deadline=0;
    for(;;) {
        int64_t now=esp_timer_get_time();
        int edge=boot_button_update(&button,gpio_get_level(GPIO_NUM_0)==0,now/1000);
        if(edge) {
            portENTER_CRITICAL(&lock);
            boot_pressed=button.pressed; desired.throttle=edge>0?1:0; desired.rpm=0;
            if(edge>0) desired.running=true;
            portEXIT_CRITICAL(&lock);
            idle_deadline=edge<0?now+3000000:0;
            ESP_LOGI(TAG,"BOOT_BUTTON %s throttle=%d",edge>0?"PRESS":"RELEASE",edge>0?100:0);
        }
        if(idle_deadline && now>=idle_deadline) {
            portENTER_CRITICAL(&lock); desired.running=false; desired.throttle=0; desired.rpm=0; portEXIT_CRITICAL(&lock);
            idle_deadline=0; ESP_LOGI(TAG,"AUTO_OFF idle_ms=3000");
        }
        /* Bounded read: console bursts never starve physical input. */
        uint8_t bytes[64]; int count=uart_read_bytes(UART_NUM_0,bytes,sizeof(bytes),0);
        for(int i=0;i<count;i++) {
            char ch=bytes[i];
            if(ch=='\r' || ch=='\n') {
                if(overflow) ESP_LOGW(TAG,"CMD rejected: line too long");
                else if(used) { line[used]=0; command(line,&idle_deadline); }
                used=0; overflow=false;
            } else if(ch>=32 && ch<127) {
                if(used<sizeof(line)-1) line[used++]=ch; else overflow=true;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
