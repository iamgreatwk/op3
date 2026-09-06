#!/bin/sh
# ============================================================
# OnePlus 3 recovery/browser integration launcher (Issue #6).
#
# This is the inittab respawn entry for the pmOS 6.12.1 product image.  The
# recovery program and its libtsm-linked assets live on the persistent sda15
# rootfs so the boot image remains below the device size limit.  Recovery is
# the foreground application; a browser session is started later by the
# `browser` command inside its PTY shell and returns to this process's UI.
# ============================================================

LOG=/var/log/op3-recovery.log
PERSIST_DIR=/newroot/var/log
PERSIST=$PERSIST_DIR/op3-recovery.log
RECOVERY=/newroot/sbin/recovery_mainline
WAIT_LIMIT=120
GPU_POWER=/sys/bus/platform/devices/b00000.gpu/power/control

mkdir -p /var/log
: > "$LOG"

log(){
	line="$(date '+%H:%M:%S') $*"
	printf '%s\n' "$line" >> "$LOG"
	printf '%s\n' "op3-recovery: $line" > /dev/kmsg 2>/dev/null
}

sync_log(){
	[ -d /newroot ] || return 0
	mkdir -p "$PERSIST_DIR" 2>/dev/null
	cat "$LOG" > "$PERSIST" 2>/dev/null
	sync
}

log "recovery launcher start pid=$$"

# MSM8996's GPU node currently has dummy vdd/vddcx regulators.  If runtime PM
# suspends the GPU before a later browser launch, changing control=auto to
# control=on can resume it without a real power sequence and hard-reset the
# SoC.  Keep the GPU awake from recovery startup so the browser handoff is an
# idempotent control write instead of the first runtime resume.  Restore to
# auto only after the DTB regulator fix is available.
if [ -f "$GPU_POWER" ]; then
	echo on > "$GPU_POWER" 2>/dev/null
	log "GPU runtime PM disabled before recovery: control=$(cat "$GPU_POWER" 2>/dev/null)"
else
	log "GPU runtime PM: $GPU_POWER not found"
fi

waited=0
while [ ! -x "$RECOVERY" ] && [ "$waited" -lt "$WAIT_LIMIT" ]; do
	sleep 1
	waited=$((waited + 1))
done

if [ ! -x "$RECOVERY" ]; then
	log "FATAL: $RECOVERY is missing or not executable after ${waited}s"
	mount | grep -E 'sda15|newroot' >> "$LOG" 2>&1
	ls -l /newroot /newroot/sbin >> "$LOG" 2>&1
	sync_log
	# Keep the respawn entry alive without creating a rapid inittab loop.  The
	# owner can inspect the persistent log over SSH/ACM and fix sda15.
	while :; do sleep 3600; done
fi

log "executing $RECOVERY after ${waited}s"
sync_log
exec "$RECOVERY" "$@"
