#!/bin/sh
# tests/browser/op3-automation-stop.sh - stop the OP3 automation/browser stack.
# Run ON THE DEVICE. Kills weston/cog/WPEWebDriver/webkit helpers/dbus/udevd
# started by op3-automation-session.sh, restores GPU runtime PM to auto and
# lets fbcon take the display back (verified: text console returns on weston
# exit). USE THIS after every debug session - the GPU is forced on while the
# display stack runs and leaving it idle-cooking the phone is a real risk
# (device heat + battery drain, see docs/handoff heat protocol).
for p in /proc/[0-9]*; do
	[ "$p" = "/proc/$$" ] && continue
	c=$(tr '\0' ' ' < "$p/cmdline" 2>/dev/null) || continue
	case "$c" in
		*"/usr/bin/weston "*|*"/usr/bin/cog "*|*"/usr/bin/WPEWebDriver"*|*"/usr/libexec/wpe-webkit-2.0/"*|*"/usr/libexec/weston-"*|*"/usr/bin/dbus-daemon "*|*"/sbin/udevd "*)
			kill -9 "${p#/proc/}" 2>/dev/null ;;
	esac
done
sleep 1
# GPU back to runtime PM (was forced on by op3-automation-session.sh)
echo auto > /sys/bus/platform/devices/b00000.gpu/power/control 2>/dev/null
rm -f /run/op3-dbus /tmp/webdriver.log
echo "browser stack stopped; GPU power control=$(cat /sys/bus/platform/devices/b00000.gpu/power/control 2>/dev/null); fbcon owns the display"
