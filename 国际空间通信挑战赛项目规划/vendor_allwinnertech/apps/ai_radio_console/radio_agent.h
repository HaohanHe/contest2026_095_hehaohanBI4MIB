/****************************************************************************
 * apps/vendor/allwinnertech/apps/ai_radio_console/radio_agent.h
 *
 * ai_agent / VelaClaw integration for radio QSO logging
 ****************************************************************************/

#ifndef __AI_RADIO_AGENT_H
#define __AI_RADIO_AGENT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize connection to ai_agent */
int radio_agent_init(void);

/* Ask ai_agent to save the transcribed radio text as a structured QSO log */
int radio_agent_save_log(const char *transcript, char **response_out);

/* Save a raw transcript locally when ai_agent is unavailable.
 * Returns 0 on success, negative errno on failure. */
int radio_agent_save_raw_log(const char *transcript);

/* Release ai_agent connection */
void radio_agent_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* __AI_RADIO_AGENT_H */
