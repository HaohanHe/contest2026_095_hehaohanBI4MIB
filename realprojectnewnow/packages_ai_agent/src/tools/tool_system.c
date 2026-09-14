/*
 * Copyright (C) 2026 Xiaomi Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "tools/tool_system.h"
#include "agent_compat.h"

#include <string.h>
#include <syslog.h>

#include "cJSON.h"

#ifdef CONFIG_UORB
#include <uORB/uORB.h>
#include <system/state.h>
#endif

/* Helper: subscribe, copy once, unsubscribe */
#ifdef CONFIG_UORB
static int orb_read_once(const struct orb_metadata *meta, void *buf)
{
    int fd = orb_subscribe(meta);
    if (fd < 0) return -1;
    int ret = orb_copy(meta, fd, buf);
    orb_unsubscribe(fd);
    return ret;
}
#endif

int tool_get_battery_execute(const char *input_json, char *output, size_t output_size)
{
    (void)input_json;

#ifdef CONFIG_UORB
    struct battery_state bs;
    memset(&bs, 0, sizeof(bs));
    if (orb_read_once(ORB_ID(battery_state), &bs) < 0) {
        snprintf(output, output_size, "{\"error\":\"sensor not available\"}");
        return ERROR;
    }

    cJSON *r = cJSON_CreateObject();
    cJSON_AddNumberToObject(r, "level",    bs.level);
    cJSON_AddNumberToObject(r, "charging", bs.state == 1 ? 1 : 0);
    cJSON_AddNumberToObject(r, "voltage",  bs.voltage);
    cJSON_AddNumberToObject(r, "temp",     bs.temp);
    char *s = cJSON_PrintUnformatted(r);
    cJSON_Delete(r);
    if (s) { strncpy(output, s, output_size - 1); output[output_size-1] = '\0'; free(s); }
    return OK;
#else
    snprintf(output, output_size, "{\"error\":\"uORB not enabled\"}");
    return ERROR;
#endif
}

int tool_get_wear_state_execute(const char *input_json, char *output, size_t output_size)
{
    (void)input_json;

#ifdef CONFIG_UORB
    struct wear_state ws;
    memset(&ws, 0, sizeof(ws));
    if (orb_read_once(ORB_ID(wear_state), &ws) < 0) {
        snprintf(output, output_size, "{\"error\":\"sensor not available\"}");
        return ERROR;
    }

    cJSON *r = cJSON_CreateObject();
    /* wear: 1=wore, 2=not wore */
    cJSON_AddBoolToObject(r, "worn", ws.wear == 1);
    char *s = cJSON_PrintUnformatted(r);
    cJSON_Delete(r);
    if (s) { strncpy(output, s, output_size - 1); output[output_size-1] = '\0'; free(s); }
    return OK;
#else
    snprintf(output, output_size, "{\"error\":\"uORB not enabled\"}");
    return ERROR;
#endif
}

int tool_get_screen_state_execute(const char *input_json, char *output, size_t output_size)
{
    (void)input_json;

#ifdef CONFIG_UORB
    struct screen_onoff so;
    memset(&so, 0, sizeof(so));
    if (orb_read_once(ORB_ID(screen_onoff), &so) < 0) {
        snprintf(output, output_size, "{\"error\":\"sensor not available\"}");
        return ERROR;
    }

    cJSON *r = cJSON_CreateObject();
    cJSON_AddBoolToObject(r, "on", so.onoff == 1);
    char *s = cJSON_PrintUnformatted(r);
    cJSON_Delete(r);
    if (s) { strncpy(output, s, output_size - 1); output[output_size-1] = '\0'; free(s); }
    return OK;
#else
    snprintf(output, output_size, "{\"error\":\"uORB not enabled\"}");
    return ERROR;
#endif
}
