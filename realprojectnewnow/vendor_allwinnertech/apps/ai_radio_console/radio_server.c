/****************************************************************************
 * apps/vendor/allwinnertech/apps/ai_radio_console/radio_server.c
 *
 * Minimal REST + WebSocket server compatible with com.agent.coapp.
 * Serves:
 *   GET    /api/config
 *   PUT    /api/config
 *   GET    /api/skills
 *   POST   /api/skills
 *   DELETE /api/skills/{name}
 *   GET    /api/logs
 *   WS     /ws/chat
 *
 * Config is stored in /data/agent/config/radio_console.json.
 * Skills are read/written under /data/agent/skills/.
 * Logs are read from /data/agent/memory/radio/.
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <netdb.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include "radio_server.h"
#include "ptt_control.h"

/* Default to 28790 so we do not conflict with ai_agent's server on 28789.
 * The com.agent.coapp primary API is provided by ai_agent; this server
 * exposes a radio-console specific view that can be pointed to port 28790. */
#ifndef CONFIG_AI_RADIO_CONSOLE_SERVER_PORT
#  define CONFIG_AI_RADIO_CONSOLE_SERVER_PORT 28790
#endif

#define RADIO_SERVER_PORT   CONFIG_AI_RADIO_CONSOLE_SERVER_PORT
#define MAX_WS_CLIENTS      4
/* Large enough for handlers that build JSON responses on the stack.
 * handle_skills_get uses ~8KB out buffer; leave margin for call frames. */
#define WS_CLIENT_STACK     32768
#define ACCEPT_STACK        4096
#define PEEK_BUFSIZE        2048
#define MAX_BODY_LEN        (64 * 1024)
#define MAX_FRAME_LEN       2048
#define MAX_PATH            256
#define WS_GUID             "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

/* Radio-console specific config. The primary ai_agent API (and its
 * config.json) lives on port 28789; this server exposes a radio view
 * on port 28790 and keeps its own flat configuration. */
#define CONFIG_FILE         "/data/agent/config/radio_console.json"
#define SKILLS_DIR          "/data/agent/skills/"
#define LOGS_DIR            "/data/agent/memory/radio/"

static const char *TAG = "radio_server";

/* ── Tiny SHA1 (public domain, Steve Reid) ────────────────────── */

typedef struct {
    uint32_t state[5];
    uint32_t count[2];
    uint8_t  buffer[64];
} sha1_ctx_t;

#define rol(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))

static void sha1_transform(uint32_t state[5], const uint8_t buffer[64])
{
    uint32_t a, b, c, d, e;
    uint32_t block[80];

    for (int i = 0; i < 16; i++) {
        block[i] = ((uint32_t)buffer[i * 4 + 0] << 24)
                 | ((uint32_t)buffer[i * 4 + 1] << 16)
                 | ((uint32_t)buffer[i * 4 + 2] << 8)
                 | ((uint32_t)buffer[i * 4 + 3]);
    }
    for (int i = 16; i < 80; i++) {
        block[i] = rol(block[i - 3] ^ block[i - 8] ^ block[i - 14] ^ block[i - 16], 1);
    }

    a = state[0]; b = state[1]; c = state[2]; d = state[3]; e = state[4];

    for (int i = 0; i < 20; i++) {
        uint32_t t = rol(a, 5) + ((b & c) | ((~b) & d)) + e + block[i] + 0x5A827999;
        e = d; d = c; c = rol(b, 30); b = a; a = t;
    }
    for (int i = 20; i < 40; i++) {
        uint32_t t = rol(a, 5) + (b ^ c ^ d) + e + block[i] + 0x6ED9EBA1;
        e = d; d = c; c = rol(b, 30); b = a; a = t;
    }
    for (int i = 40; i < 60; i++) {
        uint32_t t = rol(a, 5) + ((b & c) | (b & d) | (c & d)) + e + block[i] + 0x8F1BBCDC;
        e = d; d = c; c = rol(b, 30); b = a; a = t;
    }
    for (int i = 60; i < 80; i++) {
        uint32_t t = rol(a, 5) + (b ^ c ^ d) + e + block[i] + 0xCA62C1D6;
        e = d; d = c; c = rol(b, 30); b = a; a = t;
    }

    state[0] += a; state[1] += b; state[2] += c; state[3] += d; state[4] += e;
}

static void sha1_init(sha1_ctx_t *ctx)
{
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
    ctx->state[4] = 0xC3D2E1F0;
    ctx->count[0] = ctx->count[1] = 0;
}

static void sha1_update(sha1_ctx_t *ctx, const uint8_t *data, size_t len)
{
    size_t i, j;
    j = (ctx->count[0] >> 3) & 63;
    ctx->count[0] += (uint32_t)(len << 3);
    if (ctx->count[0] < (uint32_t)(len << 3)) ctx->count[1]++;
    ctx->count[1] += (uint32_t)(len >> 29);

    if ((j + len) > 63) {
        memcpy(&ctx->buffer[j], data, (i = 64 - j));
        sha1_transform(ctx->state, ctx->buffer);
        for (; i + 63 < len; i += 64) {
            sha1_transform(ctx->state, &data[i]);
        }
        j = 0;
    } else {
        i = 0;
    }
    memcpy(&ctx->buffer[j], &data[i], len - i);
}

static void sha1_final(sha1_ctx_t *ctx, uint8_t digest[20])
{
    uint32_t i;
    uint8_t finalcount[8];

    for (i = 0; i < 8; i++) {
        finalcount[i] = (uint8_t)((ctx->count[(i >= 4) ? 1 : 0] >> ((3 - (i & 3)) * 8)) & 255);
    }
    sha1_update(ctx, (const uint8_t *)"\x80", 1);
    while ((ctx->count[0] & 504) != 448) {
        sha1_update(ctx, (const uint8_t *)"\0", 1);
    }
    sha1_update(ctx, finalcount, 8);
    for (i = 0; i < 20; i++) {
        digest[i] = (uint8_t)((ctx->state[i >> 2] >> ((3 - (i & 3)) * 8)) & 255);
    }
}

