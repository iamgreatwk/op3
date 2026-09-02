#!/bin/sh
# OP3-AUDIO-MIC-001 runtime route helper.
#
# This script never assumes ALSA PCM device numbers.  It requires the expected
# q6routing/WCD9335 controls, logs each setting, and fails before opening a PCM
# if the card is incomplete.  Run "route.sh diagnose" first when a control
# name differs from the kernel's exported mixer list.
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

# DAPM enum labels have changed formatting in past tinyalsa releases.  Try the
# exact kernel control spelling first, but never silently skip the route.
set_control_any() {
	value=$1
	shift
	for name in "$@"; do
		if control_exists "$name"; then
			set_control "$name" "$value"
			return 0
		fi
	done
	log "none of the required mixer controls exists: $*"
	exit 1
}

find_pcm() {
	direction=$1
	awk -v direction="$direction" '
		$0 ~ /MultiMedia3/ && $0 ~ direction {
			id = $1; sub(/:$/, "", id); split(id, pair, "-");
			printf "hw:%d,%d\n", pair[1] + 0, pair[2] + 0; exit
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
	tinymix -D 0 controls | grep -E 'MultiMedia3|QUAT_MI2S|SLIMBUS_0_TX|SLIM TX4|ADC MUX4|ADC4' \
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
	# MultiMedia3 -> QDSP6 routing -> Quaternary MI2S -> TFA9890 speaker.
	set_control "QUAT_MI2S_RX Audio Mixer MultiMedia3" 1
}

mic() {
	need tinymix
	# AMIC4 -> ADC4/DEC4 -> SLIM TX4 -> SLIMBUS_0_TX -> MultiMedia3 capture.
	set_control_any AMIC4 "ADC MUX4" "ADC MUX4 Mux"
	set_control_any DEC4 "SLIM TX4 MUX" "SLIM TX4 MUX Mux"
	set_control "SLIM TX4" 1
	set_control "SLIMBUS_0_TX Audio Mixer MultiMedia3" 1
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
