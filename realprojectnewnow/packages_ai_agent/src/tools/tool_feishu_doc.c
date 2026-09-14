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

#include "tools/tool_feishu_doc.h"
#include "channels/feishu_bot.h"
#include "agent_compat.h"
#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "tool_feishu_doc";

#define RESP_BUF_SIZE (16 * 1024)

/* Default folder token for document creation */
#define FEISHU_DEFAULT_FOLDER_TOKEN ""

/* ── Helper: check Feishu API response code ──────────────────── */

static int parse_feishu_response(const char *resp, cJSON **out_data,
                                 char *output, size_t output_size)
{
    cJSON *root = cJSON_Parse(resp);
    if (!root) {
        snprintf(output, output_size, "Error: invalid JSON from Feishu API");
        return -1;
    }

    cJSON *code = cJSON_GetObjectItem(root, "code");
    if (!cJSON_IsNumber(code) || code->valueint != 0) {
        cJSON *msg = cJSON_GetObjectItem(root, "msg");
        snprintf(output, output_size, "Error: Feishu API code=%d msg=%s",
                 cJSON_IsNumber(code) ? (int)code->valueint : -1,
                 (msg && cJSON_IsString(msg)) ? msg->valuestring : "unknown");
        cJSON_Delete(root);
        return -1;
    }

    if (out_data) {
        *out_data = root;   /* caller must cJSON_Delete */
    } else {
        cJSON_Delete(root);
    }
    return 0;
}

/* ── feishu_doc_create ───────────────────────────────────────── */

int tool_feishu_doc_create_execute(const char *input_json,
                                   char *output, size_t output_size)
{
    cJSON *input = cJSON_Parse(input_json);
    if (!input) {
        snprintf(output, output_size, "Error: invalid JSON input");
        return ERROR;
    }

    const char *title = cJSON_GetStringValue(
                            cJSON_GetObjectItem(input, "title"));
    const char *folder = cJSON_GetStringValue(
                            cJSON_GetObjectItem(input, "folder_token"));

    if (!folder || !folder[0]) {
        folder = FEISHU_DEFAULT_FOLDER_TOKEN;
    }

    /* Build request body */
    cJSON *body = cJSON_CreateObject();
    if (title && title[0]) {
        cJSON_AddStringToObject(body, "title", title);
    }
    cJSON_AddStringToObject(body, "folder_token", folder);
    char *body_str = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);
    cJSON_Delete(input);

    if (!body_str) {
        snprintf(output, output_size, "Error: out of memory");
        return ERROR;
    }

    char *resp = malloc(RESP_BUF_SIZE);
    if (!resp) { free(body_str); snprintf(output, output_size, "Error: OOM"); return ERROR; }

    syslog(LOG_INFO, "[%s] Creating document: title=%s folder=%s\n", TAG,
           title ? title : "(untitled)", folder);

    /* Create document using user_access_token (docx:document requires user identity) */
    int status = feishu_api_post_as_user("/open-apis/docx/v1/documents",
                                         body_str, resp, RESP_BUF_SIZE);

    syslog(LOG_DEBUG, "[%s] Create doc HTTP status=%d\n", TAG, status);
    free(body_str);

    if (status != 200) {
        snprintf(output, output_size, "Error: HTTP %d from Feishu: %.200s", status, resp);
        free(resp);
        return ERROR;
    }

    /* Parse response: { "code":0, "data":{"document":{"document_id":"...", "title":"...", "url":"..."}} } */
    cJSON *root = NULL;
    if (parse_feishu_response(resp, &root, output, output_size) < 0) {
        free(resp);
        return ERROR;
    }
    free(resp);

    cJSON *data = cJSON_GetObjectItem(root, "data");
    cJSON *doc  = data ? cJSON_GetObjectItem(data, "document") : NULL;

    if (!doc) {
        snprintf(output, output_size, "Error: no document in response");
        cJSON_Delete(root);
        return ERROR;
    }

    /* Build output */
    cJSON *result = cJSON_CreateObject();
    cJSON *doc_id = cJSON_GetObjectItem(doc, "document_id");
    cJSON *doc_title = cJSON_GetObjectItem(doc, "title");

    if (doc_id && cJSON_IsString(doc_id))
        cJSON_AddStringToObject(result, "document_id", doc_id->valuestring);
    if (doc_title && cJSON_IsString(doc_title))
        cJSON_AddStringToObject(result, "title", doc_title->valuestring);

    /* Construct URL for convenience */
    if (doc_id && cJSON_IsString(doc_id)) {
        char url[256];
        snprintf(url, sizeof(url), "https://feishu.cn/docx/%s", doc_id->valuestring);
        cJSON_AddStringToObject(result, "url", url);
    }

    cJSON_AddStringToObject(result, "status", "created");

    char *out_str = cJSON_PrintUnformatted(result);
    cJSON_Delete(result);
    cJSON_Delete(root);

    if (out_str) {
        strncpy(output, out_str, output_size - 1);
        output[output_size - 1] = '\0';
        free(out_str);
    }

    syslog(LOG_INFO, "[%s] Document created successfully\n", TAG);
    return OK;
}