/* ── Base64 encoder ───────────────────────────────────────────── */

static const char b64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int base64_encode(const uint8_t *in, size_t in_len, char *out, size_t out_size)
{
    size_t i, j;
    for (i = 0, j = 0; i < in_len; i += 3, j += 4) {
        if (j + 4 >= out_size) return -1;
        uint8_t b0 = in[i];
        uint8_t b1 = (i + 1 < in_len) ? in[i + 1] : 0;
        uint8_t b2 = (i + 2 < in_len) ? in[i + 2] : 0;

        out[j]     = b64_table[b0 >> 2];
        out[j + 1] = b64_table[((b0 & 0x03) << 4) | (b1 >> 4)];
        out[j + 2] = (i + 1 < in_len) ? b64_table[((b1 & 0x0F) << 2) | (b2 >> 6)] : '=';
        out[j + 3] = (i + 2 < in_len) ? b64_table[b2 & 0x3F] : '=';
    }
    out[j] = '\0';
    return (int)j;
}

/* ── Server state ─────────────────────────────────────────────── */

typedef struct {
    int fd;
    bool active;
    pthread_mutex_t mtx;
} ws_client_t;

static ws_client_t s_clients[MAX_WS_CLIENTS];
static pthread_mutex_t s_clients_lock = PTHREAD_MUTEX_INITIALIZER;

static int s_listen_fd = -1;
static volatile bool s_running = false;

/* ── Helpers ──────────────────────────────────────────────────── */

static int ensure_dir(const char *path)
{
    int ret = mkdir(path, 0755);
    if (ret < 0 && errno != EEXIST) {
        return -errno;
    }
    return 0;
}

static void send_all(int fd, const char *data, size_t len)
{
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, data + sent, len - sent, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (n == 0) break;
        sent += n;
    }
}

/* Robust receive: retry on EINTR, allow short reads up to timeout. */
static ssize_t recv_all(int fd, void *buf, size_t len, int flags)
{
    size_t received = 0;
    while (received < len) {
        ssize_t n = recv(fd, (char *)buf + received, len - received, flags);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return (ssize_t)received;
        received += n;
    }
    return (ssize_t)received;
}

static const char *http_status_text(int code)
{
    switch (code) {
    case 200: return "OK";
    case 400: return "Bad Request";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 500: return "Internal Server Error";
    default:  return "Error";
    }
}

static void http_response(int fd, int code, const char *body)
{
    char hdr[256];
    int hlen = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: application/json; charset=utf-8\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n\r\n",
        code, http_status_text(code), body ? strlen(body) : 0);
    send_all(fd, hdr, hlen);
    if (body) send_all(fd, body, strlen(body));
}

static const char *find_header_end(const char *buf)
{
    return strstr(buf, "\r\n\r\n");
}

/* Forward declaration: full definition follows below. */
static char *radio_strcasestr(const char *haystack, const char *needle);

static char *read_body(int fd, const char *buf, int buf_len)
{
    const char *hdr_end = find_header_end(buf);
    if (!hdr_end) return NULL;
    hdr_end += 4;

    const char *cl = radio_strcasestr(buf, "\r\nContent-Length: ");
    int content_len = cl ? atoi(cl + 18) : 0;

    int already = buf_len - (int)(hdr_end - buf);
    if (already < 0) already = 0;

    if (content_len <= 0 || content_len > MAX_BODY_LEN) {
        if (already <= 0) return NULL;
        content_len = already;
    }

    char *body = malloc((size_t)content_len + 1);
    if (!body) return NULL;

    if (already > content_len) already = content_len;
    if (already > 0) memcpy(body, hdr_end, (size_t)already);

    int remaining = content_len - already;
    int offset = already;
    while (remaining > 0) {
        ssize_t n = recv_all(fd, body + offset, (size_t)remaining, 0);
        if (n <= 0) {
            free(body);
            return NULL;
        }
        offset += (int)n;
        remaining -= (int)n;
    }
    body[content_len] = '\0';
    return body;
}

/* Find the next unescaped quote ('"' or '\''). Returns NULL if none. */
static const char *find_unescaped_quote(const char *s, char quote)
{
    for (const char *p = s; *p; p++) {
        if (*p == '\\' && *(p + 1)) {
            p++;
        } else if (*p == quote) {
            return p;
        }
    }
    return NULL;
}

static void url_decode(const char *src, char *dst, size_t dst_size)
{
    size_t di = 0;
    for (size_t si = 0; src[si] && di < dst_size - 1; si++) {
        if (src[si] == '%' && src[si + 1] && src[si + 2]) {
            char hex[3] = { src[si + 1], src[si + 2], 0 };
            dst[di++] = (char)strtol(hex, NULL, 16);
            si += 2;
        } else if (src[si] == '+') {
            dst[di++] = ' ';
        } else {
            dst[di++] = src[si];
        }
    }
    dst[di] = '\0';
}

/* Some NuttX toolchains do not provide strcasestr; provide a tiny fallback. */
static char *radio_strcasestr(const char *haystack, const char *needle)
{
#ifdef HAVE_STRCASESTR
    return strcasestr(haystack, needle);
#else
    size_t nlen = strlen(needle);
    if (nlen == 0) return (char *)haystack;
    for (const char *p = haystack; *p; p++) {
        if (strncasecmp(p, needle, nlen) == 0) {
            return (char *)p;
        }
    }
    return NULL;
#endif
}

