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

#include "channels/feishu_internal.h"
#include "cJSON.h"

#include <stdlib.h>
#include <string.h>

static const char* TAG = "feishu_send";

/* ── Outgoing @mention conversion ──────────────────────────────── */

static char* convert_outgoing_mentions(const char* text, size_t* clean_end)
{
    typedef struct {
        char name[64];
        char oid[64];
    } mention_entry_t;

    mention_entry_t mmap[8];
    int mcount = 0;

    const char* block = strstr(text, "\n[mentioned_users:");
    if (!block) {
        block = strstr(text, "[mentioned_users:");
    }

    size_t text_end = strlen(text);

    if (block) {
        text_end = (size_t)(block - text);
        const char* start = strchr(block, ':');
        const char* end = strchr(block, ']');

        if (start && end && start < end) {
            start++;
            while (start < end && mcount < 8) {
                while (start < end && *start == ' ') {
                    start++;
                }

                const char* eq = memchr(start, '=', (size_t)(end - start));
                if (!eq) {
                    break;
                }

                const char* comma = memchr(eq, ',', (size_t)(end - eq));
                if (!comma) {
                    comma = end;
                }

                size_t nlen = (size_t)(eq - start);
                size_t olen = (size_t)(comma - eq - 1);

                if (nlen > 0 && nlen < 64 && olen > 0 && olen < 64) {
                    memcpy(mmap[mcount].name, start, nlen);
                    mmap[mcount].name[nlen] = '\0';
                    memcpy(mmap[mcount].oid, eq + 1, olen);
                    mmap[mcount].oid[olen] = '\0';
                    mcount++;
                }
                start = comma < end ? comma + 1 : end;
            }
        }
    }

    *clean_end = text_end;

    bool need_convert = (mcount > 0) || strstr(text, "(open_id:");
    if (!need_convert) {
        return NULL;
    }

    size_t alloc = text_end * 3 + 1;
    char* out = malloc(alloc);
    if (!out) {
        return NULL;
    }

    size_t out_off = 0;
    const char* p = text;
    const char* p_end = text + text_end;

    while (p < p_end && out_off < alloc - 1) {
        if (*p == '@') {
            /* Try inline: @name(open_id:ou_xxx) */
            const char* ns = p + 1;
            const char* paren = NULL;

            for (const char* s = ns; s < p_end && *s != ' ' && *s != '\n'; s++) {
                if (*s == '(') {
                    paren = s;
                    break;
                }
            }

            if (paren && (size_t)(p_end - paren) > 9 &&
                strncmp(paren, "(open_id:", 9) == 0) {
                const char* id_s = paren + 9;
                const char* id_e = strchr(id_s, ')');

                if (id_e && id_e <= p_end && (size_t)(id_e - id_s) < 64) {
                    int n = snprintf(out + out_off, alloc - out_off,
                        "<at user_id=\"%.*s\">%.*s</at>",
                        (int)(id_e - id_s), id_s,
                        (int)(paren - ns), ns);
                    if (n > 0) {
                        out_off += (size_t)n;
                    }
                    p = id_e + 1;
                    continue;
                }
            }

            /* Try map-based: @name from mention map */
            if (mcount > 0) {
                bool matched = false;

                for (int i = 0; i < mcount; i++) {
                    size_t nlen = strlen(mmap[i].name);

                    if (ns + nlen <= p_end &&
                        strncmp(ns, mmap[i].name, nlen) == 0) {
                        char next = (ns + nlen < p_end) ? ns[nlen] : ' ';
                        if (next == ' ' || next == '\n' || next == ',' ||
                            next == '\0' || (unsigned char)next >= 0x80) {
                            int n = snprintf(out + out_off, alloc - out_off,
                                "<at user_id=\"%s\">%s</at>",
                                mmap[i].oid, mmap[i].name);
                            if (n > 0) {
                                out_off += (size_t)n;
                            }
                            p = ns + nlen;
                            matched = true;
                            break;
                        }
                    }
                }
                if (matched) {
                    continue;
                }
            }
        }
        out[out_off++] = *p++;
    }
    out[out_off] = '\0';
    return out;
}

