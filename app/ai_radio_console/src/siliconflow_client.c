#include "siliconflow_client.h"
#include "json_minimal.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>

static ai_config_t g_config;
static char g_last_error[256];

typedef struct {
    const char *content_type;
    const uint8_t *multipart_buf;
    size_t multipart_len;
    const char *json_body;
} request_body_t;

static void set_error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_last_error, sizeof(g_last_error), fmt, ap);
    va_end(ap);
}

static int parse_url(const char *url, char *host, size_t host_len,
                     int *port, char *path, size_t path_len, bool *is_https)
{
    const char *p = url;
    *is_https = false;
    *port = 80;

    if (strncmp(p, "https://", 8) == 0) {
        *is_https = true;
        *port = 443;
        p += 8;
    } else if (strncmp(p, "http://", 7) == 0) {
        p += 7;
    }

    const char *slash = strchr(p, '/');
    size_t hlen = slash ? (size_t)(slash - p) : strlen(p);
    if (hlen >= host_len) return -1;
    strncpy(host, p, hlen);
    host[hlen] = '\0';

    char *colon = strchr(host, ':');
    if (colon) {
        *port = atoi(colon + 1);
        *colon = '\0';
        hlen = colon - host;
    }

    if (slash) {
        strncpy(path, slash, path_len - 1);
        path[path_len - 1] = '\0';
    } else {
        strcpy(path, "/");
    }
    return 0;
}

static int http_post(const char *url, const char *api_key, const char *content_type,
                     const uint8_t *body_buf, size_t body_len,
                     sf_http_response_t *resp)
{
    char host[128], path[512];
    int port;
    bool is_https;
    if (parse_url(url, host, sizeof(host), &port, path, sizeof(path), &is_https) != 0) {
        set_error("Invalid URL: %s", url);
        return -1;
    }

    if (is_https) {
        snprintf(g_last_error, sizeof(g_last_error),
                 "HTTPS requires TLS. Configure endpoint via HTTP proxy or build with mbedtls.");
        return -1;
    }

    struct hostent *he = gethostbyname(host);
    if (!he) {
        set_error("DNS resolve failed for %s", host);
        return -1;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        set_error("socket() failed: %d", errno);
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        set_error("connect() failed: %d", errno);
        close(sockfd);
        return -1;
    }

    char headers[2048];
    int hlen = snprintf(headers, sizeof(headers),
        "POST %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Authorization: Bearer %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n",
        path, host, api_key, content_type, body_len);

    if (send(sockfd, headers, hlen, 0) != hlen) {
        set_error("send headers failed");
        close(sockfd);
        return -1;
    }

    size_t sent = 0;
    while (sent < body_len) {
        ssize_t n = send(sockfd, body_buf + sent, body_len - sent, 0);
        if (n <= 0) {
            set_error("send body failed at %zu", sent);
            close(sockfd);
            return -1;
        }
        sent += n;
    }

    memset(resp, 0, sizeof(*resp));
    char recv_buf[4096];
    bool header_done = false;
    while (1) {
        ssize_t n = recv(sockfd, recv_buf, sizeof(recv_buf) - 1, 0);
        if (n <= 0) break;
        recv_buf[n] = '\0';

        if (!header_done) {
            const char *body_start = strstr(recv_buf, "\r\n\r\n");
            if (body_start) {
                int hdr_len = (int)(body_start - recv_buf) + 4;
                if (strstr(recv_buf, "HTTP/1.1 200") || strstr(recv_buf, "HTTP/1.0 200")) {
                    resp->http_status = 200;
                } else {
                    const char *sp = strchr(recv_buf, ' ');
                    if (sp) resp->http_status = atoi(sp + 1);
                }
                int body_part = n - hdr_len;
                if (body_part > 0) {
                    size_t to_copy = body_part;
                    if (resp->response_len + to_copy >= sizeof(resp->response))
                        to_copy = sizeof(resp->response) - 1 - resp->response_len;
                    memcpy(resp->response + resp->response_len, body_start + 4, to_copy);
                    resp->response_len += to_copy;
                    resp->response[resp->response_len] = '\0';
                }
                header_done = true;
            }
        } else {
            size_t to_copy = n;
            if (resp->response_len + to_copy >= sizeof(resp->response))
                to_copy = sizeof(resp->response) - 1 - resp->response_len;
            memcpy(resp->response + resp->response_len, recv_buf, to_copy);
            resp->response_len += to_copy;
            resp->response[resp->response_len] = '\0';
        }
    }

    close(sockfd);
    if (resp->http_status != 200) {
        set_error("HTTP %d: %s", resp->http_status, resp->response);
        return -1;
    }
    return 0;
}

