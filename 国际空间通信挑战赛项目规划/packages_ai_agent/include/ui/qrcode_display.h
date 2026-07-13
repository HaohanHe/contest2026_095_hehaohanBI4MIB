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

#ifndef AGENT_QRCODE_DISPLAY_H
#define AGENT_QRCODE_DISPLAY_H

/**
 * Display a QR code on screen via LVGL.
 * On devices without LVGL/qrcode support, logs the URL and returns OK.
 * Must be called from any thread — internally posts to LVGL UI thread.
 *
 * @param url  The URL string to encode as QR code.
 * @return OK on success, ERROR on failure.
 */
int claw_show_qrcode(const char *url);

#endif /* AGENT_QRCODE_DISPLAY_H */
