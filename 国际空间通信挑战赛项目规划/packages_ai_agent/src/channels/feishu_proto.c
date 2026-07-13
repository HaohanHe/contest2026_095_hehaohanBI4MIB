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

#include <stdlib.h>
#include <string.h>

/* ── Minimal protobuf decoder/encoder for Feishu pbbp2 frames ── */
/*
 * Frame struct (from pbbp2.proto):
 *   SeqID    uint64 field=1
 *   LogID    uint64 field=2
 *   service  int32  field=3
 *   method   int32  field=4  (0=control, 1=data)
 *   headers  Header field=5  (repeated)
 *   payload_encoding string field=6
 *   payload_type     string field=7
 *   payload  bytes  field=8
 *
 * Header: key string field=1, value string field=2
 */

static uint64_t pb_read_varint(const uint8_t* buf, int* pos, int len)
{
    uint64_t v = 0;
    int s = 0;

    while (*pos < len) {
        uint8_t b = buf[(*pos)++];
        v |= (uint64_t)(b & 0x7f) << s;
        if (!(b & 0x80)) {
            break;
        }
        s += 7;
    }
    return v;
}

static void pb_skip_field(const uint8_t* buf, int* pos, int len, int wtype)
{
    switch (wtype) {
    case 0:
        pb_read_varint(buf, pos, len);
        break;
    case 1:
        *pos += 8;
        break;
    case 2: {
        int n = (int)pb_read_varint(buf, pos, len);
        if (n > 0 && *pos + n <= len) {
            *pos += n;
        }
        break;
    }
    case 5:
        *pos += 4;
        break;
    default:
        break;
    }
}

static void pb_decode_header_msg(const uint8_t* buf, int len, fk_frame_t* f)
{
    char key[32] = { 0 };
    char val[128] = { 0 };
    int i = 0;

    while (i < len) {
        uint64_t tag = pb_read_varint(buf, &i, len);
        int fn = (int)(tag >> 3);
        int wt = (int)(tag & 7);

        if (wt == 2) {
            int slen = (int)pb_read_varint(buf, &i, len);
            if (slen < 0 || i + slen > len) {
                break;
            }
            if (fn == 1) {
                int n = slen < 31 ? slen : 31;
                memcpy(key, buf + i, n);
                key[n] = 0;
            } else if (fn == 2) {
                int n = slen < 127 ? slen : 127;
                memcpy(val, buf + i, n);
                val[n] = 0;
            }
            i += slen;
        } else {
            pb_skip_field(buf, &i, len, wt);
        }
    }

    if (!strcmp(key, "type")) {
        strncpy(f->h_type, val, sizeof(f->h_type) - 1);
    } else if (!strcmp(key, "message_id")) {
        strncpy(f->h_msg_id, val, sizeof(f->h_msg_id) - 1);
    } else if (!strcmp(key, "trace_id")) {
        strncpy(f->h_trace_id, val, sizeof(f->h_trace_id) - 1);
    } else if (!strcmp(key, "sum")) {
        f->h_sum = atoi(val);
    } else if (!strcmp(key, "seq")) {
        f->h_seq = atoi(val);
    }
}

int pb_decode_frame(const uint8_t* buf, int len, fk_frame_t* f)
{
    memset(f, 0, sizeof(*f));
    int i = 0;

    while (i < len) {
        uint64_t tag = pb_read_varint(buf, &i, len);
        int fn = (int)(tag >> 3);
        int wt = (int)(tag & 7);

        switch (fn) {
        case 1:
            f->seq_id = pb_read_varint(buf, &i, len);
            break;
        case 2:
            pb_read_varint(buf, &i, len);
            break; /* LogID */
        case 3:
            f->service = (int32_t)pb_read_varint(buf, &i, len);
            break;
        case 4:
            f->method = (int32_t)pb_read_varint(buf, &i, len);
            break;
        case 5: { /* repeated Header sub-message */
            if (wt != 2) {
                pb_skip_field(buf, &i, len, wt);
                break;
            }
            int mlen = (int)pb_read_varint(buf, &i, len);
            if (mlen < 0 || i + mlen > len) {
                return -1;
            }
            pb_decode_header_msg(buf + i, mlen, f);
            i += mlen;
            break;
        }
        case 8: { /* payload bytes */
            if (wt != 2) {
                pb_skip_field(buf, &i, len, wt);
                break;
            }
            int plen = (int)pb_read_varint(buf, &i, len);
            if (plen < 0 || i + plen > len) {
                return -1;
            }
            f->payload = buf + i;
            f->payload_len = plen;
            i += plen;
            break;
        }
        default:
            pb_skip_field(buf, &i, len, wt);
            break;
        }
    }
    return 0;
}

