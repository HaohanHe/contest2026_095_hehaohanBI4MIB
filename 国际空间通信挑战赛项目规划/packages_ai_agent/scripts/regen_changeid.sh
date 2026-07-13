#!/bin/bash
# Strip existing Change-Id, then let the Gerrit hook add a new one.
# Used as msg-filter in git filter-branch.
HOOK="$(git rev-parse --git-dir)/hooks/commit-msg"

# Read message, strip old Change-Id line
MSG=$(cat | sed '/^Change-Id: /d')

# Write to temp file, run hook to generate new Change-Id
TMPFILE=$(mktemp)
echo "$MSG" > "$TMPFILE"
"$HOOK" "$TMPFILE"
cat "$TMPFILE"
rm -f "$TMPFILE"
