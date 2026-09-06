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

# The self-built CPIO also contains recovery_mainline, so a missing or
# unmounted persistent root still has a controlled recovery fallback.
if [ ! -x "$RECOVERY" ]; then
	RECOVERY=/sbin/recovery_mainline
fi
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
	grep -q ' on /newroot ' /proc/mounts 2>/dev/null || return 0
	mkdir -p "$PERSIST_DIR" 2>/dev/null
	cat "$LOG" > "$PERSIST" 2>/dev/null
	sync
}

log "recovery launcher start pid=$$"

# The default profile does not start a browser. Leave the GPU runtime-PM
# policy at the kernel default so the display path can suspend when idle.
# The optional browser-test launcher retains its separate GPU handoff policy.
if [ -f "$GPU_POWER" ]; then
	log "GPU runtime PM unchanged for recovery: control=$(cat "$GPU_POWER" 2>/dev/null)"
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