/* ── feishu_doc_write ────────────────────────────────────────── */

int tool_feishu_doc_write_execute(const char *input_json,
                                  char *output, size_t output_size)
{
    cJSON *input = cJSON_Parse(input_json);
    if (!input) {
        snprintf(output, output_size, "Error: invalid JSON input");
        return ERROR;
    }

    const char *doc_id  = cJSON_GetStringValue(
                              cJSON_GetObjectItem(input, "document_id"));
    const char *content = cJSON_GetStringValue(
                              cJSON_GetObjectItem(input, "content"));

    if (!doc_id || !doc_id[0]) {
        snprintf(output, output_size, "Error: missing 'document_id'");
        cJSON_Delete(input);
        return ERROR;
    }
    if (!content || !content[0]) {
        snprintf(output, output_size, "Error: missing 'content'");
        cJSON_Delete(input);
        return ERROR;
    }

    /* Split content by newlines into paragraph blocks.
     * Each non-empty line becomes a text block child of the document root. */

    /* First, get the document to find the root block_id (== document_id) */
    char path[256];
    snprintf(path, sizeof(path),
             "/open-apis/docx/v1/documents/%s/blocks/%s/children",
             doc_id, doc_id);

    /* Build block children array — each paragraph is a block of type 2 (text).
     * Feishu block API (from Go SDK Block struct):
     *   block_type=1: page, block_type=2: text, block_type=3: heading1, ...
     *   { "children": [ { "block_type": 2, "text": { "elements": [
     *       { "text_run": { "content": "..." } }
     *   ] } } ], "index": 0 }
     */
    cJSON *children = cJSON_CreateArray();
    const char *p = content;

    while (*p) {
        /* Find end of line */
        const char *eol = strchr(p, '\n');
        size_t line_len = eol ? (size_t)(eol - p) : strlen(p);

        /* Skip empty lines — Feishu rejects empty text_run content */
        if (line_len == 0) {
            p++;
            continue;
        }

        /* Create a text block for each line */
        char *line = malloc(line_len + 1);
        if (!line) break;
        memcpy(line, p, line_len);
        line[line_len] = '\0';

        /* Build: { "block_type": 2, "text": { "elements": [
         *           { "text_run": { "content": "line" } } ] } } */
        cJSON *block = cJSON_CreateObject();
        cJSON_AddNumberToObject(block, "block_type", 2);

        cJSON *text_obj = cJSON_CreateObject();
        cJSON *elements = cJSON_CreateArray();
        cJSON *elem = cJSON_CreateObject();
        cJSON *text_run = cJSON_CreateObject();
        cJSON_AddStringToObject(text_run, "content", line);
        cJSON_AddItemToObject(elem, "text_run", text_run);
        cJSON_AddItemToArray(elements, elem);
        cJSON_AddItemToObject(text_obj, "elements", elements);
        cJSON_AddItemToObject(block, "text", text_obj);

        cJSON_AddItemToArray(children, block);
        free(line);

        p += line_len;
        if (*p == '\n') p++;
    }

    cJSON_Delete(input);

    /* Build request body */
    cJSON *body = cJSON_CreateObject();
    cJSON_AddItemToObject(body, "children", children);
    cJSON_AddNumberToObject(body, "index", 0);  /* 0 = append at end */

    char *body_str = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);

    if (!body_str) {
        snprintf(output, output_size, "Error: out of memory");
        return ERROR;
    }

    char *resp = malloc(RESP_BUF_SIZE);
    if (!resp) { free(body_str); snprintf(output, output_size, "Error: OOM"); return ERROR; }

    syslog(LOG_INFO, "[%s] Writing content to document %s\n", TAG, doc_id);

    int status = feishu_api_post_as_user(path, body_str, resp, RESP_BUF_SIZE);
    free(body_str);

    if (status != 200) {
        snprintf(output, output_size, "Error: HTTP %d: %.200s", status, resp);
        free(resp);
        return ERROR;
    }

    cJSON *root = NULL;
    if (parse_feishu_response(resp, &root, output, output_size) < 0) {
        free(resp);
        return ERROR;
    }
    free(resp);
    cJSON_Delete(root);

    snprintf(output, output_size,
             "{\"status\":\"ok\",\"document_id\":\"%s\",\"message\":\"Content written\"}", doc_id);
    return OK;
}

/* ── feishu_doc_read ─────────────────────────────────────────── */

