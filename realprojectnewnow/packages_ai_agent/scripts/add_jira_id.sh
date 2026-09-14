#!/bin/bash
# Reads commit message from stdin, adds JIRA ID based on content, writes to stdout.
# Classification rules (customize JIRA IDs below):
#   weixin/wechat/ilink -> JIRA_WEIXIN
#   voice/asr/tts/audio/media -> JIRA_VOICE
#   feat* (new features) -> JIRA_FEAT
#   everything else -> JIRA_MISC

# --- Configure your JIRA IDs here ---
JIRA_WEIXIN="${JIRA_WEIXIN:-PROJ-0001}"
JIRA_VOICE="${JIRA_VOICE:-PROJ-0002}"
JIRA_FEAT="${JIRA_FEAT:-PROJ-0003}"
JIRA_MISC="${JIRA_MISC:-PROJ-0004}"
JIRA_PREFIX="${JIRA_PREFIX:-PROJ}"
# -------------------------------------

MSG=$(cat)
FIRST_LINE=$(echo "$MSG" | head -1)

# Skip if already has a JIRA ID
if echo "$MSG" | grep -qiE "${JIRA_PREFIX}-"; then
    echo "$MSG"
    exit 0
fi

LOWER=$(echo "$FIRST_LINE" | tr '[:upper:]' '[:lower:]')

# 1. weixin
if echo "$LOWER" | grep -qiE 'weixin|wechat|ilink'; then
    JIRA="$JIRA_WEIXIN"
# 2. audio/voice
elif echo "$LOWER" | grep -qiE 'voice|asr|tts|audio|media|music|playback|pcm|volcengine'; then
    JIRA="$JIRA_VOICE"
# 3. feat or LLM-router feature
elif echo "$LOWER" | grep -qE '^feat|llm router|multi-backend'; then
    JIRA="$JIRA_FEAT"
# 4. everything else
else
    JIRA="$JIRA_MISC"
fi

# Insert JIRA ID after first line
echo "$FIRST_LINE"
echo ""
echo "$JIRA"
REST=$(echo "$MSG" | tail -n +2)
if [ -n "$REST" ]; then
    echo ""
    echo "$REST"
fi