/* Simple JSON string escaping: dst must be 2*len+1 minimum. */
static void json_escape(const char *src, char *dst, size_t dst_size)
{
    size_t j = 0;
    for (size_t i = 0; src[i] && j < dst_size - 1; i++) {
        char c = src[i];
        if (c == '"') {
            if (j + 2 >= dst_size) break;
            dst[j++] = '\\'; dst[j++] = '"';
        } else if (c == '\\') {
            if (j + 2 >= dst_size) break;
            dst[j++] = '\\'; dst[j++] = '\\';
        } else if (c == '\n') {
            if (j + 2 >= dst_size) break;
            dst[j++] = '\\'; dst[j++] = 'n';
        } else if (c == '\r') {
            if (j + 2 >= dst_size) break;
            dst[j++] = '\\'; dst[j++] = 'r';
        } else if ((unsigned char)c < 0x20) {
            continue;
        } else {
            dst[j++] = c;
        }
    }
    dst[j] = '\0';
}

/* Reverse of json_escape: convert \" → ", \\ → \, etc. */
static void json_unescape(const char *src, char *dst, size_t dst_size)
{
    size_t j = 0;
    for (size_t i = 0; src[i] && j < dst_size - 1; i++) {
        if (src[i] == '\\' && src[i + 1]) {
            switch (src[i + 1]) {
            case '"':  dst[j++] = '"';  i++; break;
            case '\\': dst[j++] = '\\'; i++; break;
            case '/':  dst[j++] = '/';  i++; break;
            case 'b':  dst[j++] = '\b'; i++; break;
            case 'f':  dst[j++] = '\f'; i++; break;
            case 'n':  dst[j++] = '\n'; i++; break;
            case 'r':  dst[j++] = '\r'; i++; break;
            case 't':  dst[j++] = '\t'; i++; break;
            default:   dst[j++] = src[i]; break;
            }
        } else {
            dst[j++] = src[i];
        }
    }
    dst[j] = '\0';
}

/* ── Config handlers ──────────────────────────────────────────── */

static char *read_file(const char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) return NULL;

    struct stat st;
    if (fstat(fd, &st) < 0) {
        close(fd);
        return NULL;
    }
    if (st.st_size > MAX_BODY_LEN) {
        close(fd);
        return NULL;
    }

    char *buf = malloc(st.st_size + 1);
    if (!buf) {
        close(fd);
        return NULL;
    }
    ssize_t n = read(fd, buf, st.st_size);
    close(fd);
    if (n != st.st_size) {
        free(buf);
        return NULL;
    }
    buf[n] = '\0';
    return buf;
}

static int write_file(const char *path, const char *data)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -errno;
    size_t len = strlen(data);
    if (write(fd, data, len) != (ssize_t)len) {
        close(fd);
        return -EIO;
    }
    close(fd);
    return 0;
}

/* Mask secrets like ai_agent does. */
static void mask_secret(const char *in, char *out, size_t out_size)
{
    size_t len = in ? strlen(in) : 0;
    if (len == 0) {
        out[0] = '\0';
    } else if (len <= 4) {
        snprintf(out, out_size, "****");
    } else {
        snprintf(out, out_size, "%c%c****%c%c",
                 in[0], in[1], in[len - 2], in[len - 1]);
    }
}

static bool is_secret_key(const char *key)
{
    static const char *secrets[] = {
        "api_key", "tavily_key", "volc_appkey",
        "volc_token", "volc_api_key", "asr_api_key"
    };
    for (size_t i = 0; i < sizeof(secrets) / sizeof(secrets[0]); i++) {
        if (strcmp(key, secrets[i]) == 0) return true;
    }
    return false;
}

/*
 * Very small JSON object parser for flat {"key":"value",...} maps.
 * Supports escaped quotes (\") and common escape sequences.
 * Returns pointer to value (in out) or NULL. No nesting supported.
 */
static const char *json_get_string(const char *json, const char *key, char *out, size_t out_size)
{
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(json, pattern);
    if (!p) return NULL;
    p += strlen(pattern);
    while (*p == ' ' || *p == '\t') p++;
    if (*p != ':') return NULL;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    char quote = *p;
    if (quote != '"' && quote != '\'') return NULL;
    p++;

    size_t j = 0;
    for (; *p && j < out_size - 1; p++) {
        if (*p == quote) {
            break;
        }
        if (*p == '\\' && *(p + 1)) {
            p++;
            switch (*p) {
            case '"':  out[j++] = '"';  break;
            case '\\': out[j++] = '\\'; break;
            case '/':  out[j++] = '/';  break;
            case 'b':  out[j++] = '\b'; break;
            case 'f':  out[j++] = '\f'; break;
            case 'n':  out[j++] = '\n'; break;
            case 'r':  out[j++] = '\r'; break;
            case 't':  out[j++] = '\t'; break;
            case 'u':
                /* Unicode escape \uXXXX: consume 4 hex digits and keep the escape
                 * as-is. UTF-8 strings from Gson/ASR are normally not escaped. */
                if (j + 6 < out_size) {
                    out[j++] = '\\';
                    out[j++] = 'u';
                    for (int k = 0; k < 4 && *(p + 1); k++) {
                        out[j++] = *++p;
                    }
                } else {
                    p += 4;
                }
                break;
            default:
                out[j++] = *p;
                break;
            }
        } else {
            out[j++] = *p;
        }
    }
    out[j] = '\0';
    if (*p != quote) {
        return NULL;
    }
    return out;
}

#define CONFIG_OUT_SIZE 4096

/* Build a flat JSON object from the config file.
 * The file is expected to already be a JSON object with string values. */
