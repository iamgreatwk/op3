#!/bin/sh
# OP3-AUDIO-MIC-001 initramfs launcher.
#
# The firmware-provenance control initramfs already has tinycap/tinymix/
# tinyplay, glibc and libtinyalsa.  This launcher therefore adds only the
# route helper, waits for the persistent sda15 mount, runs the read-only ALSA
# diagnostic once, and leaves the system available for owner-directed routing.

set -u

LOG=/var/log/op3-audio-initramfs.log
PERSIST_DIR=/newroot/var/log
PERSIST_LOG=$PERSIST_DIR/op3-audio-initramfs.log
HELPER=/opt/op3-audio/route.sh
WAIT_LIMIT=120

mkdir -p /var/log
: > "$LOG"

log() {
	line="$(date '+%H:%M:%S') $*"
	printf '%s\n' "$line" >> "$LOG"
	printf '%s\n' "op3-audio: $line" > /dev/kmsg 2>/dev/null || true
}

sync_log() {
	if [ -d /newroot ]; then
		mkdir -p "$PERSIST_DIR" 2>/dev/null || true
		cp "$LOG" "$PERSIST_LOG" 2>/dev/null || true
		sync
	fi
}

log "initramfs audio diagnostic launcher start pid=$$"

waited=0
while [ ! -d /newroot ] && [ "$waited" -lt "$WAIT_LIMIT" ]; do
	sleep 1
	waited=$((waited + 1))
done

if [ ! -d /newroot ]; then
	log "FAIL: /newroot did not become available after ${WAIT_LIMIT}s"
	sync_log
else
	log "persistent root available after ${waited}s"
	if [ ! -x "$HELPER" ]; then
		log "FAIL: missing diagnostic helper $HELPER"
	else
		log "running read-only ALSA diagnosis"
		OP3_AUDIO_LOG="$LOG" "$HELPER" diagnose >> "$LOG" 2>&1
		log "diagnostic exit=$?"
	fi
	sync_log
fi

log "diagnostic complete; idling for owner SSH/ACM inspection"
sync_log
while :; do
	sleep 3600
done
