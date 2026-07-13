#!/usr/bin/env python3
"""
HTTP-to-HTTPS relay for AI Radio Console ASR.

The board's minimal ASR client speaks plain HTTP. Most cloud ASR services
require HTTPS. Run this relay on a PC/phone in the same LAN:

    python3 relay.py --target https://api.siliconflow.cn/v1/audio/transcriptions

Then configure the board's ASR_URL to point to:

    http://<relay-ip>:8080/v1/audio/transcriptions

The relay forwards multipart/form-data POSTs to the real HTTPS endpoint
and streams the response back.
"""

import argparse
import http.server
import urllib.request
import urllib.parse
import ssl


class RelayHandler(http.server.BaseHTTPRequestHandler):
    target_base = None

    def log_message(self, fmt, *args):
        print(f"[relay] {self.address_string()} {fmt % args}")

    def do_GET(self):
        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.end_headers()
        self.wfile.write(b"AI Radio Console ASR relay is running\n")

    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(length)

        target = self.target_base + self.path
        print(f"[relay] POST -> {target}")

        req = urllib.request.Request(
            target,
            data=body,
            headers={
                "Content-Type": self.headers.get("Content-Type", "multipart/form-data"),
                "Authorization": self.headers.get("Authorization", ""),
            },
            method="POST",
        )

        ctx = ssl.create_default_context()
        try:
            with urllib.request.urlopen(req, context=ctx, timeout=60) as resp:
                self.send_response(resp.status)
                for k, v in resp.headers.items():
                    if k.lower() in ("content-length", "content-type"):
                        self.send_header(k, v)
                self.end_headers()
                while True:
                    chunk = resp.read(4096)
                    if not chunk:
                        break
                    self.wfile.write(chunk)
        except urllib.error.HTTPError as e:
            self.send_response(e.code)
            self.end_headers()
            self.wfile.write(e.read())
        except Exception as e:
            print(f"[relay] error: {e}")
            self.send_response(502)
            self.end_headers()
            self.wfile.write(f"{{\"error\":\"relay failed: {e}\"}}".encode())


def main():
    parser = argparse.ArgumentParser(description="AI Radio Console ASR relay")
    parser.add_argument(
        "--target",
        default="https://api.siliconflow.cn/v1/audio/transcriptions",
        help="Target HTTPS ASR endpoint",
    )
    parser.add_argument("--port", type=int, default=8080, help="Local HTTP port")
    parser.add_argument("--host", default="0.0.0.0", help="Local bind address")
    args = parser.parse_args()

    RelayHandler.target_base = args.target.rsplit("/", 1)[0]
    server = http.server.HTTPServer((args.host, args.port), RelayHandler)
    print(f"[relay] Listening http://{args.host}:{args.port}")
    print(f"[relay] Forwarding POSTs to {args.target}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n[relay] Stopping")


if __name__ == "__main__":
    main()