static void handle_config_get(int fd)
{
    char *json = read_file(CONFIG_FILE);
    if (!json) {
        http_response(fd, 200, "{}");
        return;
    }

    /* Rewrite with secrets masked. */
    char *out = malloc(CONFIG_OUT_SIZE);
    if (!out) {
        free(json);
        http_response(fd, 500, "{\"error\":\"out of memory\"}");
        return;
    }

    size_t o = 0;
    out[o++] = '{';

    const char *p = json;
    bool first = true;
    while ((p = strchr(p, '"')) != NULL) {
        const char *key_start = p + 1;
        const char *key_end = strchr(key_start, '"');
        if (!key_end) break;

        char key[64];
        size_t klen = (size_t)(key_end - key_start);
        if (klen >= sizeof(key)) klen = sizeof(key) - 1;
        memcpy(key, key_start, klen);
        key[klen] = '\0';

        const char *colon = key_end + 1;
        while (*colon == ' ' || *colon == '\t') colon++;
        if (*colon != ':') { p = key_end + 1; continue; }
        colon++;
        while (*colon == ' ' || *colon == '\t') colon++;
        char quote = *colon;
        if (quote != '"') { p = key_end + 1; continue; }
        const char *val_start = colon + 1;
        const char *val_end = find_unescaped_quote(val_start, quote);
        if (!val_end) break;

        char val[512];
        size_t vlen = (size_t)(val_end - val_start);
        if (vlen >= sizeof(val)) vlen = sizeof(val) - 1;
        memcpy(val, val_start, vlen);
        val[vlen] = '\0';

        char raw[512];
        json_unescape(val, raw, sizeof(raw));

        char display[64];
        if (is_secret_key(key)) {
            mask_secret(raw, display, sizeof(display));
        } else {
            snprintf(display, sizeof(display), "%s", raw);
        }
        char escaped[128];
        json_escape(display, escaped, sizeof(escaped));

        int n = snprintf(out + o, CONFIG_OUT_SIZE - o, "%s\"%s\":\"%s\"",
                         first ? "" : ",", key, escaped);
        if (n < 0 || (size_t)n >= CONFIG_OUT_SIZE - o) break;
        o += n;
        first = false;
        p = val_end + 1;
    }

    free(json);
    if (o + 2 >= CONFIG_OUT_SIZE) {
        free(out);
        http_response(fd, 500, "{\"error\":\"config too large\"}");
        return;
    }
    out[o++] = '}';
    out[o] = '\0';
    http_response(fd, 200, out);
    free(out);
}

static void handle_config_put(int fd, const char *body)
{
    if (!body || *body == '\0') {
        http_response(fd, 400, "{\"error\":\"empty body\"}");
        return;
    }

    char *old = read_file(CONFIG_FILE);
    if (!old) {
        old = strdup("{}");
        if (!old) {
            http_response(fd, 500, "{\"error\":\"out of memory\"}");
            return;
        }
    }

    /* Merge: for each key in body, update old. */
    char merged[4096];
    if (old[0] == '\0') {
        snprintf(merged, sizeof(merged), "{}");
    } else {
        snprintf(merged, sizeof(merged), "%s", old);
    }
    free(old);

    const char *p = body;
    while ((p = strchr(p, '"')) != NULL) {
        const char *key_start = p + 1;
        const char *key_end = strchr(key_start, '"');
        if (!key_end) break;
        char key[64];
        size_t klen = (size_t)(key_end - key_start);
        if (klen >= sizeof(key)) klen = sizeof(key) - 1;
        memcpy(key, key_start, klen);
        key[klen] = '\0';

        const char *colon = key_end + 1;
        while (*colon == ' ' || *colon == '\t') colon++;
        if (*colon != ':') { p = key_end + 1; continue; }
        colon++;
        while (*colon == ' ' || *colon == '\t') colon++;
        if (*colon != '"') { p = key_end + 1; continue; }
        const char *val_start = colon + 1;
        const char *val_end = find_unescaped_quote(val_start, '"');
        if (!val_end) break;
        char val[512];
        size_t vlen = (size_t)(val_end - val_start);
        if (vlen >= sizeof(val)) vlen = sizeof(val) - 1;
        memcpy(val, val_start, vlen);
        val[vlen] = '\0';

        char raw[512];
        json_unescape(val, raw, sizeof(raw));

        if (is_secret_key(key) && strstr(raw, "****")) {
            /* Don't overwrite secret with masked value from App. */
            p = val_end + 1;
            continue;
        }

        /* Simple replace / insert. */
        char pattern[80];
        snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
        char *pos = strstr(merged, pattern);
        if (pos) {
            char *vpos = pos + strlen(pattern);
            char *end = (char *)find_unescaped_quote(vpos, '"');
            if (end) {
                char escaped[1024];
                json_escape(raw, escaped, sizeof(escaped));
                size_t escaped_len = strlen(escaped);
                const char *tail = end + 1;
                size_t tail_len = strlen(tail);
                size_t vpos_off = (size_t)(vpos - merged);
                if (vpos_off + escaped_len + 1 + tail_len + 1 > sizeof(merged)) {
                    http_response(fd, 500, "{\"error\":\"config too large\"}");
                    return;
                }
                *vpos = '\0';
                memmove(merged + vpos_off + escaped_len + 1, tail, tail_len + 1);
                memcpy(merged + vpos_off, escaped, escaped_len);
                merged[vpos_off + escaped_len] = '"';
            }
        } else {
            /* Insert before closing brace. */
            size_t len = strlen(merged);
            if (len > 1 && merged[len - 1] == '}') {
                merged[len - 1] = '\0';
                char escaped[1024];
                json_escape(raw, escaped, sizeof(escaped));
                snprintf(merged + len - 1, sizeof(merged) - (len - 1),
                         "%s\"%s\":\"%s\"}",
                         (merged[len - 2] == '{') ? "" : ",",
                         key, escaped);
            }
        }
        p = val_end + 1;
    }

    if (write_file(CONFIG_FILE, merged) < 0) {
        http_response(fd, 500, "{\"error\":\"write failed\"}");
        return;
    }
    http_response(fd, 200, "{\"ok\":true}");
}

/* ── Skills handlers ──────────────────────────────────────────── */

#define SKILLS_OUT_SIZE 8192

