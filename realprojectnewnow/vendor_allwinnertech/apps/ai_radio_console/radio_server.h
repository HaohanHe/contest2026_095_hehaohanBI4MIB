/****************************************************************************
 * apps/vendor/allwinnertech/apps/ai_radio_console/radio_server.h
 *
 * Minimal REST + WebSocket server for com.agent.coapp companion app.
 ****************************************************************************/

#ifndef __AI_RADIO_SERVER_H
#define __AI_RADIO_SERVER_H

#include <nuttx/config.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_AI_RADIO_CONSOLE_SERVER

/* Start/stop the HTTP/WebSocket server */
int radio_server_start(void);
void radio_server_stop(void);

/* Broadcast state/status to all connected WebSocket clients */
void radio_server_broadcast_status(const char *status);

/* Broadcast a log entry to all connected WebSocket clients */
void radio_server_broadcast_log(const char *timestamp, const char *text);

#else

/* Stubs when the built-in server is disabled. */
static inline int radio_server_start(void) { return 0; }
static inline void radio_server_stop(void) { }
static inline void radio_server_broadcast_status(const char *status) { (void)status; }
static inline void radio_server_broadcast_log(const char *timestamp, const char *text)
{ (void)timestamp; (void)text; }

#endif /* CONFIG_AI_RADIO_CONSOLE_SERVER */

#ifdef __cplusplus
}
#endif

#endif /* __AI_RADIO_SERVER_H */
