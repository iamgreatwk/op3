#!/bin/sh
# Called by the validated init_mainline.sh after /newroot is mounted.
# The package and credentials both live on sda15, so a Wi-Fi profile update
# needs neither an initramfs rebuild nor a boot image change.

exec /newroot/opt/op3-wifi/wifi auto