static void handle_skills_get(int fd)
{
    char *out = malloc(SKILLS_OUT_SIZE);
    if (!out) {
        http_response(fd, 500, "{\"error\":\"out of memory\"}");
        return;
    }

    size_t o = 0;
    o += snprintf(out + o, SKILLS_OUT_SIZE - o, "{\"skills\":[");

    DIR *dir = opendir(SKILLS_DIR);
    bool first = true;
    if (dir) {
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            size_t nlen = strlen(ent->d_name);
            if (nlen < 4 || strcmp(ent->d_name + nlen - 3, ".md") != 0) continue;

            char path[MAX_PATH];
            snprintf(path, sizeof(path), "%s%s", SKILLS_DIR, ent->d_name);
            struct stat st;
            if (stat(path, &st) != 0) continue;

            char name[128];
            snprintf(name, sizeof(name), "%.*s", (int)(nlen - 3), ent->d_name);

            char mtime_str[32] = "";
            struct tm *tm_info = localtime(&st.st_mtime);
            if (tm_info) {
                strftime(mtime_str, sizeof(mtime_str),
                         "%Y-%m-%d %H:%M:%S", tm_info);
            }

            char *content = read_file(path);
            char desc[128] = "";
            if (content) {
                char *p = strstr(content, "description:");
                if (p) {
                    p += 12;
                    while (*p == ' ') p++;
                    char *nl = strchr(p, '\n');
                    if (nl) {
                        size_t dlen = (size_t)(nl - p);
                        if (dlen >= sizeof(desc)) dlen = sizeof(desc) - 1;
                        memcpy(desc, p, dlen);
                        desc[dlen] = '\0';
                    }
                }
                free(content);
            }

            char name_escaped[256];
            char desc_escaped[256];
            char mtime_escaped[64];
            json_escape(name, name_escaped, sizeof(name_escaped));
            json_escape(desc, desc_escaped, sizeof(desc_escaped));
            json_escape(mtime_str, mtime_escaped, sizeof(mtime_escaped));

            /* List view omits full content to keep responses small.
             * Use GET /api/skills/{name} to fetch the complete markdown. */
            int n = snprintf(out + o, SKILLS_OUT_SIZE - o,
                "%s{\"name\":\"%s\",\"description\":\"%s\",\"file\":\"%s\",\"size\":%ld,\"mtime\":\"%s\",\"content\":null}",
                first ? "" : ",", name_escaped, desc_escaped, ent->d_name, (long)st.st_size, mtime_escaped);
            if (n < 0 || (size_t)n >= SKILLS_OUT_SIZE - o) break;
            o += n;
            first = false;
        }
        closedir(dir);
    }

    o += snprintf(out + o, SKILLS_OUT_SIZE - o, "]}");
    http_response(fd, 200, out);
    free(out);
}

static void handle_skill_get(int fd, const char *name_enc)
{
    char name[128];
    url_decode(name_enc, name, sizeof(name));
    if (strchr(name, '/') || strchr(name, '\\') || strstr(name, "..")) {
        http_response(fd, 400, "{\"error\":\"invalid name\"}");
        return;
    }

    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s%s.md", SKILLS_DIR, name);
    char *content = read_file(path);
    if (!content) {
        http_response(fd, 404, "{\"error\":\"skill not found\"}");
        return;
    }

    char *escaped = malloc(2 * strlen(content) + 1);
    if (!escaped) {
        free(content);
        http_response(fd, 500, "{\"error\":\"out of memory\"}");
        return;
    }
    json_escape(content, escaped, 2 * strlen(content) + 1);

    char *out = malloc(strlen(escaped) + strlen(name) + 64);
    if (!out) {
        free(escaped);
        free(content);
        http_response(fd, 500, "{\"error\":\"out of memory\"}");
        return;
    }
    snprintf(out, strlen(escaped) + strlen(name) + 64,
             "{\"name\":\"%s\",\"content\":\"%s\"}", name, escaped);
    http_response(fd, 200, out);

    free(out);
    free(escaped);
    free(content);
}

static void handle_skills_post(int fd, const char *body)
{
    if (!body || *body == '\0') {
        http_response(fd, 400, "{\"error\":\"empty body\"}");
        return;
    }

    char name[128] = {0};
    char *content = malloc(MAX_BODY_LEN);
    if (!content) {
        http_response(fd, 500, "{\"error\":\"out of memory\"}");
        return;
    }

    if (!json_get_string(body, "name", name, sizeof(name)) ||
        !json_get_string(body, "content", content, MAX_BODY_LEN)) {
        free(content);
        http_response(fd, 400, "{\"error\":\"name and content required\"}");
        return;
    }

    if (strchr(name, '/') || strchr(name, '\\') || strstr(name, "..")) {
        free(content);
        http_response(fd, 400, "{\"error\":\"invalid name\"}");
        return;
    }

    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s%s.md", SKILLS_DIR, name);
    if (write_file(path, content) < 0) {
        free(content);
        http_response(fd, 500, "{\"error\":\"write failed\"}");
        return;
    }
    free(content);
    http_response(fd, 200, "{\"ok\":true}");
}

static void handle_skills_delete(int fd, const char *name_enc)
{
    char name[128];
    url_decode(name_enc, name, sizeof(name));
    if (strchr(name, '/') || strchr(name, '\\') || strstr(name, "..")) {
        http_response(fd, 400, "{\"error\":\"invalid name\"}");
        return;
    }
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s%s.md", SKILLS_DIR, name);
    if (unlink(path) != 0 && errno != ENOENT) {
        http_response(fd, 500, "{\"error\":\"delete failed\"}");
        return;
    }
    http_response(fd, 200, "{\"ok\":true}");
}

/* ── Logs handler ─────────────────────────────────────────────── */

#define LOGS_OUT_SIZE 16384

