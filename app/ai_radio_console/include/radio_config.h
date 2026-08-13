/****************************************************************************
 * daynote_config.h - DayNote Configuration
 *
 * All configuration constants for the DayNote voice memo device.
 ****************************************************************************/

#ifndef __DAYNOTE_CONFIG_H
#define __DAYNOTE_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

#define DAYNOTE_APP_NAME        "daynote"
#define DAYNOTE_APP_VERSION     "1.0"
#define DAYNOTE_APP_STACKSIZE   16384
#define DAYNOTE_APP_PRIORITY    100

/* Audio configuration */
#define AUDIO_SAMPLE_RATE       16000
#define AUDIO_CHANNELS          1
#define AUDIO_BITS_PER_SAMPLE   16
#define AUDIO_FRAME_MS          20
#define AUDIO_FRAME_SIZE        (AUDIO_SAMPLE_RATE * AUDIO_CHANNELS * AUDIO_BITS_PER_SAMPLE / 8 * AUDIO_FRAME_MS / 1000)

/* Gemini-S1 (R528) audio device paths */
#define AUDIO_CAPTURE_DEV       "/dev/audio/pcm0c"
#define AUDIO_PLAYBACK_DEV      "/dev/audio/pcm0p"

/* Storage paths */
#define DAYNOTE_DATA_DIR        "/data/daynote"
#define DAYNOTE_AUDIO_DIR       "/data/daynote/audio"
#define DAYNOTE_INDEX_PATH      "/data/daynote/hippocampus.json"
#define DAYNOTE_CONFIG_PATH     "/data/daynote/config.ini"
#define AGENT_MEMORY_DIR        "/data/agent/memory"
#define SKILLS_PATH             "/data/agent/skills/"

/* Size limits */
#define MAX_API_KEY_LEN         128
#define MAX_MODEL_NAME_LEN      64
#define MAX_ENDPOINT_LEN        128
#define MAX_TRANSCRIPT_LEN      4096
#define MAX_LLM_RESPONSE_LEN    8192

/* UI refresh rate */
#define UI_REFRESH_MS           100

/* ASR streaming parameters */
#define ASR_STREAM_TARGET_RATE  16000
#define ASR_STREAM_INPUT_RATE   8000
#define ASR_VAD_FRAME_MS        20
#define ASR_VAD_SPEECH_FRAMES   5
#define ASR_VAD_SILENT_FRAMES   25
#define ASR_VAD_THRESHOLD_DB    -40.0f
#define ASR_PARTIAL_INTERVAL_MS 1000
#define ASR_UTTERANCE_MAX_S     30

/* SiliconFlow API endpoints */
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
    bool auto_logging;
    bool realtime_translation;
} ai_config_t;

typedef enum {
    TRANSLATE_ZH = 0,
    TRANSLATE_EN,
    TRANSLATE_JA,
    TRANSLATE_RU,
    TRANSLATE_MAX
} translate_lang_t;

#endif