int tool_feishu_doc_read_execute(const char *input_json,
                                 char *output, size_t output_size)
{
    cJSON *input = cJSON_Parse(input_json);
    if (!input) {
        snprintf(output, output_size, "Error: invalid JSON input");
        return ERROR;
    }

    const char *doc_id = cJSON_GetStringValue(
                             cJSON_GetObjectItem(input, "document_id"));
    if (!doc_id || !doc_id[0]) {
        snprintf(output, output_size, "Error: missing 'document_id'");
        cJSON_Delete(input);
        return ERROR;
    }

    char path[256];
    snprintf(path, sizeof(path),
             "/open-apis/docx/v1/documents/%s/raw_content", doc_id);
    cJSON_Delete(input);

    char *resp = malloc(RESP_BUF_SIZE);
    if (!resp) { snprintf(output, output_size, "Error: OOM"); return ERROR; }

    syslog(LOG_INFO, "[%s] Reading document %s\n", TAG, doc_id);

    int status = feishu_api_request_as_user("GET", path, NULL, 0, resp, RESP_BUF_SIZE);

    if (status != 200) {
        snprintf(output, output_size, "Error: HTTP %d: %.200s", status, resp);
        free(resp);
        return ERROR;
    }

    cJSON *root = NULL;
    if (parse_feishu_response(resp, &root, output, output_size) < 0) {
        free(resp);
        return ERROR;
    }
    free(resp);

    cJSON *data = cJSON_GetObjectItem(root, "data");
    cJSON *content = data ? cJSON_GetObjectItem(data, "content") : NULL;

    if (content && cJSON_IsString(content)) {
        strncpy(output, content->valuestring, output_size - 1);
        output[output_size - 1] = '\0';
    } else {
        /* Fallback: return the whole data object */
        char *data_str = data ? cJSON_PrintUnformatted(data) : NULL;
        if (data_str) {
            strncpy(output, data_str, output_size - 1);
            output[output_size - 1] = '\0';
            free(data_str);
        } else {
            snprintf(output, output_size, "Error: no content in response");
            cJSON_Delete(root);
            return ERROR;
        }
    }

    cJSON_Delete(root);
    return OK;
}

/* ── feishu_doc_list ─────────────────────────────────────────── */

int tool_feishu_doc_list_execute(const char *input_json,
                                 char *output, size_t output_size)
{
    cJSON *input = cJSON_Parse(input_json);
    const char *folder = NULL;
    if (input) {
        folder = cJSON_GetStringValue(
                     cJSON_GetObjectItem(input, "folder_token"));
    }

    char path[256];
    if (folder && folder[0]) {
        snprintf(path, sizeof(path),
                 "/open-apis/drive/v1/files?folder_token=%s&order_by=EditedTime&direction=DESC",
                 folder);
    } else {
        snprintf(path, sizeof(path),
                 "/open-apis/drive/v1/files?order_by=EditedTime&direction=DESC");
    }
    cJSON_Delete(input);

    char *resp = malloc(RESP_BUF_SIZE);
    if (!resp) { snprintf(output, output_size, "Error: OOM"); return ERROR; }

    syslog(LOG_INFO, "[%s] Listing documents (folder=%s)\n", TAG,
           folder ? folder : "root");

    int status = feishu_api_request_as_user("GET", path, NULL, 0, resp, RESP_BUF_SIZE);

    if (status != 200) {
        snprintf(output, output_size, "Error: HTTP %d: %.200s", status, resp);
        free(resp);
        return ERROR;
    }

    cJSON *root = NULL;
    if (parse_feishu_response(resp, &root, output, output_size) < 0) {
        free(resp);
        return ERROR;
    }
    free(resp);

    cJSON *data  = cJSON_GetObjectItem(root, "data");
    cJSON *files = data ? cJSON_GetObjectItem(data, "files") : NULL;

    if (!files || !cJSON_IsArray(files)) {
        snprintf(output, output_size, "[]");
        cJSON_Delete(root);
        return OK;
    }

    /* Build compact output: [{name, token, type, url}, ...] */
    cJSON *result = cJSON_CreateArray();
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, files) {
        cJSON *entry = cJSON_CreateObject();
        cJSON *name  = cJSON_GetObjectItem(item, "name");
        cJSON *token = cJSON_GetObjectItem(item, "token");
        cJSON *type  = cJSON_GetObjectItem(item, "type");
        cJSON *url   = cJSON_GetObjectItem(item, "url");

        if (name && cJSON_IsString(name))
            cJSON_AddStringToObject(entry, "name", name->valuestring);
        if (token && cJSON_IsString(token))
            cJSON_AddStringToObject(entry, "token", token->valuestring);
        if (type && cJSON_IsString(type))
            cJSON_AddStringToObject(entry, "type", type->valuestring);
        if (url && cJSON_IsString(url))
            cJSON_AddStringToObject(entry, "url", url->valuestring);

        cJSON_AddItemToArray(result, entry);
    }

    char *out_str = cJSON_PrintUnformatted(result);
    cJSON_Delete(result);
    cJSON_Delete(root);

    if (out_str) {
        strncpy(output, out_str, output_size - 1);
        output[output_size - 1] = '\0';
        free(out_str);
    }

    return OK;
}