static void handle_logs_get(int fd)
{
    char *out = malloc(LOGS_OUT_SIZE);
    if (!out) {
        http_response(fd, 500, "{\"error\":\"out of memory\"}");
        return;
    }

    size_t o = 0;
    o += snprintf(out + o, LOGS_OUT_SIZE - o, "{\"logs\":[");

    DIR *dir = opendir(LOGS_DIR);
    bool first = true;
    if (dir) {
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            if (ent->d_name[0] == '.') continue;
            char path[MAX_PATH];
            snprintf(path, sizeof(path), "%s%s", LOGS_DIR, ent->d_name);
            char *content = read_file(path);
            if (!content) continue;

            /* Return the whole file as one log entry, truncated to keep the
             * JSON response within bounds. */
            char line[2048];
            size_t clen = strlen(content);
            if (clen >= sizeof(line)) clen = sizeof(line) - 1;
            memcpy(line, content, clen);
            line[clen] = '\0';
            free(content);

            char escaped[4096];
            json_escape(line, escaped, sizeof(escaped));
            int n = snprintf(out + o, LOGS_OUT_SIZE - o, "%s\"%s\"",
                             first ? "" : ",", escaped);
            if (n < 0 || (size_t)n >= LOGS_OUT_SIZE - o) break;
            o += n;
            first = false;
        }
        closedir(dir);
    }

    o += snprintf(out + o, LOGS_OUT_SIZE - o, "]}");
    http_response(fd, 200, out);
    free(out);
}

/* ── PTT helpers ──────────────────────────────────────────────── */

static void ptt_action_start(void)
{
    ptt_key();
    radio_server_broadcast_status("ptt_keyed");
}

static void ptt_action_stop(void)
{
    ptt_unkey();
    radio_server_broadcast_status("ptt_unkeyed");
}

static void ptt_handle_action(const char *action)
{
    if (!action) return;
    if (strcmp(action, "start") == 0 || strcmp(action, "key") == 0 ||
        strcmp(action, "press") == 0 || strcmp(action, "1") == 0) {
        ptt_action_start();
    } else if (strcmp(action, "stop") == 0 || strcmp(action, "unkey") == 0 ||
               strcmp(action, "release") == 0 || strcmp(action, "0") == 0) {
        ptt_action_stop();
    }
}

static void handle_ptt_post(int fd, const char *body)
{
    if (!body || *body == '\0') {
        http_response(fd, 400, "{\"error\":\"empty body\"}");
        return;
    }

    char action[32] = {0};
    json_get_string(body, "action", action, sizeof(action));
    if (action[0] == '\0') {
        http_response(fd, 400, "{\"error\":\"action required\"}");
        return;
    }

    ptt_handle_action(action);
    http_response(fd, 200, "{\"ok\":true}");
}

/* ── REST dispatcher ──────────────────────────────────────────── */

static void handle_rest(int fd, const char *buf, int buf_len)
{
    char method[8] = {0};
    char path[128] = {0};
    sscanf(buf, "%7s %127s", method, path);

    printf("[%s] REST %s %s\n", TAG, method, path);

    char *body = read_body(fd, buf, buf_len);

    if (strcmp(path, "/api/config") == 0) {
        if (strcmp(method, "GET") == 0) {
            handle_config_get(fd);
        } else if (strcmp(method, "PUT") == 0) {
            handle_config_put(fd, body);
        } else {
            http_response(fd, 405, "{\"error\":\"method not allowed\"}");
        }
    } else if (strcmp(path, "/api/skills") == 0) {
        if (strcmp(method, "GET") == 0) {
            handle_skills_get(fd);
        } else if (strcmp(method, "POST") == 0) {
            handle_skills_post(fd, body);
        } else {
            http_response(fd, 405, "{\"error\":\"method not allowed\"}");
        }
    } else if (strncmp(path, "/api/skills/", 12) == 0) {
        if (strcmp(method, "GET") == 0) {
            handle_skill_get(fd, path + 12);
        } else if (strcmp(method, "DELETE") == 0) {
            handle_skills_delete(fd, path + 12);
        } else {
            http_response(fd, 405, "{\"error\":\"method not allowed\"}");
        }
    } else if (strncmp(path, "/api/logs", 9) == 0 && strcmp(method, "GET") == 0) {
        handle_logs_get(fd);
    } else if (strcmp(path, "/api/ptt") == 0 && strcmp(method, "POST") == 0) {
        handle_ptt_post(fd, body);
    } else if (strcmp(path, "/a2a/health") == 0 && strcmp(method, "GET") == 0) {
        http_response(fd, 200, "{\"status\":\"ok\"}");
    } else {
        http_response(fd, 404, "{\"error\":\"not found\"}");
    }

    free(body);
}

/* ── WebSocket helpers ────────────────────────────────────────── */

static int ws_make_accept(const char *key, char *out, size_t out_size)
{
    char combined[256];
    int clen = snprintf(combined, sizeof(combined), "%s%s", key, WS_GUID);
    if (clen <= 0 || clen >= (int)sizeof(combined)) return -1;

    uint8_t digest[20];
    sha1_ctx_t ctx;
    sha1_init(&ctx);
    sha1_update(&ctx, (const uint8_t *)combined, (size_t)clen);
    sha1_final(&ctx, digest);

    return base64_encode(digest, sizeof(digest), out, out_size);
}

static int ws_send_frame(int fd, const char *payload, size_t len)
{
    uint8_t hdr[10];
    int hlen = 0;
    hdr[0] = 0x81; /* FIN + text */
    if (len < 126) {
        hdr[1] = (uint8_t)len;
        hlen = 2;
    } else if (len < 65536) {
        hdr[1] = 126;
        hdr[2] = (uint8_t)(len >> 8);
        hdr[3] = (uint8_t)(len & 0xFF);
        hlen = 4;
    } else {
        return -1;
    }
    if (send(fd, hdr, hlen, 0) != hlen) return -1;
    if (send(fd, payload, len, 0) != (ssize_t)len) return -1;
    return 0;
}

static ws_client_t *ws_add_client(int fd)
{
    pthread_mutex_lock(&s_clients_lock);
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (!s_clients[i].active) {
            /* Initialize mutex before marking active so broadcast threads
             * never see an active client with an uninitialized mutex. */
            pthread_mutex_init(&s_clients[i].mtx, NULL);
            s_clients[i].active = true;
            s_clients[i].fd = fd;
            pthread_mutex_unlock(&s_clients_lock);
            return &s_clients[i];
        }
    }
    pthread_mutex_unlock(&s_clients_lock);
    return NULL;
}