int sf_client_init(const ai_config_t *config)
{
    if (!config) return -1;
    memcpy(&g_config, config, sizeof(g_config));
    g_last_error[0] = '\0';
    return 0;
}

void sf_client_deinit(void)
{
}

int sf_client_transcribe_audio(const uint8_t *wav_data, size_t wav_size,
                                const char *model, char *out_text, size_t max_len)
{
    if (g_config.api_key[0] == '\0') {
        set_error("API key not configured");
        return -1;
    }

    char boundary[] = SF_MULTIPART_BOUNDARY;
    size_t part1_len = 256 + strlen(model) + strlen(boundary);
    size_t total = part1_len + wav_size + 128;
    uint8_t *multipart_buf = (uint8_t *)malloc(total);
    if (!multipart_buf) {
        set_error("malloc failed for multipart");
        return -1;
    }

    int pos = snprintf((char *)multipart_buf, total,
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"model\"\r\n\r\n"
        "%s\r\n"
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n"
        "Content-Type: audio/wav\r\n\r\n",
        boundary, model, boundary);

    memcpy(multipart_buf + pos, wav_data, wav_size);
    pos += wav_size;
    pos += snprintf((char *)multipart_buf + pos, total - pos,
        "\r\n--%s--\r\n", boundary);

    char content_type[256];
    snprintf(content_type, sizeof(content_type),
             "multipart/form-data; boundary=%s", boundary);

    sf_http_response_t resp;
    int ret = http_post(g_config.asr_endpoint[0] ? g_config.asr_endpoint : SILICONFLOW_ASR_ENDPOINT,
                        g_config.api_key, content_type, multipart_buf, pos, &resp);
    free(multipart_buf);
    if (ret != 0) return -1;

    if (json_extract_string(resp.response, "text", out_text, max_len) != 0) {
        set_error("Failed to parse ASR response: %s", resp.response);
        return -1;
    }
    return 0;
}

int sf_client_chat_completion(const char *model, const char *system_prompt,
                               const char *user_message, char *out_response,
                               size_t max_len, float *out_confidence)
{
    if (g_config.api_key[0] == '\0') {
        set_error("API key not configured");
        return -1;
    }

    char json_buf[SF_HTTP_BUF_SIZE];
    if (json_build_chat_completion(json_buf, sizeof(json_buf), g_config.api_key,
                                   model, system_prompt, user_message, false) != 0) {
        set_error("JSON build failed (too large?)");
        return -1;
    }

    sf_http_response_t resp;
    int ret = http_post(g_config.chat_endpoint[0] ? g_config.chat_endpoint : SILICONFLOW_CHAT_ENDPOINT,
                        g_config.api_key, "application/json",
                        (const uint8_t *)json_buf, strlen(json_buf), &resp);
    if (ret != 0) return -1;

    const char *content_key = "\"content\":\"";
    const char *c = strstr(resp.response, content_key);
    if (!c) {
        set_error("No content in LLM response: %s", resp.response);
        return -1;
    }
    c += strlen(content_key);
    size_t i = 0;
    while (*c && *c != '"' && i < max_len - 1) {
        if (*c == '\\' && *(c+1)) {
            c++;
            if (*c == 'n') out_response[i++] = '\n';
            else if (*c == 't') out_response[i++] = '\t';
            else if (*c == '"') out_response[i++] = '"';
            else out_response[i++] = *c;
        } else {
            out_response[i++] = *c;
        }
        c++;
    }
    out_response[i] = '\0';

    if (out_confidence) *out_confidence = 0.85f;
    return 0;
}

bool sf_client_check_connection(void)
{
    return g_config.api_key[0] != '\0';
}

const char *sf_client_get_last_error(void)
{
    return g_last_error;
}