/* ── Message sending with chunking ─────────────────────────────── */

static int send_message_chunks(const char* chat_id, const char* text, size_t text_len)
{
    FEISHU_AUTH_HDR(hdrs, s_access_token);

    char* resp = malloc(4096);
    if (!resp) {
        syslog(LOG_ERR, "[%s] OOM for send resp buffer\n", TAG);
        return ERROR;
    }

    size_t offset = 0;

    while (offset < text_len) {
        size_t chunk = text_len - offset;

        if (chunk > AGENT_FEISHU_MAX_MSG_LEN) {
            chunk = AGENT_FEISHU_MAX_MSG_LEN;
            /* Avoid splitting UTF-8 sequences */
            while (chunk > 0 &&
                   ((unsigned char)text[offset + chunk] & 0xC0) == 0x80) {
                chunk--;
            }
            if (chunk == 0) {
                chunk = AGENT_FEISHU_MAX_MSG_LEN;
            }
        }

        char* seg = malloc(chunk + 1);
        if (!seg) {
            free(resp);
            return ERROR;
        }

        memcpy(seg, text + offset, chunk);
        seg[chunk] = '\0';

        cJSON* content_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(content_obj, "text", seg);
        free(seg);
        char* content_str = cJSON_PrintUnformatted(content_obj);
        cJSON_Delete(content_obj);

        if (!content_str) {
            offset += chunk;
            continue;
        }

        cJSON* body_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(body_obj, "receive_id", chat_id);
        cJSON_AddStringToObject(body_obj, "msg_type", "text");
        cJSON_AddStringToObject(body_obj, "content", content_str);
        free(content_str);
        char* body_str = cJSON_PrintUnformatted(body_obj);
        cJSON_Delete(body_obj);

        if (!body_str) {
            offset += chunk;
            continue;
        }

        int status = feishu_https_post(
            "/open-apis/im/v1/messages?receive_id_type=chat_id",
            hdrs, body_str, resp, 4096);

        if (status == 400 && strstr(resp, "access token")) {
            syslog(LOG_INFO, "[%s] Token invalid on send, refreshing\n", TAG);
            if (feishu_get_app_token() == OK) {
                char ra[520];
                snprintf(ra, sizeof(ra), "Bearer %s", s_access_token);
                vela_header_t rh[2] = { { "Authorization", ra }, { NULL, NULL } };
                status = feishu_https_post(
                    "/open-apis/im/v1/messages?receive_id_type=chat_id",
                    rh, body_str, resp, 4096);
            }
        }
        free(body_str);

        if (status != 200 && status != 201) {
            syslog(LOG_WARNING, "[%s] send_message HTTP %d: %.120s\n",
                TAG, status, resp);
        }

        offset += chunk;
    }

    free(resp);
    return OK;
}

/* ── Public API ────────────────────────────────────────────────── */

int feishu_send_message(const char* chat_id, const char* text)
{
    if (s_access_token[0] == '\0' || feishu_token_expired()) {
        syslog(LOG_INFO, "[%s] Token expired/missing, refreshing before send\n", TAG);
        if (feishu_get_app_token() != OK) {
            syslog(LOG_WARNING, "[%s] Cannot send: token refresh failed\n", TAG);
            return ERROR;
        }
    }

    size_t clean_end;
    char* converted = convert_outgoing_mentions(text, &clean_end);

    if (converted) {
        text = converted;
    } else if (clean_end < strlen(text)) {
        /* No @mentions but strip metadata block */
        converted = malloc(clean_end + 1);
        if (converted) {
            memcpy(converted, text, clean_end);
            converted[clean_end] = '\0';
            text = converted;
        }
    }

    int ret = send_message_chunks(chat_id, text, strlen(text));

    free(converted);
    return ret;
}
