#!/bin/sh
# Run one foreground Wayland browser session from the recovery PTY.
#
# The shared flag tells recovery_mainline to release fb0 before Weston owns
# DRM.  The flag contains this supervisor PID; recovery can clear an orphaned
# flag after a killed session. READY is written only after the release.

FLAG=/run/op3-browser.active
READY=/run/op3-browser.recovery-ready
LOG=/newroot/var/log/op3-browser-session.log
RECOVERY_BIN=/newroot/opt/op3-recovery
READY_LIMIT=15

mkdir -p /run /newroot/var/log 2>/dev/null

log(){
	line="$(date '+%H:%M:%S') $*"
	printf '%s\n' "$line" >> "$LOG"
	printf '%s\n' "op3-browser-session: $line" > /dev/kmsg 2>/dev/null
}

engine=${1:-${OP3_BROWSER_ENGINE:-chromium}}
url=${2:-${OP3_BROWSER_URL:-}}

case "$engine" in
	cog|wpe)
		runner="$RECOVERY_BIN/cog-run.sh"
		[ "$engine" = wpe ] && engine=cog
		[ -n "$url" ] && export OP3_BROWSER_URL="$url"
		export OP3_BROWSER_BASE=/newroot/opt/op3-browser
		export OP3_BROWSER_ONESHOT=1
		;;
	chromium|chrome)
		runner="$RECOVERY_BIN/chromium-run.sh"
		[ "$engine" = chrome ] && engine=chromium
		[ -n "$url" ] && export OP3_CHROMIUM_URL="$url"
		export OP3_CHROMIUM_ONESHOT=1
		;;
	*)
		printf 'usage: browser [cog|chromium] [url]\n' >&2
		exit 2
		;;
esac

if [ ! -x "$runner" ]; then
	log "FATAL: $runner is missing; stage the Issue #6 recovery bundle"
	exit 1
fi

if [ -e "$FLAG" ]; then
	old_pid=$(head -n 1 "$FLAG" 2>/dev/null)
	if [ -n "$old_pid" ] && kill -0 "$old_pid" 2>/dev/null; then
		log "browser session already active pid=$old_pid"
		exit 1
	fi
	rm -f "$FLAG"
fi

rm -f "$READY"
printf '%s\n' "$$" > "$FLAG"
runner_pid=""
cleanup(){
	rc=$?
	trap - EXIT INT TERM
	if [ -n "$runner_pid" ]; then
		kill -TERM "$runner_pid" 2>/dev/null
		wait "$runner_pid" 2>/dev/null
		runner_pid=""
	fi
	rm -f "$FLAG"
	rm -f "$READY"
	log "session cleanup engine=$engine rc=$rc"
	sync
	exit "$rc"
}
trap cleanup EXIT INT TERM

log "session start engine=$engine url=${url:-bundle default} supervisor=$$"
# recovery_mainline closes its fbdev mapping and writes READY.  Do not start
# Weston until that marker exists; otherwise the browser can race the old
# recovery DRM/fb0 client and reset the device before any child log is flushed.
sync
ready_wait=0
while [ ! -e "$READY" ] && [ "$ready_wait" -lt "$READY_LIMIT" ]; do
	sleep 1
	ready_wait=$((ready_wait + 1))
done
if [ ! -e "$READY" ]; then
	log "FATAL: recovery framebuffer handoff timeout after ${ready_wait}s"
	exit 1
fi
log "recovery framebuffer released after ${ready_wait}s"
sync
"$runner" &
runner_pid=$!
wait "$runner_pid" 2>/dev/null
rc=$?
runner_pid=""
log "runner exited engine=$engine rc=$rc"
exit "$rc"
