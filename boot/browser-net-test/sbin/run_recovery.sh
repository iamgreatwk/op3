#!/bin/sh
# ============================================================
# OnePlus 3 Browser network gate launcher (OP3-BROWSER-005) (initramfs side).
#
# Deliberately tiny. It waits for the persistent sda15 root filesystem and then
# hands control to /newroot/opt/op3-browser/run.sh, which lives on sda15. The Mesa
# stack, the Browser test program, the test logic and the logs therefore live on
# sda15 and can be updated without repacking or re-flashing the boot image,
# which the OnePlus 3 boot.img size limit requires (docs/decisions.md).
#
# Deployed as sbin/run_recovery.sh in a derived initramfs, so
# recovery_mainline never starts and no other client can hold the DRM master.
#
# The inittab entry is ::respawn:, so this script must never exit quickly.
# ============================================================

LOG=/var/log/op3-browser-net.log
PERSIST_DIR=/newroot/var/log
PERSIST=$PERSIST_DIR/op3-browser-net.log
BUNDLE_DIR=/newroot/opt/op3-browser
BUNDLE_RUN=$BUNDLE_DIR/run.sh
WAIT_LIMIT=120

mkdir -p /var/log
: > "$LOG"

# Networked browser gate (OP3-BROWSER-005): run.sh (on sda15) brings up Wi-Fi
# via /newroot/opt/op3-wifi/wifi auto, then cog loads the remote URL.
# Phase 1 target is plain HTTP (https needs a Buildroot rebuild with
# glib-networking; see buildroot/op3-browser.defconfig).
export OP3_BROWSER_NET=1
export OP3_BROWSER_URL="${OP3_BROWSER_URL:-http://www.baidu.com}"

log() {
	line="$(date '+%H:%M:%S') $*"
	printf '%s\n' "$line" >> "$LOG"
	{ printf '%s\n' "op3-browser-net: $line" > /dev/kmsg; } 2>/dev/null
}

snapshot() {
	echo "----- $1 -----" >> "$LOG"
	echo "kernel: $(uname -a)" >> "$LOG"
	ls -l /dev/dri >> "$LOG" 2>&1
	for d in /sys/class/drm/card0-*; do
		[ -e "$d" ] || continue
		printf '%s status=%s enabled=%s mode=%s\n' "$d" \
			"$(cat "$d/status" 2>/dev/null)" \
			"$(cat "$d/enabled" 2>/dev/null)" \
			"$(head -n 1 "$d/modes" 2>/dev/null)" >> "$LOG"
	done
	echo "dmesg display/gpu lines:" >> "$LOG"
	dmesg 2>/dev/null | grep -i -E "drm|msm|dsi|adreno|a530|gpu" | tail -n 25 >> "$LOG"
	sync_log
}

sync_log() {
	if [ -d /newroot ]; then
		mkdir -p "$PERSIST_DIR" 2>/dev/null
		cat "$LOG" > "$PERSIST" 2>/dev/null
		sync
	fi
}

start_acm_console() {
	# Debugging over the USB ACM port, best effort. Two problems make
	# console=ttyGS0 useless on this control image (docs/known-issues.md):
	# 1. CONFIG_U_SERIAL_CONSOLE is not set, so the kernel argument never
	#    registers ttyGS0 as a console (/proc/consoles shows only tty0 and
	#    ramoops-1); init output and panics cannot reach the host.
	# 2. The pmOS initramfs ships a stale placeholder /dev/ttyGS0 REGULAR
	#    file (not a character device), so even user-space writes silently
	#    disappear into the initramfs tmpfs.
	# Workaround: recreate the real character device from sysfs, relay the
	# kernel log with a blocking /dev/kmsg reader, and spawn a debug shell
	# on the same port. Host side: kai must be able to open /dev/ttyACM0.
	w=0
	while [ ! -e /sys/class/tty/ttyGS0/dev ] && [ "$w" -lt 15 ]; do
		sleep 1
		w=$((w + 1))
	done
	if [ ! -e /sys/class/tty/ttyGS0/dev ]; then
		log "ACM debug: ttyGS0 not registered, skipping"
		return
	fi
	if [ ! -c /dev/ttyGS0 ]; then
		rm -f /dev/ttyGS0
		mknod /dev/ttyGS0 c \
			"$(cut -d: -f1 /sys/class/tty/ttyGS0/dev)" \
			"$(cut -d: -f2 /sys/class/tty/ttyGS0/dev)" 2>/dev/null
	fi
	if [ ! -c /dev/ttyGS0 ]; then
		log "ACM debug: failed to create /dev/ttyGS0"
		return
	fi
	setsid sh -c 'cat /dev/kmsg > /dev/ttyGS0 2>/dev/null < /dev/null' &
	setsid sh -c 'exec < /dev/ttyGS0 > /dev/ttyGS0 2>&1; exec sh' &
	log "ACM debug: kmsg relay and debug shell on /dev/ttyGS0 $(cat /sys/class/tty/ttyGS0/dev)"
}

log "Browser launcher start pid=$$"

# Globally disable A530 runtime PM, before anything else. Validated on 6.3.1
# over many runs: without this, GPU suspend/resume cycles blank the panel or
# hang the SoC. On this kernel a manual kmscube run after the GPU had
# runtime-suspended hard-reset the device with no console output
# (docs/known-issues.md). Setting control=on resumes the GPU (the same resume
# kmscube would trigger anyway) and keeps it permanently powered.
GPU_POWER=/sys/bus/platform/devices/b00000.gpu/power/control
if [ -f "$GPU_POWER" ]; then
	echo on > "$GPU_POWER" 2>/dev/null
	log "GPU runtime PM disabled: control=$(cat "$GPU_POWER" 2>/dev/null), cur_freq=$(cat /sys/class/devfreq/b00000.gpu/cur_freq 2>/dev/null)"
else
	log "GPU runtime PM: $GPU_POWER not found"
fi

start_acm_console

# Periodically capture the full kernel state into the log and sync it to
# sda15. If the kernel hangs/resets mid-run (the 7.x fastboot-stuck mystery),
# the last snapshot survives on sda15 and tells us how far the kernel got and
# what it printed before dying. This is the post-mortem channel when USB
# itself is broken (dwc3 probe defers on 7.x, suspected).
periodic_capture() {
	echo "----- periodic @ uptime=$(cut -d ' ' -f1 /proc/uptime 2>/dev/null) -----" >> "$LOG"
	dmesg | tail -n 120 >> "$LOG" 2>&1
	echo "--- ps ---" >> "$LOG"
	ps >> "$LOG" 2>&1
	sync_log
}
( while :; do sleep 5; periodic_capture; done ) & # log-sync

waited=0
while [ ! -f "$BUNDLE_RUN" ]; do
	if [ "$waited" -ge "$WAIT_LIMIT" ]; then
		break
	fi
	sleep 1
	waited=$((waited + 1))
done

if [ ! -f "$BUNDLE_RUN" ]; then
	log "FATAL: $BUNDLE_RUN not found after ${WAIT_LIMIT}s"
	mount | grep -E "sda15|newroot" >> "$LOG" 2>&1
	ls -l /newroot >> "$LOG" 2>&1
	sync_log
else
	log "bundle found after ${waited}s; executing $BUNDLE_RUN"
	snapshot "before Browser test"
	"$BUNDLE_RUN" >> "$LOG" 2>&1
	log "bundle exit=$?"
	snapshot "after Browser test"
fi

log "launcher finished; idling. Edit $BUNDLE_RUN on sda15 and reboot to retry."
sync_log
while :; do
	sleep 3600
done