/* --- Encoder --- */

static int pb_put_varint(uint8_t* buf, int pos, int cap, uint64_t v)
{
    do {
        if (pos >= cap) {
            return -1;
        }
        buf[pos++] = (v > 0x7f) ? (uint8_t)((v & 0x7f) | 0x80) : (uint8_t)v;
        v >>= 7;
    } while (v);
    return pos;
}

int pb_put_len_field(uint8_t* buf, int pos, int cap, int fnum,
    const void* data, int dlen)
{
    pos = pb_put_varint(buf, pos, cap, ((uint64_t)fnum << 3) | 2);
    if (pos < 0) {
        return -1;
    }
    pos = pb_put_varint(buf, pos, cap, (uint64_t)dlen);
    if (pos < 0 || pos + dlen > cap) {
        return -1;
    }
    if (data && dlen > 0) {
        memcpy(buf + pos, data, dlen);
    }
    return pos + dlen;
}

int pb_put_varint_field(uint8_t* buf, int pos, int cap, int fnum, uint64_t v)
{
    pos = pb_put_varint(buf, pos, cap, (uint64_t)fnum << 3); /* wire type 0 */
    if (pos < 0) {
        return -1;
    }
    return pb_put_varint(buf, pos, cap, v);
}

int pb_encode_header_entry(uint8_t* buf, int pos, int cap,
    const char* key, const char* val)
{
    uint8_t tmp[256];
    int tp = 0;

    tp = pb_put_len_field(tmp, tp, 256, 1, key, (int)strlen(key));
    if (tp < 0) {
        return -1;
    }
    tp = pb_put_len_field(tmp, tp, 256, 2, val, (int)strlen(val));
    if (tp < 0) {
        return -1;
    }
    return pb_put_len_field(buf, pos, cap, 5, tmp, tp);
}

/* ACK response for an incoming data frame */
int fk_encode_ack(uint8_t* out, int cap, const fk_frame_t* req)
{
    static const char ack_pl[] = "{\"code\":200}";
    int p = 0;

    p = pb_put_varint_field(out, p, cap, 1, req->seq_id);
    if (p < 0) {
        return -1;
    }
    p = pb_put_varint_field(out, p, cap, 2, 0);
    if (p < 0) {
        return -1;
    }
    p = pb_put_varint_field(out, p, cap, 3, (uint32_t)req->service);
    if (p < 0) {
        return -1;
    }
    p = pb_put_varint_field(out, p, cap, 4, (uint32_t)req->method);
    if (p < 0) {
        return -1;
    }
    p = pb_encode_header_entry(out, p, cap, "type", "ack");
    if (p < 0) {
        return -1;
    }
    if (req->h_msg_id[0]) {
        p = pb_encode_header_entry(out, p, cap, "message_id", req->h_msg_id);
        if (p < 0) {
            return -1;
        }
    }
    if (req->h_trace_id[0]) {
        p = pb_encode_header_entry(out, p, cap, "trace_id", req->h_trace_id);
        if (p < 0) {
            return -1;
        }
    }
    p = pb_put_len_field(out, p, cap, 8, ack_pl, (int)strlen(ack_pl));
    return p;
}

/* Protobuf ping frame (sent periodically to keep connection alive) */
int fk_encode_ping(uint8_t* out, int cap, int32_t svc_id)
{
    static uint64_t ping_seq = 1;
    int p = 0;

    p = pb_put_varint_field(out, p, cap, 1, ping_seq++);
    if (p < 0) {
        return -1;
    }
    p = pb_put_varint_field(out, p, cap, 2, 0);
    if (p < 0) {
        return -1;
    }
    p = pb_put_varint_field(out, p, cap, 3, (uint32_t)svc_id);
    if (p < 0) {
        return -1;
    }
    p = pb_put_varint_field(out, p, cap, 4, 0); /* control */
    if (p < 0) {
        return -1;
    }
    p = pb_encode_header_entry(out, p, cap, "type", "ping");
    if (p < 0) {
        return -1;
    }
    return p;
}
