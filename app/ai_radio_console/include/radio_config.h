#ifndef __RADIO_CONFIG_H
#define __RADIO_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

#define RADIO_APP_NAME          "ai_radio_console"
#define RADIO_APP_VERSION       "1.1.0"
#define RADIO_APP_STACKSIZE     16384
#define RADIO_APP_PRIORITY      100

#define AUDIO_SAMPLE_RATE       16000
#define AUDIO_CHANNELS          1
#define AUDIO_BITS_PER_SAMPLE   16
#define AUDIO_CAPTURE_FROM_I2S  1
#define AUDIO_FRAME_MS          20
#define AUDIO_FRAME_SIZE        (AUDIO_SAMPLE_RATE * AUDIO_CHANNELS * AUDIO_BITS_PER_SAMPLE / 8 * AUDIO_FRAME_MS / 1000)

#define CW_BPF_LOW_FREQ         500
#define CW_BPF_HIGH_FREQ        1000
#define CW_MIN_WPM              5
#define CW_MAX_WPM              40

#define MAYDAY_TRIGGER_CONF     0.75f
#define INTERFERENCE_DB_THRESH  -40
#define MALICIOUS_MIN_SECONDS   30

#define LOG_DB_PATH             "/data/radio/radio_log.db"
#define AUDIO_CACHE_PATH        "/data/radio/cache/"
#define SKILLS_PATH             "/data/agent/skills/"
#define RECORD_PATH             "/data/radio/records/"
#define CONFIG_PATH             "/data/radio/config.ini"

#define MAX_CALLSIGN_LEN        16
#define MAX_QTH_LEN             64
#define MAX_RAPPORT_LEN         8
#define MAX_FREQ_STR_LEN        16
#define MAX_LANG_LEN            8
#define MAX_API_KEY_LEN         128
#define MAX_MODEL_NAME_LEN      64
#define MAX_ENDPOINT_LEN        128
#define MAX_TRANSCRIPT_LEN      4096
#define MAX_LLM_RESPONSE_LEN    8192

#define UI_REFRESH_MS           100
#define AUDIO_ANALYSIS_INTERVAL 500
#define ASR_CHUNK_SECONDS       5
#define ASR_CHUNK_SAMPLES       (AUDIO_SAMPLE_RATE * ASR_CHUNK_SECONDS)

/* Streaming ASR parameters */
#define ASR_STREAM_TARGET_RATE  16000
#define ASR_STREAM_INPUT_RATE   8000
#define ASR_VAD_FRAME_MS        20
#define ASR_VAD_SPEECH_FRAMES   5    /* 100 ms of speech to trigger start */
#define ASR_VAD_SILENT_FRAMES   25   /* 500 ms of silence to trigger end */
#define ASR_VAD_THRESHOLD_DB    -40.0f
#define ASR_PARTIAL_INTERVAL_MS 1000 /* send partial ASR every 1s while speaking */
#define ASR_UTTERANCE_MAX_S     30

#define SILICONFLOW_ASR_ENDPOINT  "https://api.siliconflow.cn/v1/audio/transcriptions"
#define SILICONFLOW_CHAT_ENDPOINT "https://api.siliconflow.cn/v1/chat/completions"
#define DEFAULT_ASR_MODEL         "FunAudioLLM/SenseVoiceSmall"
#define DEFAULT_LLM_MODEL         "Qwen/Qwen2.5-7B-Instruct"

typedef enum {
    ASR_PROVIDER_SILICONFLOW = 0,
    ASR_PROVIDER_NONE
} asr_provider_t;

typedef struct {
    bool asr_enabled;
    bool llm_enabled;
    asr_provider_t provider;
    char api_key[MAX_API_KEY_LEN];
    char asr_model[MAX_MODEL_NAME_LEN];
    char llm_model[MAX_MODEL_NAME_LEN];
    char asr_endpoint[MAX_ENDPOINT_LEN];
    char chat_endpoint[MAX_ENDPOINT_LEN];
    bool mayday_detection_via_llm;
    bool violation_detection;
    bool auto_logging;
    bool realtime_translation;
} ai_config_t;

#define BLE_NOTIFY_ALERT        1
#define BLE_NOTIFY_LOG          2
#define BLE_NOTIFY_INTERFERENCE 3
#define BLE_NOTIFY_FREQ         4

typedef enum {
    RADIO_MODE_RX = 0,
    RADIO_MODE_TX,
    RADIO_MODE_SCAN
} radio_mode_t;

typedef enum {
    BAND_HF_160M = 0,
    BAND_HF_80M,
    BAND_HF_40M,
    BAND_HF_30M,
    BAND_HF_20M,
    BAND_HF_17M,
    BAND_HF_15M,
    BAND_HF_12M,
    BAND_HF_10M,
    BAND_VHF_2M,
    BAND_UHF_70CM,
    BAND_MAX
} radio_band_t;

typedef struct {
    float frequency;
    radio_band_t band;
    radio_mode_t mode;
    float signal_strength_dbm;
    float noise_floor_dbm;
    uint32_t frequency_khz;
} radio_freq_info_t;

typedef struct {
    char callsign[MAX_CALLSIGN_LEN];
    char qth[MAX_QTH_LEN];
    char rapport[MAX_RAPPORT_LEN];
    uint32_t timestamp;
    float frequency;
    int8_t signal_report;
    char mode[8];
} qso_entry_t;

typedef enum {
    ALERT_LEVEL_NONE = 0,
    ALERT_LEVEL_INFO,
    ALERT_LEVEL_WARNING,
    ALERT_LEVEL_MAYDAY,
    ALERT_LEVEL_INTERFERENCE,
    ALERT_LEVEL_MALICIOUS
} alert_level_t;

typedef struct {
    alert_level_t level;
    char message[256];
    uint32_t timestamp;
    float frequency;
    char details[512];
} alert_event_t;

typedef enum {
    TRANSLATE_ZH = 0,
    TRANSLATE_EN,
    TRANSLATE_JA,
    TRANSLATE_RU,
    TRANSLATE_MAX
} translate_lang_t;

int config_store_load(ai_config_t *config);
int config_store_save(const ai_config_t *config);
void config_set_defaults(ai_config_t *config);

#endif
