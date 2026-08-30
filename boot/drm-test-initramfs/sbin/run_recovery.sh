#!/bin/sh
# ============================================================
# OnePlus 3 direct DRM dumb-buffer test launcher.
#
# Deployed as sbin/run_recovery.sh inside a derived initramfs only. It replaces
# the recovery selector so that recovery_mainline never starts: that client
# would hold the DRM master and make the legacy SETCRTC ioctl fail with EACCES.
#
# Behaviour:
#   - waits for /dev/dri/card0
#   - runs op3-drm-dumb red, green, blue, then a long red hold for inspection
#   - stores its own output, the /dev/dri state and dmesg in
#     /var/log/op3-drm-dumb.log and mirrors them to
#     /newroot/var/log/op3-drm-dumb.log while sda15 is mounted at /newroot
#   - never exits: inittab respawns this entry, and a fast exit would make
#     busybox init disable the entry
#
# Kernel, DTB, /init, init_mainline.sh, inittab, cmdline and sda15 content are
# unchanged. USB RNDIS, the ACM shell and Dropbear keep working because
# init_mainline.sh still runs first.
# ============================================================

TEST=/usr/bin/op3-drm-dumb
LOG=/var/log/op3-drm-dumb.log
PERSIST_DIR=/newroot/var/log
PERSIST=$PERSIST_DIR/op3-drm-dumb.log
CARD=/dev/dri/card0
CARD_WAIT_SECONDS=90
COLOURS="red green blue"
HOLD_SECONDS=20
FINAL_HOLD_SECONDS=600

mkdir -p /var/log
: > "$LOG"
mkdir -p "$PERSIST_DIR" 2>/dev/null

log() {
	line="$(date '+%H:%M:%S') $*"
	printf '%s\n' "$line" >> "$LOG"
	printf '%s\n' "$line" >> "$PERSIST" 2>/dev/null
	{ printf '%s\n' "op3-drm-dumb: $line" > /dev/kmsg; } 2>/dev/null
}

sync_log() {
	if [ -d "$PERSIST_DIR" ]; then
		cat "$LOG" > "$PERSIST" 2>/dev/null
		sync
	fi
}

snapshot() {
	log "----- $1 -----"
	log "kernel: $(uname -a)"
	ls -l "$CARD" >> "$LOG" 2>&1
	ls -l /dev/dri >> "$LOG" 2>&1
	log "dmesg display lines:"
	dmesg 2>/dev/null | grep -i -E "drm|msm|dsi|panel|mdp|dsi_phy" | tail -n 25 >> "$LOG"
	log "dmesg tail:"
	dmesg 2>/dev/null | tail -n 15 >> "$LOG"
	sync_log
}

run_colour() {
	log "=== op3-drm-dumb $1 --seconds $2 ==="
	"$TEST" "$1" --seconds "$2" >> "$LOG" 2>&1
	log "exit=$?"
	snapshot "after $1"
}

log "launcher start pid=$$"
snapshot "boot state"

if [ ! -x "$TEST" ]; then
	log "FATAL: $TEST is missing or not executable"
else
	waited=0
	while [ ! -c "$CARD" ]; do
		if [ "$waited" -ge "$CARD_WAIT_SECONDS" ]; then
			break
		fi
		sleep 1
		waited=$((waited + 1))
	done

	if [ ! -c "$CARD" ]; then
		log "FATAL: $CARD did not appear within ${CARD_WAIT_SECONDS}s"
		snapshot "card wait timeout"
	else
		log "$CARD appeared after ${waited}s"
		for colour in $COLOURS; do
			run_colour "$colour" "$HOLD_SECONDS"
		done
		run_colour red "$FINAL_HOLD_SECONDS"
	fi
fi

log "sequence complete; idling. Re-flash to run the sequence again."
sync_log
while :; do
	sleep 3600
done