static void ws_remove_client(ws_client_t *c)
{
    pthread_mutex_lock(&s_clients_lock);
    if (c->active) {
        c->active = false;
        close(c->fd);
        c->fd = -1;
        pthread_mutex_destroy(&c->mtx);
    }
    pthread_mutex_unlock(&s_clients_lock);
}

static int ws_recv_frame(int fd, char *buf, size_t buf_size)
{
    uint8_t b0, b1;
    if (recv_all(fd, &b0, 1, 0) != 1) return -1;
    if (recv_all(fd, &b1, 1, 0) != 1) return -1;

    int opcode = b0 & 0x0F;
    bool masked = (b1 & 0x80) != 0;
    size_t plen = b1 & 0x7F;

    if (opcode == 0x8) return 0; /* close */
    if (opcode == 0x9) {
        /* ping: consume and reply pong */
        uint8_t mask[4] = {0};
        if (masked) recv_all(fd, mask, 4, 0);
        unsigned char payload[125] = {0};
        if (plen > 0 && plen <= 125) recv_all(fd, payload, plen, 0);
        uint8_t pong[2] = {0x8A, (uint8_t)plen};
        send(fd, pong, 2, 0);
        if (plen > 0) send(fd, payload, plen, 0);
        return -2;
    }
    if (opcode != 0x1 && opcode != 0x2) return -2;

    if (plen == 126) {
        uint8_t ext[2];
        if (recv_all(fd, ext, 2, 0) != 2) return -1;
        plen = ((size_t)ext[0] << 8) | ext[1];
    } else if (plen == 127) {
        return -1;
    }

    uint8_t mask_key[4] = {0};
    if (masked) {
        if (recv_all(fd, mask_key, 4, 0) != 4) return -1;
    }
    if (plen >= buf_size) return -1;

    if (recv_all(fd, buf, plen, 0) != (ssize_t)plen) return -1;
    if (masked) {
        for (size_t i = 0; i < plen; i++) {
            ((uint8_t *)buf)[i] ^= mask_key[i % 4];
        }
    }
    buf[plen] = '\0';
    return (int)plen;
}

