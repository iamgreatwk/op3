#!/bin/sh

# Kbuild calls date without arguments when composing UTS_VERSION. Keep normal
# date behavior for every other invocation and override only that one call.
if [ "$#" -eq 0 ] && [ -n "${OP3_REPRO_BUILD_TIMESTAMP-}" ]; then
    printf '%s\n' "$OP3_REPRO_BUILD_TIMESTAMP"
    exit 0
fi

exec /usr/bin/date "$@"
