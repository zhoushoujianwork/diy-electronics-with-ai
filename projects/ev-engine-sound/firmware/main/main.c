#include "engine_voice.h"
#include "board_audio.h"
#include "board_config.h"
#include "board_ui.h"
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tinyusb.h"
#include "tusb_cdc_acm.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/soc.h"

static const char *TAG="engine_voice";
static portMUX_TYPE lock=portMUX_INITIALIZER_UNLOCKED;
static ev_control_t desired={.volume=.60f};
static int64_t last_control_us;
static bool control_watchdog;
static bool ui_rev_active;
static bool fault;
static ev_engine_t engine;
static int16_t mono[EV_BLOCK];
#if EV_AUDIO_I2S_SLOT_BITS == 16
static int16_t stereo[EV_BLOCK*2];
#elif EV_AUDIO_I2S_SLOT_BITS == 32
static int32_t stereo[EV_BLOCK*2];
#else
#error "EV_AUDIO_I2S_SLOT_BITS must be 16 or 32"
#endif
static TaskHandle_t audio_handle, heartbeat_handle, console_handle;
static i2s_chan_handle_t tx;
static vprintf_like_t prior_vprintf;
typedef struct {
    float rpm;
    uint64_t frames, firings;
    uint32_t write_errors, max_render_us;
    unsigned audio_hwm;
    unsigned gear;
    bool running;
    ev_phase_t phase;
    float load;
    unsigned last_cylinder;
} diagnostics_t;
static diagnostics_t diagnostics;

/* ESP-IDF task sizes and high-water marks are BYTES.
 * Audio 6144: render/write path + error log/TinyUSB tee; 2560-byte PCM is static.
 * Heartbeat 4096: snapshot <64 B + task status + 256-byte TinyUSB log formatter.
 * app_main 10240: console parser plus the deepest panel/LVGL/touch init path.
 * taskLVGL 10240: mechanical draw descriptors, state snapshot, touch/scroll callbacks.
 * Conservative allocations, NOT verified margins until measured on hardware.
 */
static int cdc_tee_vprintf(const char *fmt, va_list args) {
    char buf[256];
    va_list copy;
    va_copy(copy,args);
    int len=vsnprintf(buf,sizeof(buf),fmt,copy);
    va_end(copy);
    if(len>0) {
        if(len>=(int)sizeof(buf)) len=sizeof(buf)-1;
        size_t offset=0;
        while(offset<(size_t)len) {
            size_t queued=tinyusb_cdcacm_write_queue(TINYUSB_CDC_ACM_0,
                (const uint8_t *)buf+offset,(size_t)len-offset);
            if(queued==0) break;
            offset+=queued;
        }
        if(offset) tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0,0);
    }
    return prior_vprintf?prior_vprintf(fmt,args):len;
}

static esp_err_t console_init(void) {
    const tinyusb_config_t usb={
        .device_descriptor=NULL,.string_descriptor=NULL,.external_phy=false,
        .configuration_descriptor=NULL,
    };
    esp_err_t err=tinyusb_driver_install(&usb);
    if(err!=ESP_OK) return err;
    const tinyusb_config_cdcacm_t cdc={
        .usb_dev=TINYUSB_USBDEV_0,.cdc_port=TINYUSB_CDC_ACM_0,
        .rx_unread_buf_sz=256,.callback_rx=NULL,.callback_rx_wanted_char=NULL,
        .callback_line_state_changed=NULL,.callback_line_coding_changed=NULL,
    };
    err=tusb_cdc_acm_init(&cdc);
    if(err==ESP_OK) prior_vprintf=esp_log_set_vprintf(cdc_tee_vprintf);
    return err;
}

static void amp_set(bool enabled) {
#ifdef CONFIG_EV_BOARD_CONFIRMED
    portENTER_CRITICAL(&lock);
    enabled=enabled && !fault;
    portEXIT_CRITICAL(&lock);
    esp_err_t err=board_audio_set_amp(enabled);
    if(err!=ESP_OK) ESP_LOGE(TAG,"amp_set enabled=%d err=%s",enabled,esp_err_to_name(err));
#else
    (void)enabled;
#endif
}

