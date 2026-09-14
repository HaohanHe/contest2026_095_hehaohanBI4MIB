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
#include "core/message_bus.h"
#include "cJSON.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char* TAG = "feishu_recv";

/* ── Message deduplication ring buffer ─────────────────────────── */

#define DEDUP_RING_SIZE 16
#define CONTENT_DEDUP_WINDOW_MS 120000 /* 120 seconds window for content dedup */

static char s_dedup_ring[DEDUP_RING_SIZE][64];
static int s_dedup_idx = 0;

/* Content-based dedup: (chat_id hash + content hash + timestamp) */
typedef struct {
    uint32_t hash;
    uint32_t timestamp; /* UNIX epoch seconds */
} content_dedup_entry_t;

static content_dedup_entry_t s_content_dedup[DEDUP_RING_SIZE];
static int s_content_dedup_idx = 0;

/* Feishu message_id dedup ring (om_xxx is stable across event retries) */
static char s_msgid_ring[DEDUP_RING_SIZE][64];
static int s_msgid_idx = 0;

/* ── Bot-to-bot ping-pong rate limiter ───────────────────────── */

#define BOT_RATE_SLOTS    8
#define BOT_RATE_MAX      2   /* max bot msgs per chat within window */
#define BOT_RATE_WINDOW_S 120 /* seconds */

typedef struct {
    char chat_id[64];
    uint32_t timestamps[BOT_RATE_MAX];
    int idx;
} bot_rate_entry_t;

static bot_rate_entry_t s_bot_rate[BOT_RATE_SLOTS];

/* ── Dedup helper functions ────────────────────────────────────── */

static bool dedup_check_and_record(const char* msg_id)
{
    if (!msg_id || msg_id[0] == '\0') {
        return false;
    }

    for (int i = 0; i < DEDUP_RING_SIZE; i++) {
        if (s_dedup_ring[i][0] && strcmp(s_dedup_ring[i], msg_id) == 0) {
            return true;
        }
    }

    strncpy(s_dedup_ring[s_dedup_idx], msg_id, sizeof(s_dedup_ring[0]) - 1);
    s_dedup_ring[s_dedup_idx][sizeof(s_dedup_ring[0]) - 1] = '\0';
    s_dedup_idx = (s_dedup_idx + 1) % DEDUP_RING_SIZE;
    return false;
}

static bool msgid_dedup_check(const char* msg_id)
{
    if (!msg_id || !msg_id[0]) {
        return false;
    }
    for (int i = 0; i < DEDUP_RING_SIZE; i++) {
        if (s_msgid_ring[i][0] && strcmp(s_msgid_ring[i], msg_id) == 0) {
            return true;
        }
    }
    strncpy(s_msgid_ring[s_msgid_idx], msg_id, sizeof(s_msgid_ring[0]) - 1);
    s_msgid_ring[s_msgid_idx][sizeof(s_msgid_ring[0]) - 1] = '\0';
    s_msgid_idx = (s_msgid_idx + 1) % DEDUP_RING_SIZE;
    return false;
}

static uint32_t simple_hash(const char* chat_id, const char* content)
{
    uint32_t h = 5381;

    if (chat_id) {
        for (const char* p = chat_id; *p; p++) {
            h = ((h << 5) + h) ^ (uint8_t)*p;
        }
    }
    if (content) {
        for (const char* p = content; *p; p++) {
            h = ((h << 5) + h) ^ (uint8_t)*p;
        }
    }
    return h;
}

static bool content_dedup_check(const char* chat_id, const char* content)
{
    uint32_t hash = simple_hash(chat_id, content);
    uint32_t now = (uint32_t)time(NULL);
    uint32_t window_sec = CONTENT_DEDUP_WINDOW_MS / 1000;

    for (int i = 0; i < DEDUP_RING_SIZE; i++) {
        if (s_content_dedup[i].hash == hash && s_content_dedup[i].timestamp > 0) {
            uint32_t age = now - s_content_dedup[i].timestamp;
            if (age < window_sec) {
                syslog(LOG_WARNING, "[%s] Duplicate content within %lus, skipping\n",
                    TAG, (unsigned long)age);
                return true;
            }
        }
    }

    s_content_dedup[s_content_dedup_idx].hash = hash;
    s_content_dedup[s_content_dedup_idx].timestamp = now;
    s_content_dedup_idx = (s_content_dedup_idx + 1) % DEDUP_RING_SIZE;
    return false;
}

