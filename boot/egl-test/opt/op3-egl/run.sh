#!/bin/sh
# ============================================================
# OnePlus 3 EGL gate: test logic.
#
# This file lives on sda15 at /newroot/opt/op3-egl/run.sh, so it can be edited
# without repacking or re-flashing the boot image. The initramfs launcher only
# waits for it and executes it.
#
# Expected layout next to this file (produced by scripts/stage-egl-rootfs.sh):
#   bin/            kmscube
#   lib/            libEGL, libGLESv2, libgbm, libdrm, libgallium-<ver>.so,
#                   glibc loader and libraries
#   lib/gbm/        dri_gbm.so (libgbm's DRI backend)
#
# CRITICAL: never export LD_LIBRARY_PATH here. The initramfs userland is
# dynamically linked against a different glibc build; putting the bundle's
# libraries on the library path makes every busybox applet this script calls
# (ls, cat, head) crash, and the test program then also mixes loaders and
# crashes with SIGBUS (observed 2026-08-30). Instead, everything the bundle
# needs is passed to the test program through the bundled glibc loader, and
# this script's own diagnostics run in a clean environment.
#
# Mesa 26 note: there is no LIBGL_DRIVERS_PATH/msm_dri.so any more; the drivers
# are inside libgallium-<ver>.so.
# ============================================================

BASE=$(dirname "$0")

export GBM_BACKENDS_PATH="$BASE/lib/gbm"
export EGL_LOG_LEVEL=debug
export MESA_DEBUG=1
unset DISPLAY WAYLAND_DISPLAY XDG_RUNTIME_DIR LD_LIBRARY_PATH

TEST_BIN=${EGL_TEST_BIN:-$BASE/bin/kmscube}
TEST_SECONDS=${EGL_TEST_SECONDS:-30}

# Pure-shell glob so that no external command runs before the loader is known.
set -- "$BASE"/lib/ld-linux-*.so*
LOADER=$1
[ -f "$LOADER" ] || LOADER=

echo "egl bundle:        $BASE"
echo "GBM_BACKENDS_PATH: $GBM_BACKENDS_PATH"
echo "test program:      $TEST_BIN"
echo "hold seconds:      $TEST_SECONDS"
echo "loader:            ${LOADER:-MISSING}"
echo "lib:"
ls -l "$BASE/lib"
echo "lib/gbm:"
ls -l "$BASE/lib/gbm" 2>&1
echo "/dev/dri:"
ls -l /dev/dri 2>&1

for d in /sys/class/drm/card0-*; do
	[ -e "$d" ] || continue
	printf '%s status=%s enabled=%s mode=%s\n' "$d" \
		"$(cat "$d/status" 2>/dev/null)" \
		"$(cat "$d/enabled" 2>/dev/null)" \
		"$(head -n 1 "$d/modes" 2>/dev/null)"
done

if [ ! -x "$TEST_BIN" ]; then
	echo "FATAL: $TEST_BIN is missing or not executable"
	exit 1
fi

if [ -z "$LOADER" ]; then
	echo "FATAL: no bundled dynamic loader in $BASE/lib"
	exit 1
fi

echo "=== loader identity ==="
"$LOADER" --version 2>&1 | head -n 2

# kmscube polls its stdin and exits with "user interrupted!" as soon as stdin
# is readable -- and /dev/null, a closed channel or a pipe EOF all count as
# readable. That made every earlier run exit after a single frame (observed
# 2026-08-30: one flash, then the plane was torn down with the fbs). Feed its
# stdin from a sleep so it stays blocked for the whole window; when the sleep
# ends, the EOF stops kmscube cleanly.
echo "=== starting $TEST_BIN via the bundled loader ==="
sleep "$TEST_SECONDS" | "$LOADER" --library-path "$BASE/lib" "$TEST_BIN" &
test_pid=$!

sleep "$((TEST_SECONDS + 5))"
if kill -0 "$test_pid" 2>/dev/null; then
	echo "test still running; stopping it"
	kill -TERM "$test_pid" 2>/dev/null
fi
wait "$test_pid" 2>/dev/null
echo "exit=$?"
