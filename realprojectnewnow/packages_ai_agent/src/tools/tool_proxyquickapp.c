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

#include "tools/tool_proxyquickapp.h"
#include "agent_compat.h"

#include <string.h>
#include "cJSON.h"

#if defined(CONFIG_LVX_ACTIVITY_MANAGER)
#include <lvgl/lvgl.h>
#include <activitymanager/lvx_navigator.h>
#endif

static const char *TAG = "tool_proxyquickapp";

#if defined(CONFIG_LVX_ACTIVITY_MANAGER)
/**
 * lv_timer callback — guaranteed to run on the miwear UI thread
 * because lv_timer_handler() is called from the miwear main loop.
 *
 * user_data is a strdup'd URL string — this callback owns it and
 * must free it before returning.
 */
static void launch_timer_cb(lv_timer_t *timer)
{
    char *url = (char *)timer->user_data;
    if (!url || url[0] == '\0') {
        free(url);
        return;
    }

    syslog(LOG_INFO, "[%s] timer cb: navigate to %s\n", TAG, url);

    lvx_navi_action action = {
        .full = 0,
        .url = url,
    };
    Navigator.navigate_app(&action);
    free(url);
}

/* ── Core launch ──────────────────────────────────────────────── */
static int tool_launch_quickapp(const char *package_name)
{
    syslog(LOG_INFO, "[%s] launch_quickapp %s\n", TAG, package_name);

    if (!package_name || strlen(package_name) == 0) {
        syslog(LOG_ERR, "[%s] launch_quickapp: invalid package_name\n", TAG);
        return ERROR;
    }

    /* Each timer gets its own heap-allocated URL copy, so rapid
     * back-to-back launches don't clobber each other's data. */
    char *url_copy = strdup(package_name);
    if (!url_copy) {
        syslog(LOG_ERR, "[%s] strdup failed for package_name\n", TAG);
        return ERROR;
    }

    /* Create a one-shot lv_timer (period=0, repeat=1).
     * The callback runs in the LVGL/miwear UI thread context
     * and frees url_copy when done. */
    lv_timer_t *t = lv_timer_create(launch_timer_cb, 0, (void *)url_copy);
    if (!t) {
        syslog(LOG_ERR, "[%s] failed to create lv_timer\n", TAG);
        free(url_copy);
        return ERROR;
    }
    lv_timer_set_repeat_count(t, 1);
    lv_timer_set_auto_delete(t, true);

    return OK;
}
#endif /* CONFIG_LVX_ACTIVITY_MANAGER */

/* ── Tool execute wrapper ─────────────────────────────────────── */
int tool_launch_quickapp_execute(const char *input_json, char *output, size_t output_size)
{
    syslog(LOG_INFO, "[%s] launch_quickapp_execute\n", TAG);

#if !defined(CONFIG_LVX_ACTIVITY_MANAGER)
    (void)input_json;
    snprintf(output, output_size, "{\"error\":\"activity manager not supported\"}");
    return ERROR;
#else
    cJSON *root = cJSON_Parse(input_json);
    if (!root) {
        snprintf(output, output_size, "{\"error\":\"invalid JSON\"}");
        return ERROR;
    }

    cJSON *pkg = cJSON_GetObjectItem(root, "package_name");
    if (!pkg || !cJSON_IsString(pkg) || !pkg->valuestring[0]) {
        snprintf(output, output_size, "{\"error\":\"missing package_name\"}");
        cJSON_Delete(root);
        return ERROR;
    }

    const char *test_pkg = pkg->valuestring;
    int ret = tool_launch_quickapp(test_pkg);
    syslog(LOG_INFO, "[%s] package_name: %s\n", TAG, test_pkg);

    if (ret == OK) {
        snprintf(output, output_size,
                 "{\"status\":\"launched\",\"package\":\"%s\"}", test_pkg);
    } else {
        snprintf(output, output_size, "{\"error\":\"launch failed\"}");
    }

    cJSON_Delete(root);
    return ret;
#endif /* CONFIG_LVX_ACTIVITY_MANAGER */
}

#if defined(CONFIG_LVX_ACTIVITY_MANAGER)
/* ── Exit timer callback ──────────────────────────────────────── */
static void exit_timer_cb(lv_timer_t *timer)
{
    syslog(LOG_INFO, "[%s] timer cb: navigate_home\n", TAG);
    Navigator.navigate_home(true);
}

/* ── Core exit ────────────────────────────────────────────────── */
static int tool_exit_quickapp(void)
{
    syslog(LOG_INFO, "[%s] exit_quickapp\n", TAG);

    lv_timer_t *t = lv_timer_create(exit_timer_cb, 0, NULL);
    if (!t) {
        syslog(LOG_ERR, "[%s] failed to create lv_timer\n", TAG);
        return ERROR;
    }
    lv_timer_set_repeat_count(t, 1);
    lv_timer_set_auto_delete(t, true);

    return OK;
}
#endif /* CONFIG_LVX_ACTIVITY_MANAGER */

/* ── Tool exit execute wrapper ────────────────────────────────── */
int tool_exit_quickapp_execute(const char *input_json, char *output, size_t output_size)
{
    (void)input_json;
    syslog(LOG_INFO, "[%s] exit_quickapp_execute\n", TAG);

#if !defined(CONFIG_LVX_ACTIVITY_MANAGER)
    snprintf(output, output_size, "{\"error\":\"activity manager not supported\"}");
    return ERROR;
#else
    int ret = tool_exit_quickapp();

    if (ret == OK) {
        snprintf(output, output_size, "{\"status\":\"exited\"}");
    } else {
        snprintf(output, output_size, "{\"error\":\"exit failed\"}");
    }

    return ret;
#endif /* CONFIG_LVX_ACTIVITY_MANAGER */
}
