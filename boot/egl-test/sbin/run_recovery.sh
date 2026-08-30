#!/bin/sh
# ============================================================
# OnePlus 3 EGL gate launcher (initramfs side).
#
# Deliberately tiny. It waits for the persistent sda15 root filesystem and then
# hands control to /newroot/opt/op3-egl/run.sh, which lives on sda15. The Mesa
# stack, the EGL test program, the test logic and the logs therefore live on
# sda15 and can be updated without repacking or re-flashing the boot image,
# which the OnePlus 3 boot.img size limit requires (docs/decisions.md).
#
# Deployed as sbin/run_recovery.sh in a derived initramfs, so
# recovery_mainline never starts and no other client can hold the DRM master.
#
# The inittab entry is ::respawn:, so this script must never exit quickly.
# ============================================================

LOG=/var/log/op3-egl.log
PERSIST_DIR=/newroot/var/log
PERSIST=$PERSIST_DIR/op3-egl.log
BUNDLE_DIR=/newroot/opt/op3-egl
BUNDLE_RUN=$BUNDLE_DIR/run.sh
WAIT_LIMIT=120

mkdir -p /var/log
: > "$LOG"

log() {
	line="$(date '+%H:%M:%S') $*"
	printf '%s\n' "$line" >> "$LOG"
	{ printf '%s\n' "op3-egl: $line" > /dev/kmsg; } 2>/dev/null
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

log "EGL launcher start pid=$$"

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
	snapshot "before EGL test"
	"$BUNDLE_RUN" >> "$LOG" 2>&1
	log "bundle exit=$?"
	snapshot "after EGL test"
fi

log "launcher finished; idling. Edit $BUNDLE_RUN on sda15 and reboot to retry."
sync_log
while :; do
	sleep 3600
done