static bool bot_rate_limited(const char* chat_id)
{
    uint32_t now = (uint32_t)time(NULL);
    bot_rate_entry_t* slot = NULL;

    for (int i = 0; i < BOT_RATE_SLOTS; i++) {
        if (s_bot_rate[i].chat_id[0] &&
            strcmp(s_bot_rate[i].chat_id, chat_id) == 0) {
            slot = &s_bot_rate[i];
            break;
        }
    }

    if (!slot) {
        uint32_t oldest = UINT32_MAX;
        int oldest_i = 0;

        for (int i = 0; i < BOT_RATE_SLOTS; i++) {
            if (!s_bot_rate[i].chat_id[0]) {
                oldest_i = i;
                break;
            }
            uint32_t ts = s_bot_rate[i].timestamps[0];
            for (int j = 1; j < BOT_RATE_MAX; j++) {
                if (s_bot_rate[i].timestamps[j] < ts) {
                    ts = s_bot_rate[i].timestamps[j];
                }
            }
            if (ts < oldest) {
                oldest = ts;
                oldest_i = i;
            }
        }
        slot = &s_bot_rate[oldest_i];
        memset(slot, 0, sizeof(*slot));
        strncpy(slot->chat_id, chat_id, sizeof(slot->chat_id) - 1);
    }

    int recent = 0;

    for (int i = 0; i < BOT_RATE_MAX; i++) {
        if (slot->timestamps[i] > 0 &&
            (now - slot->timestamps[i]) < BOT_RATE_WINDOW_S) {
            recent++;
        }
    }

    if (recent >= BOT_RATE_MAX) {
        syslog(LOG_WARNING,
            "[%s] Bot rate limit hit for chat %.24s (%d msgs in %ds), dropping\n",
            TAG, chat_id, recent, BOT_RATE_WINDOW_S);
        return true;
    }

    slot->timestamps[slot->idx] = now;
    slot->idx = (slot->idx + 1) % BOT_RATE_MAX;
    return false;
}

/* ── Frame processing helpers ──────────────────────────────────── */

static bool handle_control_frame(const fk_frame_t* f)
{
    if (f->method != 0) {
        return false;
    }

    if (strcmp(f->h_type, "pong") == 0) {
        syslog(LOG_DEBUG, "[%s] pong received (alive)\n", TAG);
        return true;
    }

    if (strcmp(f->h_type, "ping") == 0) {
        syslog(LOG_INFO, "[%s] Control ping (seq=%llu), sending pong ACK\n",
            TAG, (unsigned long long)f->seq_id);
        uint8_t buf[512];
        int p = 0;

        p = pb_put_varint_field(buf, p, (int)sizeof(buf), 1, f->seq_id);
        if (p >= 0) {
            p = pb_put_varint_field(buf, p, (int)sizeof(buf), 2, 0);
        }
        if (p >= 0) {
            p = pb_put_varint_field(buf, p, (int)sizeof(buf), 3,
                (uint32_t)f->service);
        }
        if (p >= 0) {
            p = pb_put_varint_field(buf, p, (int)sizeof(buf), 4, 0);
        }
        if (p >= 0) {
            p = pb_encode_header_entry(buf, p, (int)sizeof(buf), "type", "pong");
        }
        if (p >= 0 && f->payload && f->payload_len > 0) {
            p = pb_put_len_field(buf, p, (int)sizeof(buf), 8,
                f->payload, f->payload_len);
        }
        if (p > 0) {
            ws_send_frame(&s_ws, 0x02, buf, (size_t)p);
        } else {
            syslog(LOG_ERR, "[%s] Failed to encode pong frame\n", TAG);
        }
        return true;
    }

    syslog(LOG_DEBUG, "[%s] Control frame type=%s (ignored)\n", TAG, f->h_type);
    return true;
}

static void send_data_ack(const fk_frame_t* f)
{
    static uint8_t ack_buf[512];
    int len = fk_encode_ack(ack_buf, (int)sizeof(ack_buf), f);

    if (len > 0) {
        ws_send_frame(&s_ws, 0x02, (const char*)ack_buf, (size_t)len);
    }
}

