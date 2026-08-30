#!/bin/sh
# ============================================================
# OnePlus 3 EGL gate: test logic.
#
# This file lives on sda15 at /newroot/opt/op3-egl/run.sh, so it can be edited
# without repacking or re-flashing the boot image. The initramfs launcher only
# waits for it and executes it.
#
# Expected layout next to this file (produced by scripts/stage-egl-rootfs.sh
# from a Buildroot output/target):
#   bin/            kmscube
#   lib/            libEGL, libGLESv2, libgbm, libdrm, libgallium-<ver>.so,
#                   libc and friends, and the glibc loader ld-linux-*.so
#   lib/gbm/        dri_gbm.so (libgbm's DRI backend)
#
# Mesa 26 note: there is no LIBGL_DRIVERS_PATH/msm_dri.so any more; the drivers
# are inside libgallium-<ver>.so. The bundle carries its own glibc loader, so
# the test is invoked through it explicitly and does not care which libc the
# sda15 rootfs uses.
# ============================================================

BASE=$(dirname "$0")

export LD_LIBRARY_PATH="$BASE/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export GBM_BACKENDS_PATH="$BASE/lib/gbm"
export EGL_LOG_LEVEL=debug
export MESA_DEBUG=1
unset DISPLAY WAYLAND_DISPLAY XDG_RUNTIME_DIR

TEST_BIN=${EGL_TEST_BIN:-$BASE/bin/kmscube}
TEST_SECONDS=${EGL_TEST_SECONDS:-30}
LOADER=$(ls "$BASE/lib/ld-linux-"*.so* 2>/dev/null | head -n 1)

echo "egl bundle:         $BASE"
echo "LD_LIBRARY_PATH:    $LD_LIBRARY_PATH"
echo "GBM_BACKENDS_PATH:  $GBM_BACKENDS_PATH"
echo "test program:       $TEST_BIN"
echo "hold seconds:       $TEST_SECONDS"
echo "loader:             ${LOADER:-<bundle has none; using the system one>}"
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

echo "=== starting $TEST_BIN ==="
if [ -n "$LOADER" ]; then
	"$LOADER" --library-path "$BASE/lib" "$TEST_BIN" &
else
	"$TEST_BIN" &
fi
test_pid=$!
sleep "$TEST_SECONDS"

if kill -0 "$test_pid" 2>/dev/null; then
	echo "test still alive after ${TEST_SECONDS}s (expected for kmscube); stopping it"
	kill -TERM "$test_pid" 2>/dev/null
	wait "$test_pid" 2>/dev/null
	echo "stopped=1"
else
	wait "$test_pid" 2>/dev/null
	echo "exit=$?"
fi