static int ws_do_handshake(int fd, const char *buf, int buf_len)
{
    (void)buf_len;
    const char *key_hdr = radio_strcasestr(buf, "\r\nSec-WebSocket-Key: ");
    if (!key_hdr) return -1;
    key_hdr += 21;
    const char *eol = strstr(key_hdr, "\r\n");
    if (!eol) return -1;

    char key[128] = {0};
    size_t klen = (size_t)(eol - key_hdr);
    if (klen >= sizeof(key)) klen = sizeof(key) - 1;
    memcpy(key, key_hdr, klen);

    char accept[64];
    if (ws_make_accept(key, accept, sizeof(accept)) < 0) return -1;

    char resp[512];
    int rlen = snprintf(resp, sizeof(resp),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n\r\n", accept);
    if (send(fd, resp, rlen, 0) != rlen) return -1;
    return 0;
}

static void ws_handle_client(int fd)
{
    ws_client_t *c = ws_add_client(fd);
    if (!c) {
        uint8_t close_frame[2] = {0x88, 0x00};
        send(fd, close_frame, 2, 0);
        close(fd);
        return;
    }

    /* Announce connection status. */
    const char *connected_msg = "{\"type\":\"status\",\"content\":\"connected\"}";
    ws_send_frame(fd, connected_msg, strlen(connected_msg));

    char frame[MAX_FRAME_LEN];
    while (s_running) {
        int n = ws_recv_frame(fd, frame, sizeof(frame));
        if (n == 0) break;
        if (n < 0 && n != -2) break;
        if (n < 0) continue;

        /* Parse simple {"type":"...","action"/"content":"..."}. */
        char type[32] = {0};
        char content[512] = {0};
        char action[32] = {0};
        json_get_string(frame, "type", type, sizeof(type));
        json_get_string(frame, "content", content, sizeof(content));
        json_get_string(frame, "action", action, sizeof(action));

        if (strcmp(type, "message") == 0 && content[0]) {
            printf("[%s] WS msg: %s\n", TAG, content);
            /* Echo back an acknowledgement. */
            char reply[1024];
            snprintf(reply, sizeof(reply),
                "{\"type\":\"response\",\"content\":\"收到: %s\"}", content);
            pthread_mutex_lock(&c->mtx);
            ws_send_frame(fd, reply, strlen(reply));
            pthread_mutex_unlock(&c->mtx);
        } else if (strcmp(type, "ptt") == 0 && action[0]) {
            ptt_handle_action(action);
            char reply[128];
            snprintf(reply, sizeof(reply),
                "{\"type\":\"status\",\"content\":\"ptt_%s\"}", action);
            pthread_mutex_lock(&c->mtx);
            ws_send_frame(fd, reply, strlen(reply));
            pthread_mutex_unlock(&c->mtx);
        }
    }

    ws_remove_client(c);
}

/* ── Connection handling ──────────────────────────────────────── */

typedef struct {
    int fd;
} conn_arg_t;

static void *client_thread(void *arg)
{
    conn_arg_t *ca = (conn_arg_t *)arg;
    int fd = ca->fd;
    free(ca);

    char *peek = malloc(PEEK_BUFSIZE);
    if (!peek) {
        close(fd);
        return NULL;
    }

    int peek_total = 0;
    while (peek_total < PEEK_BUFSIZE - 1) {
        int n = recv(fd, peek + peek_total, PEEK_BUFSIZE - 1 - peek_total, 0);
        if (n <= 0) {
            free(peek);
            close(fd);
            return NULL;
        }
        peek_total += n;
        peek[peek_total] = '\0';
        if (strstr(peek, "\r\n\r\n")) break;
    }

    /* Upgrade header present -> WebSocket. */
    if (radio_strcasestr(peek, "Upgrade: websocket")) {
        if (ws_do_handshake(fd, peek, peek_total) == 0) {
            free(peek);
            ws_handle_client(fd);
            return NULL;
        }
    }

    handle_rest(fd, peek, peek_total);
    free(peek);
    close(fd);
    return NULL;
}

static void *accept_thread(void *arg)
{
    (void)arg;
    while (s_running) {
        struct pollfd pfd = { .fd = s_listen_fd, .events = POLLIN };
        int pr = poll(&pfd, 1, 500);
        if (pr <= 0) continue;

        struct sockaddr_in addr;
        socklen_t addr_len = sizeof(addr);
        int cfd = accept(s_listen_fd, (struct sockaddr *)&addr, &addr_len);
        if (cfd < 0) continue;

        conn_arg_t *ca = malloc(sizeof(conn_arg_t));
        if (!ca) {
            close(cfd);
            continue;
        }
        ca->fd = cfd;

        pthread_t tid;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_attr_setstacksize(&attr, WS_CLIENT_STACK);
        if (pthread_create(&tid, &attr, client_thread, ca) != 0) {
            free(ca);
            close(cfd);
        }
        pthread_attr_destroy(&attr);
    }
    return NULL;
}

/* ── Public API ───────────────────────────────────────────────── */

int radio_server_start(void)
{
    ensure_dir("/data/agent/config");
    ensure_dir(SKILLS_DIR);
    ensure_dir(LOGS_DIR);

    /* Create empty config file if missing. */
    int fd = open(CONFIG_FILE, O_RDONLY);
    if (fd < 0) {
        write_file(CONFIG_FILE, "{}");
    } else {
        close(fd);
    }

    memset(s_clients, 0, sizeof(s_clients));

    s_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (s_listen_fd < 0) {
        printf("[%s] socket failed\n", TAG);
        return -1;
    }

    int opt = 1;
    setsockopt(s_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(RADIO_SERVER_PORT),
        .sin_addr.s_addr = INADDR_ANY,
    };

    int bind_retries = 5;
    while (bind_retries-- > 0) {
        if (bind(s_listen_fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
            break;
        }
        printf("[%s] bind port %d failed: %d (retries left %d)\n",
               TAG, RADIO_SERVER_PORT, errno, bind_retries);
        if (bind_retries <= 0) {
            close(s_listen_fd);
            s_listen_fd = -1;
            return -1;
        }
        usleep(200000); /* 200 ms */
    }

    if (listen(s_listen_fd, MAX_WS_CLIENTS) < 0) {
        printf("[%s] listen failed\n", TAG);
        close(s_listen_fd);
        s_listen_fd = -1;
        return -1;
    }

    s_running = true;

    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_attr_setstacksize(&attr, ACCEPT_STACK);
    int rc = pthread_create(&tid, &attr, accept_thread, NULL);
    pthread_attr_destroy(&attr);

    if (rc != 0) {
        printf("[%s] accept thread failed\n", TAG);
        s_running = false;
        close(s_listen_fd);
        s_listen_fd = -1;
        return -1;
    }

    printf("[%s] started on port %d\n", TAG, RADIO_SERVER_PORT);
    return 0;
}

void radio_server_stop(void)
{
    s_running = false;

    /* Shutdown the listener first so accept() returns. */
    if (s_listen_fd >= 0) {
        int fd = s_listen_fd;
        s_listen_fd = -1;
        shutdown(fd, SHUT_RDWR);
        close(fd);
    }

    /* Shutdown active client sockets so their recv loops exit cleanly.
     * Each client thread owns the close() after detecting the error. */
    pthread_mutex_lock(&s_clients_lock);
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (s_clients[i].active) {
            shutdown(s_clients[i].fd, SHUT_RDWR);
        }
    }
    pthread_mutex_unlock(&s_clients_lock);

    /* Give detached client threads a moment to observe the shutdown and
     * self-cleanup.  This is best-effort; the threads are detached and
     * will finish on their own even if we don't wait. */
    usleep(100000);

    pthread_mutex_lock(&s_clients_lock);
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (!s_clients[i].active && s_clients[i].fd >= 0) {
            /* Already cleaned up by its own thread; close lingering fd. */
            close(s_clients[i].fd);
            s_clients[i].fd = -1;
        } else if (s_clients[i].active) {
            /* Force close any client that did not exit in time. */
            close(s_clients[i].fd);
            s_clients[i].active = false;
            s_clients[i].fd = -1;
            pthread_mutex_destroy(&s_clients[i].mtx);
        }
    }
    pthread_mutex_unlock(&s_clients_lock);

    printf("[%s] stopped\n", TAG);
}

void radio_server_broadcast_status(const char *status)
{
    if (!status) return;
    char escaped[256];
    json_escape(status, escaped, sizeof(escaped));
    char msg[512];
    snprintf(msg, sizeof(msg), "{\"type\":\"status\",\"content\":\"%s\"}", escaped);

    pthread_mutex_lock(&s_clients_lock);
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (s_clients[i].active) {
            pthread_mutex_lock(&s_clients[i].mtx);
            ws_send_frame(s_clients[i].fd, msg, strlen(msg));
            pthread_mutex_unlock(&s_clients[i].mtx);
        }
    }
    pthread_mutex_unlock(&s_clients_lock);
}

void radio_server_broadcast_log(const char *timestamp, const char *text)
{
    if (!text) return;
    char ts_esc[128];
    char txt_esc[512];
    json_escape(timestamp ? timestamp : "", ts_esc, sizeof(ts_esc));
    json_escape(text, txt_esc, sizeof(txt_esc));
    char msg[1024];
    snprintf(msg, sizeof(msg),
        "{\"type\":\"log\",\"timestamp\":\"%s\",\"content\":\"%s\"}",
        ts_esc, txt_esc);

    pthread_mutex_lock(&s_clients_lock);
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (s_clients[i].active) {
            pthread_mutex_lock(&s_clients[i].mtx);
            ws_send_frame(s_clients[i].fd, msg, strlen(msg));
            pthread_mutex_unlock(&s_clients[i].mtx);
        }
    }
    pthread_mutex_unlock(&s_clients_lock);
}