static esp_err_t audio_init(void) {
#ifdef CONFIG_EV_BOARD_CONFIRMED
    if (!GPIO_IS_VALID_OUTPUT_GPIO(EV_AUDIO_I2S_BCLK_GPIO) ||
        !GPIO_IS_VALID_OUTPUT_GPIO(EV_AUDIO_I2S_WS_GPIO) ||
        !GPIO_IS_VALID_OUTPUT_GPIO(EV_AUDIO_I2S_DOUT_GPIO) ||
        EV_AUDIO_I2S_BCLK_GPIO==EV_AUDIO_I2S_WS_GPIO ||
        EV_AUDIO_I2S_BCLK_GPIO==EV_AUDIO_I2S_DOUT_GPIO ||
        EV_AUDIO_I2S_WS_GPIO==EV_AUDIO_I2S_DOUT_GPIO)
        return ESP_ERR_INVALID_ARG;
    ESP_RETURN_ON_ERROR(board_audio_prepare(),TAG,"board audio prepare");
    i2s_chan_config_t chan=I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0,I2S_ROLE_MASTER);
    chan.dma_desc_num=6;
    chan.dma_frame_num=EV_BLOCK;
    chan.auto_clear=true;
    esp_err_t rc=i2s_new_channel(&chan,&tx,NULL);
    if(rc!=ESP_OK) return rc;
    i2s_std_clk_config_t clock=I2S_STD_CLK_DEFAULT_CONFIG(EV_RATE);
    clock.mclk_multiple=I2S_MCLK_MULTIPLE_256;
#if EV_AUDIO_I2S_SLOT_BITS == 16
    i2s_std_slot_config_t slots=I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
        I2S_DATA_BIT_WIDTH_16BIT,I2S_SLOT_MODE_STEREO);
#else
    i2s_std_slot_config_t slots=I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
        I2S_DATA_BIT_WIDTH_32BIT,I2S_SLOT_MODE_STEREO);
#endif
    i2s_std_config_t cfg={
        .clk_cfg=clock,
        .slot_cfg=slots,
        .gpio_cfg={.mclk=EV_AUDIO_I2S_MCLK_GPIO,.bclk=EV_AUDIO_I2S_BCLK_GPIO,
                   .ws=EV_AUDIO_I2S_WS_GPIO,
                   .dout=EV_AUDIO_I2S_DOUT_GPIO,.din=I2S_GPIO_UNUSED}
    };
    rc=i2s_channel_init_std_mode(tx,&cfg);
    if(rc==ESP_OK) rc=i2s_channel_enable(tx);
    if(rc==ESP_OK) rc=board_audio_start();
    return rc;
#else
    ESP_LOGW(TAG,"BOARD_UNCONFIRMED: DSP/console only, audio GPIOs untouched");
    return ESP_OK;
#endif
}

static void audio_task(void *unused) {
    (void)unused;
    ev_init(&engine);
    uint32_t errors=0,max_render_us=0;
    bool previous_running=false;
    unsigned previous_gear=0;
    ev_phase_t previous_phase=EV_PHASE_OFF;
    for(;;) {
        int64_t now=esp_timer_get_time();
        bool expired=false;
        portENTER_CRITICAL(&lock);
        if(control_watchdog && desired.running &&
           now-last_control_us>(int64_t)CONFIG_EV_TIMEOUT_MS*1000) {
            desired.running=false; desired.throttle=0; desired.rpm=0; expired=true;
        }
        ev_control_t control=desired;
        bool failed=fault;
        portEXIT_CRITICAL(&lock);
        if(failed) control.running=false;
        if(expired) ESP_LOGW(TAG,"STATE_TRANSITION: RUN -> STOP reason=control_timeout");
        if(control.running!=previous_running) {
            ESP_LOGI(TAG,"STATE_TRANSITION: %s -> %s",previous_running?"RUN":"STOP",control.running?"RUN":"STOP");
            previous_running=control.running;
        }
        if(control.gear!=previous_gear) {
            ESP_LOGI(TAG,"STATE_TRANSITION: GEAR %u -> %u shift_cut_ms=120",
                     previous_gear,control.gear);
            previous_gear=control.gear;
        }
        ev_set_control(&engine,&control);
        int64_t begin=esp_timer_get_time();
        ev_render(&engine,mono,EV_BLOCK);
        uint32_t elapsed=(uint32_t)(esp_timer_get_time()-begin);
        if(engine.phase!=previous_phase) {
            ESP_LOGI(TAG,"STATE_TRANSITION: AUDIO %s -> %s",ev_phase_name(previous_phase),ev_phase_name(engine.phase));
            previous_phase=engine.phase;
        }
        if(elapsed>max_render_us) max_render_us=elapsed;
        if(tx && !failed) {
            for(unsigned i=0;i<EV_BLOCK;i++) {
#if EV_AUDIO_I2S_SLOT_BITS == 16
                stereo[2*i]=stereo[2*i+1]=mono[i];
#else
                /* Multiplication avoids undefined left shift of negative PCM. */
                stereo[2*i]=stereo[2*i+1]=(int32_t)mono[i]*65536;
#endif
            }
            size_t offset=0;
            esp_err_t err=ESP_OK;
            while(offset<sizeof(stereo)) {
                size_t written=0;
                /* New channel API timeout is milliseconds, NOT RTOS ticks. */
                err=i2s_channel_write(tx,((uint8_t *)stereo)+offset,sizeof(stereo)-offset,&written,100);
                offset+=written;
                if(err!=ESP_OK || written==0) break;
            }
            if(err!=ESP_OK || offset!=sizeof(stereo)) {
                amp_set(false);
                ++errors;
                portENTER_CRITICAL(&lock); fault=true; desired.running=false; portEXIT_CRITICAL(&lock);
                ESP_LOGE(TAG,"AUDIO_FAULT err=%s bytes=%u/%u (latched; reboot required)",
                         esp_err_to_name(err),(unsigned)offset,(unsigned)sizeof(stereo));
            } else {
                amp_set(engine.gain>.0001f);
            }
        } else {
            amp_set(false);
            vTaskDelay(pdMS_TO_TICKS(8));
        }
        unsigned hwm=uxTaskGetStackHighWaterMark(NULL);
        portENTER_CRITICAL(&lock);
        diagnostics=(diagnostics_t){.rpm=engine.rpm,.frames=engine.frames,.firings=engine.firings,
            .write_errors=errors,.max_render_us=max_render_us,.audio_hwm=hwm,
            .gear=control.gear,.running=control.running,.phase=engine.phase,
            .load=engine.load,.last_cylinder=engine.last_cylinder};
        portEXIT_CRITICAL(&lock);
    }
}

