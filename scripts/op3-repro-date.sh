#!/bin/sh
# Provide the locked build timestamp for Kbuild's final utsversion.h step.
# The script is normally exposed as "date" through a temporary PATH entry.

if [ "$#" -eq 0 ] && [ -n "${OP3_REPRO_BUILD_TIMESTAMP-}" ]; then
	printf '%s\n' "$OP3_REPRO_BUILD_TIMESTAMP"
	exit 0
fi

exec /usr/bin/date "$@"
