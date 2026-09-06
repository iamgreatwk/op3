#!/bin/sh
# Called by the validated init_mainline.sh after /newroot is mounted.
# The package and credentials both live on sda15, so a Wi-Fi profile update
# needs neither an initramfs rebuild nor a boot image change. IPv6 is opt-in;
# the persistent CLI exposes `wifi ipv6 on` for an explicit enable.

/newroot/opt/op3-wifi/wifi ipv6 off || exit $?
exec /newroot/opt/op3-wifi/wifi auto