static bool filter_sender(cJSON* ev)
{
    cJSON* sender = cJSON_GetObjectItem(ev, "sender");

    if (!cJSON_IsObject(sender)) {
        syslog(LOG_WARNING, "[%s] No sender in event, skipping\n", TAG);
        return true;
    }

    cJSON* sender_type = cJSON_GetObjectItem(sender, "sender_type");
    const char* stype = cJSON_IsString(sender_type) ? sender_type->valuestring : "";

    if (strcmp(stype, "user") != 0 && strcmp(stype, "app") != 0) {
        syslog(LOG_INFO, "[%s] Non-user/app sender_type='%s', skipping\n",
            TAG, stype);
        return true;
    }

    cJSON* sender_id = cJSON_GetObjectItem(sender, "sender_id");
    const char* sender_oid = NULL;

    if (cJSON_IsObject(sender_id)) {
        cJSON* uid = cJSON_GetObjectItem(sender_id, "open_id");
        if (cJSON_IsString(uid)) {
            sender_oid = uid->valuestring;
        }
    }

    if (strcmp(stype, "app") == 0) {
        if (sender_oid && s_bot_open_id[0] &&
            strcmp(sender_oid, s_bot_open_id) == 0) {
            syslog(LOG_DEBUG, "[%s] Own bot message, skipping\n", TAG);
            return true;
        }
    }

    syslog(LOG_DEBUG, "[%s] sender=%s type=%s\n", TAG,
        sender_oid ? sender_oid : "unknown", stype);
    return false;
}

static bool filter_stale_message(cJSON* msg)
{
    cJSON* ct = cJSON_GetObjectItem(msg, "create_time");

    if (!cJSON_IsString(ct) || !ct->valuestring[0]) {
        return false;
    }

    long long create_ms = atoll(ct->valuestring);
    long long now_ms = (long long)time(NULL) * 1000LL;
    long long age_ms = now_ms - create_ms;

    if (age_ms > 60000 || age_ms < -120000) {
        syslog(LOG_WARNING, "[%s] Stale message (age=%lldms), dropping\n",
            TAG, age_ms);
        return true;
    }
    return false;
}


/* ── Image handling helpers ────────────────────────────────────── */

static bool validate_image_data(const char* buf, size_t len)
{
    if (len < 4) {
        syslog(LOG_ERR, "[%s] Image too small (%d bytes)\n", TAG, (int)len);
        return false;
    }

    if (buf[0] == '{') {
        syslog(LOG_ERR, "[%s] Image API returned JSON error: %.300s\n", TAG, buf);
        return false;
    }

    bool is_jpeg = ((unsigned char)buf[0] == 0xFF &&
                    (unsigned char)buf[1] == 0xD8);
    bool is_png = ((unsigned char)buf[0] == 0x89 &&
                   buf[1] == 'P' && buf[2] == 'N' && buf[3] == 'G');

    if (!is_jpeg && !is_png) {
        syslog(LOG_ERR,
            "[%s] Unsupported image format (magic: 0x%02x%02x%02x%02x)\n", TAG,
            (unsigned char)buf[0], (unsigned char)buf[1],
            (unsigned char)buf[2], (unsigned char)buf[3]);
        return false;
    }
    return true;
}

