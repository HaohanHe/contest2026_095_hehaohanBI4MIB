#!/bin/sh
# AI Radio Console REST API demo script
# Run this on the Gemini-S1 NSH shell or from a host with curl.

IP=${1:-127.0.0.1}
PORT=28790
BASE="http://$IP:$PORT"

echo "== AI Radio Console API Demo =="
echo "Target: $BASE"
echo ""

echo "== GET /a2a/health =="
curl -s "$BASE/a2a/health" | head -c 1000
echo ""
echo ""

echo "== GET /api/config =="
curl -s "$BASE/api/config" | head -c 1000
echo ""
echo ""

echo "== GET /api/skills =="
curl -s "$BASE/api/skills" | head -c 1000
echo ""
echo ""

echo "== GET /api/logs =="
curl -s "$BASE/api/logs" | head -c 1000
echo ""
echo ""

echo "== PUT /api/config (example) =="
curl -s -X PUT "$BASE/api/config" \
  -H "Content-Type: application/json" \
  -d '{"asr_model":"iic/SenseVoiceSmall"}'
echo ""
echo ""

echo "== Done =="
