/****************************************************************************
 * apps/vendor/allwinnertech/apps/ai_radio_console/asr_client.c
 *
 * Minimal HTTP ASR client for OpenAI-compatible transcription endpoints.
 *
 * Note: This implementation speaks plain HTTP. Most cloud ASR services
 * (SiliconFlow, MiMo) require HTTPS. For those you have two options:
 *   1) Run a local HTTP-to-HTTPS relay on a phone/laptop (relay.py provided).
 *   2) Configure ai_agent to use its built-in Volcengine ASR (HTTPS) and
 *      call the agent via VelaClaw instead of using this client.
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "asr_client.h"

#define ASR_BUFSIZE (32 * 1024)
#define ASR_REQ_BUFSIZE (4 * 1024)
#define ASR_RETRIES 2
#define ASR_RETRY_DELAY_MS 500

struct asr_ctx_s {
    char url[256];
    char model[64];
    char api_key[128];
};

static struct asr_ctx_s g_asr;

static int parse_url(const char *url, char *host, size_t host_size,
                     char *path, size_t path_size, int *port, bool *https)
{
    const char *p = url;

    *https = false;
    if (strncmp(p, "http://", 7) == 0) {
        p += 7;
    } else if (strncmp(p, "https://", 8) == 0) {
        p += 8;
        *https = true;
    }

    const char *slash = strchr(p, '/');
    const char *colon = strchr(p, ':');

    if (!slash) {
        slash = p + strlen(p);
    }

    *port = *https ? 443 : 80;

    if (colon && colon < slash) {
        size_t host_len = (size_t)(colon - p);
        if (host_len >= host_size) {
            return -EINVAL;
        }
        memcpy(host, p, host_len);
        host[host_len] = '\0';
        *port = atoi(colon + 1);
    } else {
        size_t host_len = (size_t)(slash - p);
        if (host_len >= host_size) {
            return -EINVAL;
        }
        memcpy(host, p, host_len);
        host[host_len] = '\0';
    }

    if (path) {
        if (*slash == '\0') {
            strncpy(path, "/", path_size);
        } else {
            strncpy(path, slash, path_size - 1);
            path[path_size - 1] = '\0';
        }
    }

    return 0;
}

static int tcp_connect(const char *host, int port)
{
    struct hostent *he = gethostbyname(host);
    if (!he || !he->h_addr_list[0]) {
        printf("[asr] gethostbyname(%s) failed\n", host);
        return -1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("[asr] socket failed: %d\n", errno);
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr, he->h_addr_list[0], sizeof(struct in_addr));

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("[asr] connect %s:%d failed: %d\n", host, port, errno);
        close(sock);
        return -1;
    }

    return sock;
}

static int http_post_multipart(const char *host, int port, const char *path,
                               const char *api_key, const char *model,
                               const uint8_t *pcm, size_t pcm_len,
                               uint32_t sample_rate,
                               char *response, size_t response_cap)
{
    int sock = tcp_connect(host, port);
    if (sock < 0) {
        return -1;
    }

    static const char *boundary = "----RadioFormBoundary7MA4YWxkTrZu0gW";
    char req[ASR_REQ_BUFSIZE];
    int req_len = 0;

    /* Body */
    char body[ASR_REQ_BUFSIZE];
    int body_len = snprintf(body, sizeof(body),
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"model\"\r\n\r\n"
        "%s\r\n"
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"radio.wav\"\r\n"
        "Content-Type: audio/wav\r\n\r\n",
        boundary, model, boundary);

    /* Minimal WAV header for raw PCM */
    uint32_t byte_rate = sample_rate * 2; /* 16-bit mono */
    uint32_t data_size = pcm_len;
    uint32_t riff_size = 36 + data_size;
    uint8_t wav_hdr[44];
    memcpy(wav_hdr, "RIFF", 4);
    memcpy(wav_hdr + 4, &riff_size, 4);
    memcpy(wav_hdr + 8, "WAVE", 4);
    memcpy(wav_hdr + 12, "fmt ", 4);
    uint32_t subchunk1_size = 16;
    uint16_t audio_format = 1;
    uint16_t num_channels = 1;
    uint16_t bits_per_sample = 16;
    memcpy(wav_hdr + 16, &subchunk1_size, 4);
    memcpy(wav_hdr + 20, &audio_format, 2);
    memcpy(wav_hdr + 22, &num_channels, 2);
    memcpy(wav_hdr + 24, &sample_rate, 4);
    memcpy(wav_hdr + 28, &byte_rate, 4);
    uint16_t block_align = 2;
    memcpy(wav_hdr + 32, &block_align, 2);
    memcpy(wav_hdr + 34, &bits_per_sample, 2);
    memcpy(wav_hdr + 36, "data", 4);
    memcpy(wav_hdr + 40, &data_size, 4);

    int tail_len = snprintf(body + body_len, sizeof(body) - body_len,
                            "\r\n--%s--\r\n", boundary);

    int content_len = body_len + sizeof(wav_hdr) + pcm_len + tail_len;

    req_len = snprintf(req, sizeof(req),
        "POST %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Authorization: Bearer %s\r\n"
        "Content-Type: multipart/form-data; boundary=%s\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n",
        path, host, port, api_key, boundary, content_len);

    /* Send headers */
    if (send(sock, req, req_len, 0) != req_len) {
        printf("[asr] send headers failed\n");
        close(sock);
        return -1;
    }

    /* Send body prefix */
    if (send(sock, body, body_len, 0) != body_len) {
        printf("[asr] send body prefix failed\n");
        close(sock);
        return -1;
    }

    /* Send WAV header */
    if (send(sock, wav_hdr, sizeof(wav_hdr), 0) != (ssize_t)sizeof(wav_hdr)) {
        printf("[asr] send wav header failed\n");
        close(sock);
        return -1;
    }

    /* Send PCM */
    size_t sent = 0;
    while (sent < pcm_len) {
        ssize_t n = send(sock, pcm + sent, pcm_len - sent, 0);
        if (n <= 0) {
            printf("[asr] send pcm failed at %zu\n", sent);
            close(sock);
            return -1;
        }
        sent += n;
    }

    /* Send body tail */
    if (send(sock, body + body_len, tail_len, 0) != tail_len) {
        printf("[asr] send body tail failed\n");
        close(sock);
        return -1;
    }

    /* Receive response (retry on EINTR). */
    size_t total = 0;
    while (total < response_cap - 1) {
        ssize_t n = recv(sock, response + total, response_cap - total - 1, 0);
        if (n > 0) {
            total += (size_t)n;
        } else if (n == 0) {
            break;
        } else if (errno == EINTR) {
            continue;
        } else {
            break;
        }
    }
    response[total] = '\0';
    close(sock);

    if (total == 0) {
        printf("[asr] empty response\n");
        return -1;
    }

    /* Find body */
    char *body_start = strstr(response, "\r\n\r\n");
    if (!body_start) {
        printf("[asr] malformed response\n");
        return -1;
    }
    body_start += 4;

    /* Move body to start of response buffer */
    memmove(response, body_start, strlen(body_start) + 1);
    return 0;
}

