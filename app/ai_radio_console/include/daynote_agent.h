#ifndef DAYNOTE_AGENT_H
#define DAYNOTE_AGENT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

int  daynote_agent_init(void);
int  daynote_agent_start(void);
void daynote_agent_stop(void);
int  daynote_agent_handle_command(const char *cmd, char *response, size_t max_len);
const char *daynote_agent_get_status(void);

#ifdef __cplusplus
}
#endif

#endif