static int download_feishu_image(const char* message_id, const char* image_key)
{
    if (feishu_token_expired()) {
        if (feishu_get_app_token() != OK) {
            syslog(LOG_WARNING,
                "[%s] Cannot download image: token refresh failed\n", TAG);
            return -1;
        }
    }

    char img_path[512];
    snprintf(img_path, sizeof(img_path),
        "/open-apis/im/v1/messages/%s/resources/%s?type=image",
        message_id, image_key);
    syslog(LOG_INFO, "[%s] Downloading image: %s\n", TAG, img_path);

    FEISHU_AUTH_HDR(hdrs, s_access_token);

    char* img_buf = malloc(AGENT_VISION_MAX_IMAGE_SIZE);
    if (!img_buf) {
        syslog(LOG_ERR, "[%s] OOM for image download buffer\n", TAG);
        return -1;
    }

    size_t img_len = 0;
    int status = feishu_https_request("GET", img_path, hdrs, NULL, 0,
        img_buf, AGENT_VISION_MAX_IMAGE_SIZE, &img_len);

    if (status != 200) {
        syslog(LOG_ERR, "[%s] Image download HTTP %d: %.200s\n",
            TAG, status, img_buf);
        free(img_buf);
        return -1;
    }

    if (!validate_image_data(img_buf, img_len)) {
        free(img_buf);
        return -1;
    }

    FILE* fp = fopen(AGENT_CAPTURE_PATH, "wb");
    if (!fp) {
        syslog(LOG_ERR, "[%s] Cannot open %s for writing\n",
            TAG, AGENT_CAPTURE_PATH);
        free(img_buf);
        return -1;
    }

    fwrite(img_buf, 1, img_len, fp);
    fclose(fp);
    free(img_buf);
    syslog(LOG_INFO, "[%s] Image saved to %s (%d bytes)\n",
        TAG, AGENT_CAPTURE_PATH, (int)img_len);
    return 0;
}

static bool handle_image_message(cJSON* msg, const char* chat_id, cJSON* content_j)
{
    cJSON* msg_id_j = cJSON_GetObjectItem(msg, "message_id");
    if (!cJSON_IsString(msg_id_j) || !msg_id_j->valuestring[0]) {
        syslog(LOG_WARNING, "[%s] image msg but no message_id\n", TAG);
        return true;
    }

    const char* image_key = NULL;
    cJSON* cj = NULL;

    if (cJSON_IsString(content_j) && content_j->valuestring[0]) {
        cj = cJSON_Parse(content_j->valuestring);
        if (cj) {
            cJSON* ik = cJSON_GetObjectItem(cj, "image_key");
            if (cJSON_IsString(ik)) {
                image_key = ik->valuestring;
            }
        }
    }

    if (!image_key || !image_key[0]) {
        cJSON* ik2 = cJSON_GetObjectItem(msg, "image_key");
        if (cJSON_IsString(ik2)) {
            image_key = ik2->valuestring;
        }
    }

    if (!image_key || !image_key[0]) {
        syslog(LOG_WARNING, "[%s] image msg but no image_key\n", TAG);
        cJSON_Delete(cj);
        return true;
    }

    if (download_feishu_image(msg_id_j->valuestring, image_key) != 0) {
        cJSON_Delete(cj);
        return true;
    }

    cJSON* ct_j = cJSON_GetObjectItem(msg, "chat_type");
    const char* ct = cJSON_IsString(ct_j) ? ct_j->valuestring : "p2p";

    if (strcmp(ct, "group") == 0) {
        cJSON* mentions = cJSON_GetObjectItem(msg, "mentions");
        if (!cJSON_IsArray(mentions) || cJSON_GetArraySize(mentions) == 0) {
            syslog(LOG_DEBUG, "[%s] Group image without @mention, skipping\n", TAG);
            cJSON_Delete(cj);
            return true;
        }
    }

    char prompt[128];
    snprintf(prompt, sizeof(prompt),
        "User sent an image. Analyze it: %s", AGENT_CAPTURE_PATH);

    agent_msg_t m = { 0 };
    strncpy(m.channel, AGENT_CHAN_FEISHU, sizeof(m.channel) - 1);
    strncpy(m.chat_id, chat_id, sizeof(m.chat_id) - 1);
    m.content = strdup(prompt);
    if (!m.content) {
        syslog(LOG_ERR, "[%s] OOM strdup image prompt\n", TAG);
        cJSON_Delete(cj);
        return true;
    }
    m.image_b64 = NULL;
    if (message_bus_push_inbound(&m) != OK) {
        free(m.content);
    }

    syslog(LOG_INFO, "[%s] Image msg from [%.24s] pushed to bus\n", TAG, chat_id);
    cJSON_Delete(cj);
    return true;
}

/* ── Text/Post content extraction ──────────────────────────────── */

