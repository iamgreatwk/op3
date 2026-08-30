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
#   bin/        kmscube or another EGL test program
#   lib/        libEGL, libGLESv2, libgbm, libdrm, libglapi, ...
#   lib/dri/    msm_dri.so (freedreno)
# ============================================================

BASE=$(dirname "$0")

export LD_LIBRARY_PATH="$BASE/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export LIBGL_DRIVERS_PATH="$BASE/lib/dri"
export EGL_LOG_LEVEL=debug
export MESA_DEBUG=1
unset DISPLAY WAYLAND_DISPLAY XDG_RUNTIME_DIR

TEST_BIN=${EGL_TEST_BIN:-$BASE/bin/kmscube}
TEST_SECONDS=${EGL_TEST_SECONDS:-30}

echo "egl bundle:        $BASE"
echo "LD_LIBRARY_PATH:   $LD_LIBRARY_PATH"
echo "LIBGL_DRIVERS_PATH:$LIBGL_DRIVERS_PATH"
echo "test program:      $TEST_BIN"
echo "hold seconds:      $TEST_SECONDS"
echo "lib:"
ls -l "$BASE/lib"
echo "lib/dri:"
ls -l "$BASE/lib/dri" 2>&1
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
"$TEST_BIN" &
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
