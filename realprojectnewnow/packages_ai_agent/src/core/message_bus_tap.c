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

/*
 * This file contains code derived from MimiClaw (https://github.com/memovai/mimiclaw)
 * Copyright (c) 2026 Ziboyan Wang, licensed under the MIT License.
 * See NOTICE file for the original MIT License terms.
 */

/**
 * message_bus_tap.c — outbound message tap mechanism
 *
 * A lightweight per-channel intercept for outbound messages.
 * Used by the integration test harness to capture agent responses
 * without interfering with normal channel dispatch.
 *
 * Max 4 concurrent taps.  When no taps are registered the overhead
 * is a single mutex lock/unlock per outbound message.
 */

#include "core/message_bus_tap.h"

#include <pthread.h>
#include <string.h>
#include <syslog.h>

#define TAP_MAX_SLOTS 4

typedef struct {
    char channel[16];
    mbus_tap_cb_t cb;
    void* cookie;
    bool active;
} tap_slot_t;

static tap_slot_t g_taps[TAP_MAX_SLOTS];
static pthread_mutex_t g_tap_lock = PTHREAD_MUTEX_INITIALIZER;

int mbus_tap_register(const char* channel, mbus_tap_cb_t cb, void* cookie)
{
    if (!channel || !cb) {
        return ERROR;
    }

    pthread_mutex_lock(&g_tap_lock);

    /* Check for duplicate */
    for (int i = 0; i < TAP_MAX_SLOTS; i++) {
        if (g_taps[i].active && strcmp(g_taps[i].channel, channel) == 0) {
            pthread_mutex_unlock(&g_tap_lock);
            syslog(LOG_WARNING, "[tap] channel '%s' already tapped\n", channel);
            return ERROR;
        }
    }

    /* Find free slot */
    for (int i = 0; i < TAP_MAX_SLOTS; i++) {
        if (!g_taps[i].active) {
            strncpy(g_taps[i].channel, channel, sizeof(g_taps[i].channel) - 1);
            g_taps[i].channel[sizeof(g_taps[i].channel) - 1] = '\0';
            g_taps[i].cb = cb;
            g_taps[i].cookie = cookie;
            g_taps[i].active = true;
            pthread_mutex_unlock(&g_tap_lock);
            syslog(LOG_INFO, "[tap] registered tap for channel '%s'\n", channel);
            return OK;
        }
    }

    pthread_mutex_unlock(&g_tap_lock);
    syslog(LOG_WARNING, "[tap] no free slots (max %d)\n", TAP_MAX_SLOTS);
    return ERROR;
}

void mbus_tap_unregister(const char* channel)
{
    if (!channel) {
        return;
    }

    pthread_mutex_lock(&g_tap_lock);

    for (int i = 0; i < TAP_MAX_SLOTS; i++) {
        if (g_taps[i].active && strcmp(g_taps[i].channel, channel) == 0) {
            g_taps[i].active = false;
            g_taps[i].cb = NULL;
            g_taps[i].cookie = NULL;
            syslog(LOG_INFO, "[tap] unregistered tap for channel '%s'\n", channel);
            break;
        }
    }

    pthread_mutex_unlock(&g_tap_lock);
}

bool mbus_tap_try_deliver(const agent_msg_t* msg)
{
    if (!msg || !msg->channel[0]) {
        return false;
    }

    pthread_mutex_lock(&g_tap_lock);

    for (int i = 0; i < TAP_MAX_SLOTS; i++) {
        if (g_taps[i].active && strcmp(g_taps[i].channel, msg->channel) == 0) {
            mbus_tap_cb_t cb = g_taps[i].cb;
            void* cookie = g_taps[i].cookie;
            pthread_mutex_unlock(&g_tap_lock);

            /* Invoke callback outside the lock to avoid deadlock */
            cb(msg, cookie);
            return true;
        }
    }

    pthread_mutex_unlock(&g_tap_lock);
    return false;
}