static char* extract_post_text(cJSON* cj, char* post_img_key, size_t img_key_cap)
{
    cJSON* paragraphs = NULL;
    cJSON* lang = cJSON_GetObjectItem(cj, "zh_cn");

    if (!lang) {
        lang = cJSON_GetObjectItem(cj, "en_us");
    }
    if (lang) {
        paragraphs = cJSON_GetObjectItem(lang, "content");
    } else {
        paragraphs = cJSON_GetObjectItem(cj, "content");
    }

    if (!paragraphs || !cJSON_IsArray(paragraphs)) {
        return NULL;
    }

    char* buf = calloc(1, 2048);
    if (!buf) {
        return NULL;
    }

    size_t off = 0;
    cJSON* para;

    cJSON_ArrayForEach(para, paragraphs) {
        cJSON* elem;
        cJSON_ArrayForEach(elem, para) {
            const char* tag = cJSON_GetStringValue(
                cJSON_GetObjectItem(elem, "tag"));
            const char* etxt = cJSON_GetStringValue(
                cJSON_GetObjectItem(elem, "text"));
            const char* href = cJSON_GetStringValue(
                cJSON_GetObjectItem(elem, "href"));

            if (etxt && off < 2048 - 2) {
                off += snprintf(buf + off, 2048 - off, "%s", etxt);
            }
            if (href && href[0] && off < 2048 - 4) {
                off += snprintf(buf + off, 2048 - off, " %s", href);
            }
            if (tag && strcmp(tag, "img") == 0 && post_img_key[0] == '\0') {
                const char* ik = cJSON_GetStringValue(
                    cJSON_GetObjectItem(elem, "image_key"));
                if (ik && ik[0]) {
                    strncpy(post_img_key, ik, img_key_cap - 1);
                }
            }
        }
        if (off < 2048 - 2) {
            buf[off++] = '\n';
        }
    }

    if (buf[0]) {
        char* result = strdup(buf);
        free(buf);
        return result;
    }
    free(buf);
    return NULL;
}

static void download_post_image(const char* post_image_key, cJSON* msg, char** text_ptr)
{
    cJSON* msg_id_j = cJSON_GetObjectItem(msg, "message_id");
    if (!cJSON_IsString(msg_id_j) || !msg_id_j->valuestring[0]) {
        return;
    }

    if (download_feishu_image(msg_id_j->valuestring, post_image_key) != 0) {
        return;
    }

    char* old_text = *text_ptr;
    size_t hint_len = 128 + (old_text ? strlen(old_text) : 0);
    char* new_text = malloc(hint_len);
    if (!new_text) {
        return;
    }

    if (old_text && old_text[0] && old_text[0] != '\n') {
        snprintf(new_text, hint_len, "[User sent an image at %s] %s",
            AGENT_CAPTURE_PATH, old_text);
    } else {
        snprintf(new_text, hint_len,
            "User sent an image. Please analyze it using the "
            "analyze_image tool. Path: %s",
            AGENT_CAPTURE_PATH);
    }

    free(old_text);
    *text_ptr = new_text;
}

/* ── Text processing helpers ───────────────────────────────────── */

static char* trim_unicode_whitespace(char* text)
{
    char* t = text;

    while (*t == ' ' || *t == '\n' || *t == '\r' || *t == '\t' ||
           ((unsigned char)t[0] == 0xE2 && (unsigned char)t[1] == 0x80 &&
            ((unsigned char)t[2] >= 0x8B && (unsigned char)t[2] <= 0x8F)) ||
           ((unsigned char)t[0] == 0xEF && (unsigned char)t[1] == 0xBB &&
            (unsigned char)t[2] == 0xBF) ||
           ((unsigned char)t[0] == 0xC2 && (unsigned char)t[1] == 0xA0)) {
        if ((unsigned char)t[0] >= 0xC0) {
            t += ((unsigned char)t[0] >= 0xE0 ? 3 : 2);
        } else {
            t++;
        }
    }
    return t;
}

static bool validate_text_content(const char* text)
{
    const char* p = text;
    int printable = 0;
    int chinese = 0;

    while (*p) {
        unsigned char c = (unsigned char)*p;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9')) {
            printable++;
        } else if (c >= 0xE4 && c <= 0xE9 && (unsigned char)p[1] >= 0x80) {
            chinese++;
        } else if (c >= 0xC0 && c < 0xE4) {
            printable++;
        }
        p++;
    }

    if (printable < 2 && chinese < 1) {
        syslog(LOG_WARNING,
            "[%s] Content too short or garbled (%d ascii, %d cjk), skipping\n",
            TAG, printable, chinese);
        return false;
    }
    return true;
}


