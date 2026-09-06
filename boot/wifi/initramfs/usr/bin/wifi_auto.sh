#!/bin/sh
# Called by init_mainline.sh. The default CPIO root contains the same Wi-Fi
# CLI as the persistent payload; when sda15 is mounted, prefer its profiles
# and binaries so wifi connect remains persistent. IPv6 is opt-in.

if [ -x /newroot/opt/op3-wifi/wifi ]; then
	OP3_WIFI_ROOT=/newroot
	WIFI=/newroot/opt/op3-wifi/wifi
else
	OP3_WIFI_ROOT=/
	WIFI=/opt/op3-wifi/wifi
fi

export OP3_WIFI_ROOT
"$WIFI" ipv6 off || exit $?
exec "$WIFI" auto
