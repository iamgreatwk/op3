#!/bin/sh
set -eu

preview="${1:-/tmp/op3-v4l2-live-preview}"
if [ "$#" -gt 0 ]; then
	shift
fi
flag=/run/op3-browser.active
ready=/run/op3-browser.recovery-ready

cleanup() {
	rm -f "$flag" "$ready"
}
trap cleanup EXIT INT TERM

rm -f "$ready"
printf '%s\n' "$$" > "$flag"
for _ in $(seq 1 100); do
	[ -e "$ready" ] && break
	sleep 0.05
done
[ -e "$ready" ] || {
	echo 'recovery did not release DRM' >&2
	exit 1
}

"$preview" "$@"