/* ── @mention processing helpers ───────────────────────────────── */

static void strip_placeholder(char* text, const char* key)
{
    size_t klen = strlen(key);
    char* pos;

    while ((pos = strstr(text, key)) != NULL) {
        memmove(pos, pos + klen, strlen(pos + klen) + 1);
    }
}

static void replace_placeholder(char** text_ptr, const char* key, const char* replacement)
{
    char* text = *text_ptr;
    size_t klen = strlen(key);
    size_t rlen = strlen(replacement);
    char* pos;

    while ((pos = strstr(text, key)) != NULL) {
        size_t tail_len = strlen(pos + klen);

        if (rlen > klen) {
            size_t cur_len = strlen(text);
            size_t pos_off = pos - text;
            char* new_text = realloc(text, cur_len + (rlen - klen) + 1);
            if (!new_text) {
                break;
            }
            pos = new_text + pos_off;
            text = new_text;
        }
        memmove(pos + rlen, pos + klen, tail_len + 1);
        memcpy(pos, replacement, rlen);
    }
    *text_ptr = text;
}

static bool is_bot_self_mention(const char* key, const char* mention_open_id)
{
    if (s_bot_open_id[0] && mention_open_id &&
        strcmp(mention_open_id, s_bot_open_id) == 0) {
        return true;
    }

    if (!s_bot_open_id[0] && strcmp(key, "@_user_1") == 0) {
        if (mention_open_id && mention_open_id[0]) {
            strncpy(s_bot_open_id, mention_open_id, sizeof(s_bot_open_id) - 1);
            syslog(LOG_INFO, "[%s] Learned bot open_id=%s\n", TAG, s_bot_open_id);
        }
        return true;
    }
    return false;
}

static bool finalize_mention_text(char** text_ptr, const char* mention_map)
{
    char* text = *text_ptr;

    if (mention_map[0]) {
        size_t text_len = strlen(text);
        size_t extra = 32 + strlen(mention_map);
        char* new_text = realloc(text, text_len + extra + 1);

        if (new_text) {
            text = new_text;
            snprintf(text + text_len, extra + 1,
                "\n[mentioned_users: %s]", mention_map);
        }
    }

    char* trimmed = trim_unicode_whitespace(text);
    if (trimmed != text) {
        memmove(text, trimmed, strlen(trimmed) + 1);
    }

    if (!text[0]) {
        syslog(LOG_DEBUG, "[%s] Group @mention with empty text, skipping\n", TAG);
        free(text);
        *text_ptr = NULL;
        return true;
    }

    *text_ptr = text;
    return false;
}

static void apply_single_mention(cJSON* m_item, char** text_ptr,
    char* mention_map, size_t map_cap, size_t* map_off)
{
    cJSON* key_j = cJSON_GetObjectItem(m_item, "key");
    cJSON* name_j = cJSON_GetObjectItem(m_item, "name");
    cJSON* id_j = cJSON_GetObjectItem(m_item, "id");

    if (!cJSON_IsString(key_j) || !key_j->valuestring[0]) {
        return;
    }

    const char* mention_oid = NULL;
    if (cJSON_IsObject(id_j)) {
        cJSON* oid = cJSON_GetObjectItem(id_j, "open_id");
        if (cJSON_IsString(oid)) {
            mention_oid = oid->valuestring;
        }
    }

    const char* key = key_j->valuestring;

    if (is_bot_self_mention(key, mention_oid)) {
        strip_placeholder(*text_ptr, key);
        return;
    }

    const char* display = (cJSON_IsString(name_j) && name_j->valuestring[0])
        ? name_j->valuestring : "user";
    char repl[128];
    snprintf(repl, sizeof(repl), "@%s", display);
    replace_placeholder(text_ptr, key, repl);

    if (mention_oid && mention_oid[0]) {
        *map_off += snprintf(mention_map + *map_off, map_cap - *map_off,
            "%s%s=%s", *map_off > 0 ? ", " : "", display, mention_oid);
    }
}

