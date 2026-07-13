#!/bin/sh
set +e

echo "[wifi] start_wifi"

WAPI_CONF="/data/etc/wifi/wapi.conf"

# 从配置文件读取参数（小写字段）
SSID=$(grep -oP '"ssid":\s*"\K[^"]+' $WAPI_CONF)
PSK=$(grep -oP '"psk":\s*"\K[^"]+' $WAPI_CONF)
BSSID=$(grep -oP '"bssid":\s*"\K[^"]+' $WAPI_CONF)

echo "[wifi] SSID=$SSID  BSSID=$BSSID"

# 断开连接
wapi disconnect wlan0
sleep 1

# 设置 SSID
if [ -n "$SSID" ]; then
    echo "[wifi] setting ssid: $SSID"
    wapi essid wlan0 "$SSID" 1
fi

# 设置 PSK（第四个参数 3 = WPA2）
if [ -n "$PSK" ]; then
    echo "[wifi] setting psk"
    wapi psk wlan0 "$PSK" 1 3
fi

# 设置固定 BSSID（5GHz）
if [ -n "$BSSID" ]; then
    echo "[wifi] setting bssid: $BSSID"
    wapi ap wlan0 $BSSID
fi

# 锁定 5GHz 频率（Channel 44 → 5220MHz）
#echo "[wifi] lock 5GHz freq 5220"
#wapi freq wlan0 5220 1

# 关闭自适应
echo "[wifi] disable adaptive mode"
wapi private wlan0 adaptivity 0

# 关闭省电
echo "[wifi] disable power save"
wapi power_save wlan0 off

# 保存配置
wapi save_config wlan0

# 重连
echo "[wifi] reconnecting..."
wapi reconnect wlan0

# 等待 WLAN ready
sleep 5

# 尝试 DHCP（最多尝试 3 次）
for i in 1 2 3; do
    echo "[wifi] DHCP renew try $i"
    renew wlan0
    if [ "$?" = "0" ]; then
        break
    fi
    sleep 2
done

# 显示最终配置
wapi show wlan0
echo "[wifi] wifi startup done"