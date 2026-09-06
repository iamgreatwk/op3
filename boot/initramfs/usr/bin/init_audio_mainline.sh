#!/bin/sh
# Mainline OP3 audio initialization for the self-built initramfs.
#
# The sound card appears asynchronously while ADSP/WCD9335 firmware is
# brought up. Wait without blocking PID 1, then use the same controls as the
# verified recovery voice path. Audio firmware is staged into the Buildroot
# target by scripts/stage-op3-initramfs-firmware.sh.

LOG=/tmp/op3-audio-init.log
log() {
    printf '%s audio-init: %s\n' "$(date '+%H:%M:%S')" "$*" >> "$LOG"
}

log 'start'
i=0
while [ ! -e /dev/snd/controlC0 ] && [ "$i" -lt 90 ]; do
    sleep 1
    i=$((i + 1))
done

if [ ! -e /dev/snd/controlC0 ]; then
    log 'FAIL: /dev/snd/controlC0 did not appear within 90s'
    dmesg 2>/dev/null | grep -iE 'adsp|remoteproc|wcd|msm_snd|slimbus|audio' \
        | tail -20 >> "$LOG" 2>/dev/null || true
    exit 1
fi
log "sound card ready after ${i}s"

if [ -x /opt/op3-audio/route.sh ]; then
    OP3_AUDIO_LOG=/tmp/op3-audio-route.log /opt/op3-audio/route.sh all \
        >> "$LOG" 2>&1
    rc=$?
else
    log 'FAIL: /opt/op3-audio/route.sh is missing'
    rc=1
fi
log "route rc=${rc}"
exit "${rc}"