static void heartbeat_task(void *unused) {
    (void)unused;
    uint64_t last_frames=0;
    for(;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        portENTER_CRITICAL(&lock);
        diagnostics_t d=diagnostics;
        if(d.frames==last_frames) { fault=true; desired.running=false; }
        bool failed=fault;
        portEXIT_CRITICAL(&lock);
        if(failed) amp_set(false);
        last_frames=d.frames;
        unsigned lvgl_hwm=board_ui_stack_high_water_mark();
        if(d.audio_hwm<1024 || uxTaskGetStackHighWaterMark(console_handle)<1024 ||
           uxTaskGetStackHighWaterMark(NULL)<1024 || (lvgl_hwm && lvgl_hwm<1024))
            ESP_LOGW(TAG,"STACK_MARGIN_LOW minimum_required_bytes=1024");
        ESP_LOGI(TAG,"HEARTBEAT gear=%u rpm=%.0f frames=%"PRIu64" firings=%"PRIu64
                 " write_errors=%"PRIu32" render_max_us=%"PRIu32" block_budget_us=%u"
                 " stack_audio=%u stack_console=%u stack_heartbeat=%u stack_lvgl=%u"
                 " heap=%"PRIu32" fault=%d",
                 d.gear,d.rpm,d.frames,d.firings,d.write_errors,d.max_render_us,
                 (unsigned)(1000000U*EV_BLOCK/EV_RATE),d.audio_hwm,
                 (unsigned)uxTaskGetStackHighWaterMark(console_handle),
                 (unsigned)uxTaskGetStackHighWaterMark(NULL),lvgl_hwm,
                 esp_get_free_heap_size(),failed);
    }
}

#if EV_UI_ENABLE
static bool ui_state_snapshot(board_ui_state_t *state,void *context) {
    (void)context;
    if(!state) return false;
    portENTER_CRITICAL(&lock);
    ev_control_t control=desired;
    diagnostics_t d=diagnostics;
    bool failed=fault;
    portEXIT_CRITICAL(&lock);
    unsigned profile=control.profile<EV_PROFILES?control.profile:0;
    *state=(board_ui_state_t){
        .profile=profile,
        .exhaust=control.exhaust<EV_EXHAUSTS?control.exhaust:0,
        .redline_rpm=(unsigned)ev_redline(&control),
        .rpm=d.rpm,
        .throttle=control.throttle,
        .volume=control.volume,
        .gear=control.gear,
        .running=d.running,
        .fault=failed,
        .firings=d.firings,
        .phase=d.phase,.load=d.load,.last_cylinder=d.last_cylinder,
    };
    return true;
}

