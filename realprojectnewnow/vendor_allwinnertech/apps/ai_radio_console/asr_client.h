/****************************************************************************
 * apps/vendor/allwinnertech/apps/ai_radio_console/asr_client.h
 *
 * Simple HTTP ASR client for OpenAI-compatible transcription APIs
 ****************************************************************************/

#ifndef __AI_RADIO_ASR_CLIENT_H
#define __AI_RADIO_ASR_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize ASR client with endpoint, model and api key */
int asr_client_init(const char *url, const char *model, const char *api_key);

/* Transcribe PCM16LE mono audio data. text_out must be freed by caller. */
int asr_transcribe(const uint8_t *pcm_data, size_t pcm_len,
                   uint32_t sample_rate, char **text_out);

/* Release ASR client resources */
void asr_client_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* __AI_RADIO_ASR_CLIENT_H */
