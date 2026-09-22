#include "binding_announcement.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "board.h"
#include "driver/i2s_std.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define VOICE_SAMPLE_RATE 16000
#define VOICE_TASK_STACK 4096
#define VOICE_FRAMES_PER_WRITE 256

typedef struct {
    const uint8_t *start;
    const uint8_t *end;
} voice_clip_t;

typedef struct {
    char code[7];
} announcement_job_t;

#define DECLARE_CLIP(name)                                      \
    extern const uint8_t name##_start[] asm("_binary_" #name "_pcm_start"); \
    extern const uint8_t name##_end[] asm("_binary_" #name "_pcm_end")

DECLARE_CLIP(binding_code);
DECLARE_CLIP(digit_0);
DECLARE_CLIP(digit_1);
DECLARE_CLIP(digit_2);
DECLARE_CLIP(digit_3);
DECLARE_CLIP(digit_4);
DECLARE_CLIP(digit_5);
DECLARE_CLIP(digit_6);
DECLARE_CLIP(digit_7);
DECLARE_CLIP(digit_8);
DECLARE_CLIP(digit_9);

static const char *TAG = "binding_voice";
static i2s_chan_handle_t tx_channel;
static QueueHandle_t announcement_queue;
static TaskHandle_t announcement_task_handle;
static int16_t stereo_buffer[VOICE_FRAMES_PER_WRITE * 2];

static const voice_clip_t prefix_clip = {binding_code_start, binding_code_end};
static const voice_clip_t digit_clips[] = {
    {digit_0_start, digit_0_end}, {digit_1_start, digit_1_end},
    {digit_2_start, digit_2_end}, {digit_3_start, digit_3_end},
    {digit_4_start, digit_4_end}, {digit_5_start, digit_5_end},
    {digit_6_start, digit_6_end}, {digit_7_start, digit_7_end},
    {digit_8_start, digit_8_end}, {digit_9_start, digit_9_end},
};

static esp_err_t write_stereo_frames(size_t frames)
{
    size_t total = frames * 2 * sizeof(stereo_buffer[0]);
    size_t offset = 0;
    while (offset < total) {
        size_t written = 0;
        esp_err_t err = i2s_channel_write(tx_channel, (uint8_t *)stereo_buffer + offset,
                                          total - offset, &written, 1000);
        if (err != ESP_OK) return err;
        if (written == 0) return ESP_ERR_TIMEOUT;
        offset += written;
    }
    return ESP_OK;
}

static esp_err_t write_clip(voice_clip_t clip)
{
    size_t bytes = (size_t)(clip.end - clip.start);
    ESP_RETURN_ON_FALSE(bytes > 0 && bytes % sizeof(int16_t) == 0,
                        ESP_ERR_INVALID_SIZE, TAG, "invalid clip bytes=%u", (unsigned)bytes);
    const int16_t *mono = (const int16_t *)clip.start;
    size_t samples = bytes / sizeof(*mono);
    while (samples > 0) {
        size_t frames = samples > VOICE_FRAMES_PER_WRITE ? VOICE_FRAMES_PER_WRITE : samples;
        for (size_t i = 0; i < frames; ++i) {
            stereo_buffer[i * 2] = mono[i];
            stereo_buffer[i * 2 + 1] = mono[i];
        }
        ESP_RETURN_ON_ERROR(write_stereo_frames(frames), TAG, "write voice clip");
        mono += frames;
        samples -= frames;
    }
    return ESP_OK;
}

static esp_err_t write_silence(unsigned duration_ms)
{
    memset(stereo_buffer, 0, sizeof(stereo_buffer));
    size_t frames_remaining = (size_t)VOICE_SAMPLE_RATE * duration_ms / 1000;
    while (frames_remaining > 0) {
        size_t frames = frames_remaining > VOICE_FRAMES_PER_WRITE ?
                        VOICE_FRAMES_PER_WRITE : frames_remaining;
        ESP_RETURN_ON_ERROR(write_stereo_frames(frames), TAG, "write silence");
        frames_remaining -= frames;
    }
    return ESP_OK;
}