static void ui_action(board_ui_action_t action,int value,void *context) {
    (void)context;
    bool accepted=true;
    bool takes_control=true;
    portENTER_CRITICAL(&lock);
    switch(action) {
        case BOARD_UI_PROFILE:
            if(value<0 || value>=EV_PROFILES) accepted=false;
            else {
                desired.profile=(unsigned)value;
                desired.rpm=0;
                desired.redline_rpm=0;
            }
            break;
        case BOARD_UI_EXHAUST:
            if(value<0 || value>=EV_EXHAUSTS) accepted=false;
            else desired.exhaust=(unsigned)value;
            takes_control=false;
            break;
        case BOARD_UI_ENGINE_TOGGLE:
            desired.running=!desired.running;
            if(!desired.running) { desired.throttle=0; desired.rpm=0; ui_rev_active=false; }
            break;
        case BOARD_UI_ENGINE_STOP:
            desired.running=false;
            desired.throttle=0;
            desired.rpm=0;
            ui_rev_active=false;
            break;
        case BOARD_UI_VOLUME_DELTA: {
            float next=desired.volume+(float)value/100.0f;
            if(next<0) next=0;
            if(next>1) next=1;
            desired.volume=next;
            takes_control=false;
            break;
        }
        case BOARD_UI_REDLINE_DELTA: {
            unsigned profile=desired.profile<EV_PROFILES?desired.profile:0;
            int minimum=(int)ev_profiles[profile].idle_rpm+500;
            int next=(int)ev_redline(&desired)+value;
            if(next<minimum) next=minimum;
            if(next>EV_MAX_RPM) next=EV_MAX_RPM;
            desired.redline_rpm=(float)next;
            if(desired.rpm>desired.redline_rpm) desired.rpm=desired.redline_rpm;
            takes_control=false;
            break;
        }
        case BOARD_UI_REDLINE_SET: {
            unsigned profile=desired.profile<EV_PROFILES?desired.profile:0;
            int minimum=(int)ev_profiles[profile].idle_rpm+500;
            int next=value;
            if(next<minimum) next=minimum;
            if(next>EV_MAX_RPM) next=EV_MAX_RPM;
            desired.redline_rpm=(float)next;
            if(desired.rpm>desired.redline_rpm) desired.rpm=desired.redline_rpm;
            takes_control=false;
            break;
        }
        case BOARD_UI_REV_PRESS:
            ui_rev_active=true;
            desired.throttle=(float)value/100.0f;
            desired.rpm=0;
            desired.running=true;
            break;
        case BOARD_UI_REV_RELEASE:
            if(ui_rev_active) {
                desired.throttle=0;
                desired.rpm=0;
                ui_rev_active=false;
            }
            break;
        default:
            accepted=false;
            break;
    }
    if(accepted) {
        if(takes_control) control_watchdog=false;
        last_control_us=esp_timer_get_time();
    }
    ev_control_t after=desired;
    portEXIT_CRITICAL(&lock);
    unsigned profile=after.profile<EV_PROFILES?after.profile:0;
    ESP_LOGI(TAG,"UI_ACTION action=%d value=%d result=%s profile=%s exhaust=%s gear=%u running=%d throttle=%.0f volume=%.0f redline=%.0f",
             action,value,accepted?"OK":"INVALID_ARGUMENT",ev_profiles[profile].name,
             ev_exhausts[after.exhaust<EV_EXHAUSTS?after.exhaust:0].name,
             after.gear,after.running,after.throttle*100,after.volume*100,
             ev_redline(&after));
}
#endif

