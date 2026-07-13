#!/bin/bash
# CI helper: take screenshots of all tabs
# Coordinates from pixel_5 API 31 uiautomator dump (parent clickable nodes)
set -e

mkdir -p screenshots

adb shell screencap -p /sdcard/screenshot.png
adb pull /sdcard/screenshot.png screenshots/01-provisioning.png

# Tab centers derived from UI dump bounds:
# 配置: [221,1988][420,2072] -> (320, 2030)
# 技能: [442,1988][640,2072] -> (541, 2030)
# 日志: [662,1988][860,2072] -> (761, 2030)
# 对话: [882,1988][1080,2072] -> (981, 2030)

echo "=== Config tab ==="
adb shell input tap 320 2030
sleep 3
adb shell screencap -p /sdcard/screenshot.png
adb pull /sdcard/screenshot.png screenshots/02-config.png

echo "=== Skills tab ==="
adb shell input tap 541 2030
sleep 3
adb shell screencap -p /sdcard/screenshot.png
adb pull /sdcard/screenshot.png screenshots/03-skills.png

echo "=== Logs tab ==="
adb shell input tap 761 2030
sleep 3
adb shell screencap -p /sdcard/screenshot.png
adb pull /sdcard/screenshot.png screenshots/04-logs.png

echo "=== Chat tab ==="
adb shell input tap 981 2030
sleep 3
adb shell screencap -p /sdcard/screenshot.png
adb pull /sdcard/screenshot.png screenshots/05-chat.png

echo "All screenshots captured."
