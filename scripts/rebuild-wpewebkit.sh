#!/usr/bin/env bash
set -euo pipefail

# Owner helper: full clean rebuild of wpewebkit (fixes the mixed-object
# renderer crash). Run from anywhere; takes 1-2 h with ccache.
# Log: /tmp/bd-webkit-clean.log
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
O="$ROOT/out/buildroot-op3-egl"
export PATH="$HOME/gnubin:$PATH"

echo "== 1/3 wpewebkit-dirclean =="
make -C "$ROOT/source/buildroot" O="$O" wpewebkit-dirclean

echo "== 2/3 full rebuild =="
make -C "$ROOT/source/buildroot" O="$O" BR2_JLEVEL=3 2>&1 | tee /tmp/bd-webkit-clean.log

echo "== 3/3 verify =="
test -f "$O/target/usr/bin/WPEWebDriver" && echo "WPEWebDriver: OK"
ls -l "$O/target/usr/lib/libWPEWebKit-2.0.so.1.6.10"
echo "done - tell the agent to redeploy and verify"
