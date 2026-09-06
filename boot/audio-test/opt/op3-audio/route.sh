#!/bin/sh
# OP3 audio route diagnostic helper.
#
# The integrated recovery binary programs the same controls for the voice-key
# path. This helper is kept in the persistent audio bundle so an owner can
# inspect and select the routes independently of recovery_mainline.
set -eu

PATH=/usr/bin:/usr/sbin:/bin:/sbin
LOG=${OP3_AUDIO_LOG:-/newroot/var/log/op3-audio-route.log}

mkdir -p "$(dirname "$LOG")"

log() {
	printf '%s op3-audio: %s\n' "$(date '+%H:%M:%S')" "$*" | tee -a "$LOG"
}

need() {
	command -v "$1" >/dev/null 2>&1 || {
		log "missing required program: $1"
		exit 1
	}
}

control_exists() {
	tinymix -D 0 get "$1" >/dev/null 2>&1
}

set_control() {
	name=$1
	shift
	if ! control_exists "$name"; then
		log "required mixer control missing: $name"
		exit 1
	fi
	log "tinymix -D 0 set $name $*"
	tinymix -D 0 set "$name" "$@" >>"$LOG" 2>&1
}

find_pcm() {
	direction=$1
	awk -v direction="$direction" '
		$0 ~ /MultiMedia3/ && $0 ~ direction {
			id = $1; sub(/:$/, "", id); split(id, pair, "-");
			printf "hw:%d,%d\\n", pair[1] + 0, pair[2] + 0; exit
		}
	' /proc/asound/pcm
}

diagnose() {
	need tinymix
	log "kernel=$(uname -r)"
	log "sound nodes:"
	ls -l /dev/snd >>"$LOG" 2>&1 || true
	cat /proc/asound/cards >>"$LOG" 2>&1 || true
	cat /proc/asound/pcm >>"$LOG" 2>&1 || true
	log "relevant mixer controls:"
	tinymix -D 0 controls | grep -E 'MultiMedia3|QUAT_MI2S|SLIMBUS_0_TX|SLIM TX4|AIF1_CAP|ADC MUX4|ADC4' \
		>>"$LOG" 2>&1 || true
	if [ ! -e /dev/snd/controlC0 ]; then
		log "missing /dev/snd/controlC0"
		exit 1
	fi
	log "MultiMedia3 playback PCM: $(find_pcm playback || true)"
	log "MultiMedia3 capture PCM: $(find_pcm capture || true)"
}

speaker() {
	need tinymix
	set_control "QUAT_MI2S_RX Audio Mixer MultiMedia3" 1
}

mic() {
	set_control "ADC MUX4" AMIC
	set_control "SLIM TX4 MUX" DEC4
	set_control "AIF1_CAP Mixer SLIM TX4" 1
	set_control "MultiMedia3 Mixer SLIMBUS_0_TX" 1
}

case "${1:-diagnose}" in
	diagnose) diagnose ;;
	speaker) speaker ;;
	mic) mic ;;
	all) diagnose; speaker; mic ;;
	*)
		printf 'Usage: %s {diagnose|speaker|mic|all}\n' "$0" >&2
		exit 2
		;;
esac
