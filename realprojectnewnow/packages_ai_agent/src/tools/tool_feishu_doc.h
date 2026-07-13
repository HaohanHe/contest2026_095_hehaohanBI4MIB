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

#pragma once

#include <stddef.h>

/**
 * Create a new Feishu document.
 * Input: {"title":"...", "folder_token":"..." (optional)}
 * Returns: document_id, title, url
 */
int tool_feishu_doc_create_execute(const char *input_json,
                                   char *output, size_t output_size);

/**
 * Write content blocks into an existing Feishu document.
 * Input: {"document_id":"...", "content":"..." (markdown-like text)}
 * Inserts text blocks as children of the document's root block.
 */
int tool_feishu_doc_write_execute(const char *input_json,
                                  char *output, size_t output_size);

/**
 * Get the plain-text content of a Feishu document.
 * Input: {"document_id":"..."}
 */
int tool_feishu_doc_read_execute(const char *input_json,
                                 char *output, size_t output_size);

/**
 * List recent documents in a folder or the root space.
 * Input: {"folder_token":"..." (optional)}
 */
int tool_feishu_doc_list_execute(const char *input_json,
                                 char *output, size_t output_size);