static esp_err_t play_code(const char code[7])
{
    ESP_RETURN_ON_ERROR(board_audio_set_amp(false), TAG, "mute before announcement");
    ESP_RETURN_ON_ERROR(write_silence(50), TAG, "prime audio");
    ESP_RETURN_ON_ERROR(board_audio_set_amp(true), TAG, "enable amplifier");
    ESP_RETURN_ON_ERROR(write_clip(prefix_clip), TAG, "play binding prefix");
    ESP_RETURN_ON_ERROR(write_silence(140), TAG, "prefix pause");
    for (size_t i = 0; i < 6; ++i) {
        unsigned digit = (unsigned)(code[i] - '0');
        ESP_RETURN_ON_ERROR(write_clip(digit_clips[digit]), TAG, "play digit");
        ESP_RETURN_ON_ERROR(write_silence(130), TAG, "digit pause");
    }
    ESP_RETURN_ON_ERROR(write_silence(120), TAG, "trailing silence");
    vTaskDelay(pdMS_TO_TICKS(150));
    return board_audio_set_amp(false);
}

static void announcement_task(void *unused)
{
    (void)unused;
    announcement_job_t job;
    char last_code[sizeof(job.code)] = {0};
    for (;;) {
        if (xQueueReceive(announcement_queue, &job, portMAX_DELAY) != pdTRUE) continue;
        if (strcmp(job.code, last_code) == 0) continue;
        memcpy(last_code, job.code, sizeof(last_code));
        ESP_LOGI(TAG, "BINDING_VOICE_START digits=6");
        esp_err_t err = play_code(job.code);
        if (err != ESP_OK) {
            (void)board_audio_set_amp(false);
            ESP_LOGE(TAG, "BINDING_VOICE_FAILED err=%s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "BINDING_VOICE_DONE stack_free=%u",
                     (unsigned)uxTaskGetStackHighWaterMark(NULL));
        }
    }
}

esp_err_t binding_announcement_init(void)
{
    ESP_RETURN_ON_ERROR(board_audio_prepare(), TAG, "prepare board audio");
    i2s_chan_config_t channel_config = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    channel_config.dma_desc_num = 6;
    channel_config.dma_frame_num = VOICE_FRAMES_PER_WRITE;
    channel_config.auto_clear = true;
    ESP_RETURN_ON_ERROR(i2s_new_channel(&channel_config, &tx_channel, NULL), TAG, "create I2S channel");

    i2s_std_clk_config_t clock_config = I2S_STD_CLK_DEFAULT_CONFIG(VOICE_SAMPLE_RATE);
    clock_config.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    i2s_std_config_t i2s_config = {
        .clk_cfg = clock_config,
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = STICKS3_AUDIO_MCLK_GPIO,
            .bclk = STICKS3_AUDIO_BCLK_GPIO,
            .ws = STICKS3_AUDIO_WS_GPIO,
            .dout = STICKS3_AUDIO_DOUT_GPIO,
            .din = I2S_GPIO_UNUSED,
        },
    };
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(tx_channel, &i2s_config), TAG, "configure I2S");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(tx_channel), TAG, "enable I2S");
    ESP_RETURN_ON_ERROR(board_audio_start(), TAG, "start codec");
    ESP_RETURN_ON_ERROR(board_audio_report(), TAG, "audio readback");

    announcement_queue = xQueueCreate(1, sizeof(announcement_job_t));
    ESP_RETURN_ON_FALSE(announcement_queue, ESP_ERR_NO_MEM, TAG, "create announcement queue");
    BaseType_t created = xTaskCreate(announcement_task, "binding_voice", VOICE_TASK_STACK,
                                     NULL, 3, &announcement_task_handle);
    ESP_RETURN_ON_FALSE(created == pdPASS, ESP_ERR_NO_MEM, TAG, "create announcement task");
    ESP_LOGI(TAG, "BINDING_VOICE_READY sample_rate=%d task_stack=%d",
             VOICE_SAMPLE_RATE, VOICE_TASK_STACK);
    return ESP_OK;
}

esp_err_t binding_announcement_enqueue(const char *code)
{
    ESP_RETURN_ON_FALSE(announcement_queue, ESP_ERR_INVALID_STATE, TAG, "voice not initialized");
    ESP_RETURN_ON_FALSE(code && strlen(code) == 6, ESP_ERR_INVALID_ARG, TAG, "invalid binding code");
    announcement_job_t job = {0};
    for (size_t i = 0; i < 6; ++i) {
        ESP_RETURN_ON_FALSE(code[i] >= '0' && code[i] <= '9',
                            ESP_ERR_INVALID_ARG, TAG, "binding code is not numeric");
        job.code[i] = code[i];
    }
    xQueueOverwrite(announcement_queue, &job);
    ESP_LOGI(TAG, "BINDING_VOICE_QUEUED digits=6");
    return ESP_OK;
}

unsigned binding_announcement_stack_high_water_mark(void)
{
    return announcement_task_handle ?
           (unsigned)uxTaskGetStackHighWaterMark(announcement_task_handle) : 0;
}
