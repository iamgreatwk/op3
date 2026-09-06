#!/usr/bin/env bash
set -euo pipefail

# Verify the single external-input directory used by the OP3 recovery rebuild.
# This is intentionally read-only and does not download, install, or modify
# any input.
#
# Usage:
#   scripts/verify-op3-external-inputs.sh /path/to/op3-recovery-external-inputs

external="${1:-${OP3_EXTERNAL_INPUTS:-}}"

if [ -z "$external" ]; then
	printf 'usage: scripts/verify-op3-external-inputs.sh <external-input-dir>\n' >&2
	exit 2
fi
external="$(readlink -f "$external")"
test -d "$external" || {
	printf 'Missing external-input directory: %s\n' "$external" >&2
	exit 1
}

for command in sha256sum sha512sum; do
	command -v "$command" >/dev/null || {
		printf 'Missing required command: %s\n' "$command" >&2
		exit 1
	}
done

for sums in SHA256SUMS SHA512SUMS; do
	test -f "$external/$sums" || {
		printf 'Missing checksum file: %s/%s\n' "$external" "$sums" >&2
		exit 1
	}
done

( cd "$external" && sha256sum --check SHA256SUMS )
( cd "$external" && sha512sum --check SHA512SUMS )

for tool in mcopy pil-squasher; do
	test -x "$external/tools/bin/$tool" || {
		printf 'Missing executable: %s/tools/bin/%s\n' "$external" "$tool" >&2
		exit 1
	}
done

printf 'External inputs verified: %s\n' "$external"
printf 'Use: export OP3_EXTERNAL_INPUTS=%q\n' "$external"
printf 'Use: export OP3_ATH10K_EXTFW_SOURCE=\"$OP3_EXTERNAL_INPUTS\"\n'
printf 'Use: export OP3_MCOPY=\"$OP3_EXTERNAL_INPUTS/tools/bin/mcopy\"\n'
printf 'Use: export OP3_PIL_SQUASHER=\"$OP3_EXTERNAL_INPUTS/tools/bin/pil-squasher\"\n'