static int extract_text(const char *json, char **text_out)
{
    const char *key = "\"text\"";
    const char *p = strstr(json, key);
    if (!p) {
        /* Try alternative key used by some providers */
        p = strstr(json, "\"transcript\"");
    }
    if (!p) {
        printf("[asr] no text/transcript in response: %s\n", json);
        return -1;
    }

    p = strchr(p, ':');
    if (!p) {
        return -1;
    }
    p++;
    while (*p == ' ' || *p == '\t') {
        p++;
    }

    char quote = *p;
    if (quote != '"' && quote != '\'') {
        return -1;
    }
    p++;

    /* Find closing quote, skipping escaped quotes. */
    const char *end = p;
    while (*end) {
        if (*end == '\\' && *(end + 1)) {
            end += 2;
        } else if (*end == quote) {
            break;
        } else {
            end++;
        }
    }
    if (*end != quote) {
        return -1;
    }

    size_t len = (size_t)(end - p);
    char *text = malloc(len + 1);
    if (!text) {
        return -ENOMEM;
    }

    /* Unescape common sequences while copying. */
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (p[i] == '\\' && i + 1 < len) {
            switch (p[i + 1]) {
            case '"':  text[j++] = '"';  i++; break;
            case '\\': text[j++] = '\\'; i++; break;
            case '/':  text[j++] = '/';  i++; break;
            case 'b':  text[j++] = '\b'; i++; break;
            case 'f':  text[j++] = '\f'; i++; break;
            case 'n':  text[j++] = '\n'; i++; break;
            case 'r':  text[j++] = '\r'; i++; break;
            case 't':  text[j++] = '\t'; i++; break;
            default:   text[j++] = p[i]; break;
            }
        } else {
            text[j++] = p[i];
        }
    }
    text[j] = '\0';
    *text_out = text;
    return 0;
}

int asr_client_init(const char *url, const char *model, const char *api_key)
{
    if (!url || !model || !api_key) {
        return -EINVAL;
    }

    memset(&g_asr, 0, sizeof(g_asr));
    strncpy(g_asr.url, url, sizeof(g_asr.url) - 1);
    strncpy(g_asr.model, model, sizeof(g_asr.model) - 1);
    strncpy(g_asr.api_key, api_key, sizeof(g_asr.api_key) - 1);

    printf("[asr] init url=%s model=%s\n", g_asr.url, g_asr.model);
    return 0;
}

int asr_transcribe(const uint8_t *pcm_data, size_t pcm_len,
                   uint32_t sample_rate, char **text_out)
{
    char host[128];
    char path[256];
    int port;
    bool https;
    static char response[ASR_BUFSIZE];

    if (!pcm_data || pcm_len == 0 || !text_out) {
        return -EINVAL;
    }

    if (parse_url(g_asr.url, host, sizeof(host), path, sizeof(path),
                  &port, &https) < 0) {
        return -EINVAL;
    }

    if (https) {
        printf("[asr] HTTPS not supported by this minimal client.\n"
               "       Use an HTTP relay or configure ai_agent Volc ASR.\n");
        return -ENOTSUP;
    }

    for (int attempt = 0; attempt < ASR_RETRIES; attempt++) {
        if (http_post_multipart(host, port, path, g_asr.api_key, g_asr.model,
                                pcm_data, pcm_len, sample_rate,
                                response, sizeof(response)) == 0) {
            return extract_text(response, text_out);
        }
        printf("[asr] transcription failed (attempt %d/%d)\n",
               attempt + 1, ASR_RETRIES);
        if (attempt + 1 < ASR_RETRIES) {
            usleep(ASR_RETRY_DELAY_MS * 1000);
        }
    }

    return -EIO;
}

void asr_client_deinit(void)
{
    memset(&g_asr, 0, sizeof(g_asr));
}
