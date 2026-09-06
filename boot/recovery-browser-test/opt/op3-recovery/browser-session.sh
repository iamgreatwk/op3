#!/bin/sh
# Run one foreground Wayland browser session from the recovery PTY.
#
# Recovery keeps its framebuffer and input descriptors open, so the shared
# flag tells recovery_mainline to stop consuming input and submitting fb0
# frames while Weston owns DRM.  The flag contains this supervisor PID; the
# recovery program can clear an orphaned flag after a killed session.

FLAG=/run/op3-browser.active
LOG=/newroot/var/log/op3-browser-session.log
RECOVERY_BIN=/newroot/opt/op3-recovery

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
		return 2 2>/dev/null || exit 2
		;;
esac

if [ ! -x "$runner" ]; then
	log "FATAL: $runner is missing; stage the Issue #6 recovery bundle"
	return 1 2>/dev/null || exit 1
fi

if [ -e "$FLAG" ]; then
	old_pid=$(head -n 1 "$FLAG" 2>/dev/null)
	if [ -n "$old_pid" ] && kill -0 "$old_pid" 2>/dev/null; then
		log "browser session already active pid=$old_pid"
		return 1 2>/dev/null || exit 1
	fi
	rm -f "$FLAG"
fi

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
	log "session cleanup engine=$engine rc=$rc"
	sync
	exit "$rc"
}
trap cleanup EXIT INT TERM

log "session start engine=$engine url=${url:-bundle default} supervisor=$$"
"$runner" &
runner_pid=$!
wait "$runner_pid" 2>/dev/null
rc=$?
runner_pid=""
log "runner exited engine=$engine rc=$rc"
exit "$rc"
