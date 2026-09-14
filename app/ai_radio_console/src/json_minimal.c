#include "json_minimal.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

static const char *skip_ws(const char *p)
{
    while (p && *p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    return p;
}

static const char *find_key(const char *json, const char *key)
{
    if (!json || !key) return NULL;
    size_t klen = strlen(key);
    const char *p = json;
    while ((p = strchr(p, '"')) != NULL) {
        p++;
        if (strncmp(p, key, klen) == 0 && p[klen] == '"') {
            const char *colon = strchr(p + klen + 1, ':');
            if (colon) return colon + 1;
        }
        p = strchr(p, '"');
        if (p) p++;
    }
    return NULL;
}

int json_extract_string(const char *json, const char *key, char *out, size_t max_len)
{
    if (!json || !key || !out || max_len == 0) return -1;
    const char *val = find_key(json, key);
    if (!val) return -1;
    val = skip_ws(val);
    if (*val != '"') return -1;
    val++;
    size_t i = 0;
    while (*val && *val != '"' && i < max_len - 1) {
        if (*val == '\\' && *(val + 1)) {
            val++;
            switch (*val) {
                case 'n': out[i++] = '\n'; break;
                case 't': out[i++] = '\t'; break;
                case 'r': out[i++] = '\r'; break;
                case '"': out[i++] = '"'; break;
                case '\\': out[i++] = '\\'; break;
                default: out[i++] = *val; break;
            }
        } else {
            out[i++] = *val;
        }
        val++;
    }
    out[i] = '\0';
    return 0;
}

int json_extract_float(const char *json, const char *key, float *out)
{
    char buf[64];
    if (json_extract_string(json, key, buf, sizeof(buf)) != 0) {
        const char *val = find_key(json, key);
        if (!val) return -1;
        val = skip_ws(val);
        *out = strtof(val, NULL);
        return 0;
    }
    *out = strtof(buf, NULL);
    return 0;
}

int json_extract_int(const char *json, const char *key, int *out)
{
    char buf[32];
    if (json_extract_string(json, key, buf, sizeof(buf)) != 0) {
        const char *val = find_key(json, key);
        if (!val) return -1;
        val = skip_ws(val);
        *out = atoi(val);
        return 0;
    }
    *out = atoi(buf);
    return 0;
}

int json_extract_bool(const char *json, const char *key, bool *out)
{
    const char *val = find_key(json, key);
    if (!val) return -1;
    val = skip_ws(val);
    if (strncmp(val, "true", 4) == 0) *out = true;
    else if (strncmp(val, "false", 5) == 0) *out = false;
    else return -1;
    return 0;
}

const char *json_find_array_start(const char *json, const char *key)
{
    const char *val = find_key(json, key);
    if (!val) return NULL;
    val = skip_ws(val);
    if (*val == '[') return val + 1;
    return NULL;
}

int json_array_count(const char *arr_start)
{
    if (!arr_start) return 0;
    int count = 0;
    int depth = 1;
    bool in_str = false;
    const char *p = arr_start;
    while (*p && depth > 0) {
        if (*p == '"' && (p == arr_start || *(p-1) != '\\')) in_str = !in_str;
        if (!in_str) {
            if (*p == '[') depth++;
            else if (*p == ']') depth--;
            else if (*p == '{' ) {
                depth++;
                while (*p && depth > 0) {
                    p++;
                    if (*p == '"' && *(p-1) != '\\') in_str = !in_str;
                    if (!in_str) {
                        if (*p == '{') depth++;
                        else if (*p == '}') depth--;
                    }
                }
                if (depth == 1) count++;
            }
            else if (*p == ',' && depth == 1) count++;
        }
        p++;
    }
    return count + 1;
}

static void json_escape(char *dst, size_t *dst_len, const char *src)
{
    size_t di = *dst_len;
    while (*src) {
        if (*src == '"' || *src == '\\') {
            dst[di++] = '\\';
            dst[di++] = *src;
        } else if (*src == '\n') {
            dst[di++] = '\\';
            dst[di++] = 'n';
        } else if (*src == '\r') {
            dst[di++] = '\\';
            dst[di++] = 'r';
        } else if (*src == '\t') {
            dst[di++] = '\\';
            dst[di++] = 't';
        } else {
            dst[di++] = *src;
        }
        src++;
    }
    *dst_len = di;
}

int json_build_chat_completion(char *buf, size_t max_len, const char *api_key,
                                const char *model, const char *system_prompt,
                                const char *user_message, bool stream)
{
    if (!buf || max_len == 0) return -1;
    int n = snprintf(buf, max_len,
        "{"
        "\"model\":\"%s\","
        "\"messages\":["
        "{\"role\":\"system\",\"content\":\"", model);
    size_t pos = (size_t)n;
    if (system_prompt) json_escape(buf, &pos, system_prompt);
    n = snprintf(buf + pos, max_len - pos, "\"},{\"role\":\"user\",\"content\":\"");
    pos += (size_t)n;
    if (user_message) json_escape(buf, &pos, user_message);
    n = snprintf(buf + pos, max_len - pos, "\"}],"
        "\"temperature\":0.1,\"max_tokens\":1024,\"stream\":%s"
        "}", stream ? "true" : "false");
    pos += (size_t)n;
    if (pos >= max_len) return -1;
    return 0;
}

int json_build_asr_multipart(char *buf, size_t *buf_len, const char *boundary,
                              const char *api_key, const char *model,
                              const uint8_t *wav_data, size_t wav_size)
{
    (void)api_key;
    size_t pos = 0;
    int n;

    n = snprintf(buf + pos, *buf_len - pos,
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"model\"\r\n\r\n"
        "%s\r\n", boundary, model);
    pos += (size_t)n;

    n = snprintf(buf + pos, *buf_len - pos,
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n"
        "Content-Type: audio/wav\r\n\r\n", boundary);
    pos += (size_t)n;

    if (pos + wav_size + 256 > *buf_len) return -1;

    memcpy(buf + pos, wav_data, wav_size);
    pos += wav_size;

    n = snprintf(buf + pos, *buf_len - pos, "\r\n--%s--\r\n", boundary);
    pos += (size_t)n;

    *buf_len = pos;
    return 0;
}