void app_main(void) {
    console_handle=xTaskGetCurrentTaskHandle();
    ESP_ERROR_CHECK(console_init());
    ESP_LOGI(TAG,"BOOT version=0.2.0 reset_reason=%d rate=%d",esp_reset_reason(),EV_RATE);
#ifdef CONFIG_EV_BOARD_CONFIRMED
    ESP_LOGI(TAG,"BOARD name=%s i2s={mclk:%d,bclk:%d,ws:%d,dout:%d,slot:%d} amp_gpio=%d",
             EV_BOARD_NAME,EV_AUDIO_I2S_MCLK_GPIO,
             EV_AUDIO_I2S_BCLK_GPIO,EV_AUDIO_I2S_WS_GPIO,
             EV_AUDIO_I2S_DOUT_GPIO,EV_AUDIO_I2S_SLOT_BITS,EV_AMP_ENABLE_GPIO);
#endif
    esp_err_t err=audio_init();
    if(err!=ESP_OK) {
        amp_set(false); fault=true;
        ESP_LOGE(TAG,"audio_init err=%s; console retained",esp_err_to_name(err));
    }
#if EV_UI_ENABLE
    if(err==ESP_OK) {
        esp_err_t ui_err=board_ui_init(board_audio_i2c_bus(),ui_action,ui_state_snapshot,NULL);
        if(ui_err!=ESP_OK)
            ESP_LOGE(TAG,"ui_init err=%s; audio/console retained",esp_err_to_name(ui_err));
    }
#endif
    if(xTaskCreatePinnedToCore(audio_task,"voice_audio",6144,NULL,5,&audio_handle,1)!=pdPASS) {
        amp_set(false); ESP_LOGE(TAG,"task_start voice_audio failed err=ENOMEM"); return;
    }
    if(xTaskCreatePinnedToCore(heartbeat_task,"voice_heartbeat",4096,NULL,2,&heartbeat_handle,0)!=pdPASS) {
        portENTER_CRITICAL(&lock); fault=true; portEXIT_CRITICAL(&lock);
        amp_set(false); ESP_LOGE(TAG,"task_start voice_heartbeat failed err=ENOMEM");
    }
    ESP_LOGI(TAG,"READY profiles=single,twin180,twin270,twin360,vtwin,vtwin90,triple120,triple270,inline4,crossplane4,inline5,inline6,v6_60,flat6,flatplane8,crossplane8,v10,v12");
    ESP_LOGI(TAG,"READY exhausts=stock,akrapovic,yoshimura,tin_can,straight");
    ESP_LOGI(TAG,"READY commands=profile;exhaust;gear N|0..6;redline default|N(max %d);start;stop;throttle 0..100;rpm 0..redline;volume 0..100;ping;status;bootloader",
             EV_MAX_RPM);
    char line[96]; size_t used=0; bool overflow=false;
    for(;;) {
        uint8_t input[32];
        size_t n=0;
        if(tinyusb_cdcacm_read(TINYUSB_CDC_ACM_0,input,sizeof(input),&n)!=ESP_OK) n=0;
        for(size_t i=0;i<n;i++) {
            char ch=(char)input[i];
            if(ch=='\r' || ch=='\n') {
                if(overflow) ESP_LOGW(TAG,"COMMAND rejected reason=line_too_long");
                else if(used) {
                    line[used]=0;
                    portENTER_CRITICAL(&lock); ev_control_t c=desired; portEXIT_CRITICAL(&lock);
                    int result=ev_command(&c,line);
                    if(result==0 || result==1) {
                        portENTER_CRITICAL(&lock);
                        if(result==0) { desired=c; ui_rev_active=false; }
                        control_watchdog=true;
                        last_control_us=esp_timer_get_time();
                        portEXIT_CRITICAL(&lock);
                    }
                    if(result==3) {
                        ESP_LOGI(TAG,"COMMAND bootloader result=OK; disconnecting USB");
                        amp_set(false);
                        vTaskDelay(pdMS_TO_TICKS(150));
                        tud_disconnect();
                        vTaskDelay(pdMS_TO_TICKS(300));
                        REG_WRITE(RTC_CNTL_USB_CONF_REG,0);
                        REG_WRITE(RTC_CNTL_OPTION1_REG,RTC_CNTL_FORCE_DOWNLOAD_BOOT);
                        esp_rom_software_reset_system();
                    } else if(result==2) {
                        portENTER_CRITICAL(&lock); diagnostics_t d=diagnostics; bool f=fault; portEXIT_CRITICAL(&lock);
                        ESP_LOGI(TAG,"STATUS version=0.2.0 reset_reason=%d profile=%s exhaust=%s gear=%u running=%d rpm=%.0f throttle=%.0f volume=%.0f redline=%.0f fault=%d",
                                 esp_reset_reason(),
                                 ev_profiles[c.profile].name,ev_exhausts[c.exhaust].name,
                                 c.gear,d.running,d.rpm,c.throttle*100,
                                 c.volume*100,ev_redline(&c),f);
                        esp_err_t report_err=board_audio_report();
                        if(report_err!=ESP_OK)
                            ESP_LOGE(TAG,"audio_report err=%s",esp_err_to_name(report_err));
#if EV_UI_ENABLE
                        report_err=board_ui_report();
                        if(report_err!=ESP_OK)
                            ESP_LOGE(TAG,"ui_report err=%s",esp_err_to_name(report_err));
#endif
                    } else if(result==0) ESP_LOGI(TAG,"COMMAND %s result=OK",line);
                    else if(result<0) ESP_LOGW(TAG,"COMMAND %s result=INVALID_ARGUMENT",line);
                }
                used=0; overflow=false;
            } else if((unsigned char)ch<32 || (unsigned char)ch>126) {
                overflow=true;
            } else if(used<sizeof(line)-1 && !overflow) line[used++]=ch;
            else overflow=true;
        }
        if(n==0) vTaskDelay(pdMS_TO_TICKS(10));
    }
}