static bool process_group_mentions(cJSON* msg, char** text_ptr)
{
    cJSON* chat_type_j = cJSON_GetObjectItem(msg, "chat_type");
    const char* chat_type = cJSON_IsString(chat_type_j)
        ? chat_type_j->valuestring : "p2p";

    if (strcmp(chat_type, "group") != 0) {
        return false;
    }

    cJSON* mentions = cJSON_GetObjectItem(msg, "mentions");
    if (!cJSON_IsArray(mentions) || cJSON_GetArraySize(mentions) == 0) {
        syslog(LOG_DEBUG, "[%s] Group msg without @mention, skipping\n", TAG);
        return true;
    }

    char mention_map[512];
    size_t map_off = 0;
    mention_map[0] = '\0';

    cJSON* m_item;
    cJSON_ArrayForEach(m_item, mentions) {
        apply_single_mention(m_item, text_ptr, mention_map,
            sizeof(mention_map), &map_off);
    }

    return finalize_mention_text(text_ptr, mention_map);
}

/* ── Main event processing function ────────────────────────────── */

void feishu_process_binary_frame(const uint8_t* data, int len, int32_t svc_id)
{
    fk_frame_t f;

    if (pb_decode_frame(data, len, &f) != 0) {
        syslog(LOG_WARNING, "[%s] pb_decode_frame failed\n", TAG);
        return;
    }

    /* Frame-level logging disabled to reduce noise */

    if (handle_control_frame(&f)) {
        return;
    }

    if (strcmp(f.h_type, "event") != 0 || !f.payload || f.payload_len <= 0) {
        return;
    }

    send_data_ack(&f);

    char* payload_str = malloc((size_t)f.payload_len + 1);
    if (!payload_str) {
        syslog(LOG_ERR, "[%s] OOM for payload_str (%d bytes)\n", TAG, f.payload_len);
        return;
    }

    memcpy(payload_str, f.payload, f.payload_len);
    payload_str[f.payload_len] = '\0';

    if (payload_str[0] != '{') {
        syslog(LOG_WARNING, "[%s] payload not JSON (starts with 0x%02x), dropping\n",
            TAG, (unsigned char)payload_str[0]);
        free(payload_str);
        return;
    }

    cJSON* root = cJSON_Parse(payload_str);
    if (!root) {
        syslog(LOG_WARNING, "[%s] payload JSON parse failed\n", TAG);
        free(payload_str);
        return;
    }

    cJSON* hdr = cJSON_GetObjectItem(root, "header");
    cJSON* ev = cJSON_GetObjectItem(root, "event");
    char* text = NULL;

    /* Deduplicate by event_id */
    if (cJSON_IsObject(hdr)) {
        cJSON* eid = cJSON_GetObjectItem(hdr, "event_id");
        if (cJSON_IsString(eid) && eid->valuestring[0]) {
            if (dedup_check_and_record(eid->valuestring)) {
                goto cleanup; /* Duplicate event, skip silently */
            }
        }
    }

    /* Check event type */
    {
        const char* event_type = "";
        if (cJSON_IsObject(hdr)) {
            cJSON* et = cJSON_GetObjectItem(hdr, "event_type");
            if (cJSON_IsString(et)) {
                event_type = et->valuestring;
            }
        }
        if (strcmp(event_type, "im.message.receive_v1") != 0) {
            syslog(LOG_DEBUG, "[%s] ignored event_type=%s\n", TAG, event_type);
            goto cleanup;
        }
    }

    if (!cJSON_IsObject(ev)) {
        goto cleanup;
    }

    if (filter_sender(ev)) {
        goto cleanup;
    }

    /* Check if sender is a bot (for rate limiting) */
    bool is_bot_sender = false;
    {
        cJSON* sender = cJSON_GetObjectItem(ev, "sender");
        if (cJSON_IsObject(sender)) {
            cJSON* sender_type = cJSON_GetObjectItem(sender, "sender_type");
            if (cJSON_IsString(sender_type) &&
                strcmp(sender_type->valuestring, "app") == 0) {
                is_bot_sender = true;
            }
        }
    }

    cJSON* msg = cJSON_GetObjectItem(ev, "message");
    if (!cJSON_IsObject(msg)) {
        goto cleanup;
    }

    cJSON* chat_id_j = cJSON_GetObjectItem(msg, "chat_id");
    cJSON* content_j = cJSON_GetObjectItem(msg, "content");
    cJSON* msg_type_j = cJSON_GetObjectItem(msg, "message_type");
    const char* msg_type = cJSON_IsString(msg_type_j) ? msg_type_j->valuestring : "text";

    syslog(LOG_DEBUG, "[%s] msg_type=%s\n", TAG, msg_type);

    /* Bot-to-bot rate limiting */
    if (is_bot_sender && cJSON_IsString(chat_id_j)) {
        if (bot_rate_limited(chat_id_j->valuestring)) {
            goto cleanup;
        }
    }

    if (filter_stale_message(msg)) {
        goto cleanup;
    }

    if (!cJSON_IsString(chat_id_j)) {
        goto cleanup;
    }

    if (!cJSON_IsString(content_j) && strcmp(msg_type, "image") != 0) {
        goto cleanup;
    }

    bool is_text = (strcmp(msg_type, "text") == 0);
    bool is_post = (strcmp(msg_type, "post") == 0);
    bool is_image = (strcmp(msg_type, "image") == 0);

    if (!is_text && !is_post && !is_image) {
        syslog(LOG_DEBUG, "[%s] Unsupported msg_type=%s, skipping\n", TAG, msg_type);
        goto cleanup;
    }

    if (is_image) {
        handle_image_message(msg, chat_id_j->valuestring, content_j);
        goto cleanup;
    }

    /* Text/Post message handling */
    {
        char post_image_key[128] = { 0 };
        cJSON* cj = cJSON_Parse(content_j->valuestring);

        if (!cj) {
            syslog(LOG_WARNING, "[%s] content JSON parse failed, dropping\n", TAG);
            goto cleanup;
        }

        if (is_post) {
            text = extract_post_text(cj, post_image_key, sizeof(post_image_key));
        } else {
            cJSON* ti = cJSON_GetObjectItem(cj, "text");
            if (cJSON_IsString(ti)) {
                text = strdup(ti->valuestring);
            }
        }
        cJSON_Delete(cj);

        if (post_image_key[0] && text) {
            download_post_image(post_image_key, msg, &text);
        }

        if (!text || !text[0]) {
            free(text);
            text = NULL;
            goto cleanup;
        }

        char* trimmed = trim_unicode_whitespace(text);
        if (!*trimmed) {
            syslog(LOG_DEBUG, "[%s] Empty text after trim, skipping\n", TAG);
            free(text);
            text = NULL;
            goto cleanup;
        }
        if (trimmed != text) {
            memmove(text, trimmed, strlen(trimmed) + 1);
        }

        if (!validate_text_content(text)) {
            free(text);
            text = NULL;
            goto cleanup;
        }

        if (process_group_mentions(msg, &text)) {
            text = NULL;
            goto cleanup;
        }

        cJSON* msg_id_j = cJSON_GetObjectItem(msg, "message_id");
        if (cJSON_IsString(msg_id_j) && msgid_dedup_check(msg_id_j->valuestring)) {
            syslog(LOG_WARNING, "[%s] Duplicate message_id %s, skipping\n",
                TAG, msg_id_j->valuestring);
            free(text);
            text = NULL;
            goto cleanup;
        }

        if (!post_image_key[0] && content_dedup_check(chat_id_j->valuestring, text)) {
            free(text);
            text = NULL;
            goto cleanup;
        }

        /* Final empty check */
        {
            const char* p = text;
            while (*p && ((unsigned char)*p <= 0x20)) {
                p++;
            }
            if (!*p) {
                syslog(LOG_WARNING, "[%s] Final check: empty content, dropping\n", TAG);
                free(text);
                text = NULL;
                goto cleanup;
            }
        }

        syslog(LOG_INFO, "[%s] MSG from [%.24s] len=%zu\n", TAG,
            chat_id_j->valuestring, strlen(text));

        agent_msg_t m = { 0 };
        strncpy(m.channel, AGENT_CHAN_FEISHU, sizeof(m.channel) - 1);
        strncpy(m.chat_id, chat_id_j->valuestring, sizeof(m.chat_id) - 1);
        m.content = text;
        message_bus_push_inbound(&m);
        text = NULL; /* ownership transferred */
    }

cleanup:
    free(text);
    free(payload_str);
    cJSON_Delete(root);
}
