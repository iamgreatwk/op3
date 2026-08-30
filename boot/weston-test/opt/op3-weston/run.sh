#!/bin/sh
# ============================================================
# OnePlus 3 Wayland/Weston gate runner (lives on sda15).
#
# Layer 06: the compositor must come up on the panel with the GL renderer and
# composite a Wayland client (weston-simple-egl). This is the prerequisite for
# the browser (layer 07: Cog / WPE).
#
# Same deployment model as the EGL bundle: the initramfs launcher only waits
# for this file and executes it, so the test can be changed on sda15 without
# repacking or re-flashing the boot image.
#
# Layout next to this file (produced by scripts/stage-weston-rootfs.sh from a
# Buildroot output/target — the WHOLE target tree, not a NEEDED sweep, because
# weston's module directory and the data files are located through
# compile-time-absolute paths):
#   lib/            glibc loader ld-linux-*.so* and every library
#   usr/bin/        weston, weston-simple-egl, udevadm, ...
#   usr/lib/weston/ backend/renderer/shell modules
#   usr/lib/udev/   eudev daemon and rules
#   usr/share/      xkb data, libinput quirks
#   etc/            weston.ini
#
# CRITICAL (learned in the EGL gate, 2026-08-30): never export
# LD_LIBRARY_PATH — the initramfs busybox then crashes and the test program
# mixes loaders and dies with SIGBUS. Every bundle binary is invoked through
# the bundled glibc loader explicitly.
#
# Weston dlopens its backend/renderer/shell modules through
# compile-time-absolute paths (/usr/lib/weston), and libinput/libxkbcommon
# read their data from /usr/share/... — none of these can be redirected with
# an environment variable, so run.sh symlinks those paths on the live
# (ephemeral) initramfs rootfs into the bundle first.
# ============================================================

BASE=$(dirname "$0")

export GBM_BACKENDS_PATH="$BASE/usr/lib/gbm"
unset DISPLAY WAYLAND_DISPLAY XDG_RUNTIME_DIR LD_LIBRARY_PATH

TEST_SECONDS=${WESTON_TEST_SECONDS:-40}
CLIENT_SECONDS=${WESTON_CLIENT_SECONDS:-25}

# Pure-shell glob so that no external command runs before the loader is known.
set -- "$BASE"/lib/ld-linux-*.so*
LOADER=$1
[ -f "$LOADER" ] || LOADER=""

run() { # run <bundle-binary> [args...] — through the bundled loader
	echo "+ $*"
	if [ -n "$LOADER" ]; then
		"$LOADER" --library-path "$BASE/lib" "$@"
	else
		"$@"
	fi
}

echo "weston bundle:      $BASE"
echo "test program:       weston --backend=drm-backend.so"
echo "client:             weston-simple-egl"
echo "hold seconds:       $TEST_SECONDS (client $CLIENT_SECONDS)"
echo "loader:             ${LOADER:-MISSING}"

if [ -z "$LOADER" ]; then
	echo "FATAL: no bundled dynamic loader in $BASE/lib"
	exit 1
fi

# --- Bridge compile-time-absolute paths into the bundle ----------------------
# The live rootfs is the initramfs tmpfs; these links vanish on reboot.
mkdir -p /usr/lib /usr/share /run
for m in weston udev; do
	ln -sfn "$BASE/usr/lib/$m" "/usr/lib/$m"
done
for d in libinput X11 weston; do
	[ -e "$BASE/usr/share/$d" ] && ln -sfn "$BASE/usr/share/$d" "/usr/share/$d"
done

# --- udev: libinput needs the udev database to enumerate devices -------------
echo "=== starting udevd ==="
if [ -x "$BASE/usr/lib/udev/udevd" ]; then
	run "$BASE/usr/lib/udev/udevd" --daemon 2>&1
	run "$BASE/usr/bin/udevadm" trigger 2>&1 | tail -n 1
	run "$BASE/usr/bin/udevadm" settle --timeout=10 2>&1
	echo "udev: $(ls /run/udev/data 2>/dev/null | wc -l) device records"
else
	echo "WARN: no bundled udevd; libinput will find no devices (OK for smoke test)"
fi

# --- weston ------------------------------------------------------------------
# libseat: running as root, the builtin backend needs no seatd daemon.
export XDG_RUNTIME_DIR=/run/op3-weston
mkdir -p "$XDG_RUNTIME_DIR" && chmod 0700 "$XDG_RUNTIME_DIR"

echo "=== starting weston (DRM backend, GL renderer) ==="
run "$BASE/usr/bin/weston" \
	--backend=drm-backend.so \
	--idle-time=0 \
	--log="$XDG_RUNTIME_DIR/weston.log" &
weston_pid=$!

# Wait for the compositor to be ready before starting the client.
i=0
ready=""
while [ "$i" -lt 20 ]; do
	if grep -q "compositor ready\|waiting for clients" \
			"$XDG_RUNTIME_DIR/weston.log" 2>/dev/null; then
		ready=1
		break
	fi
	if ! kill -0 "$weston_pid" 2>/dev/null; then
		break
	fi
	sleep 1
	i=$((i + 1))
done
if [ -n "$ready" ]; then
	echo "weston is ready after ${i}s"
else
	echo "WARN: weston readiness not confirmed after ${i}s (pid alive: $(kill -0 "$weston_pid" 2>/dev/null && echo yes || echo no))"
fi

# --- the visible client: weston-simple-egl -----------------------------------
# A rotating EGL cube in a Wayland window — the full stack under test:
# client → Wayland → weston → gl-renderer → GBM/EGL → freedreno → panel.
if [ -x "$BASE/usr/bin/weston-simple-egl" ]; then
	echo "=== starting weston-simple-egl for ${CLIENT_SECONDS}s ==="
	run "$BASE/usr/bin/weston-simple-egl" &
	client_pid=$!
	sleep "$CLIENT_SECONDS"
	kill -TERM "$client_pid" 2>/dev/null
	wait "$client_pid" 2>/dev/null
	echo "client exit=$?"
else
	echo "FATAL: weston-simple-egl missing — the GL renderer was likely not built"
fi

echo "=== stopping weston ==="
kill -TERM "$weston_pid" 2>/dev/null
wait "$weston_pid" 2>/dev/null
echo "weston exit=$?"

echo "=== weston log (filtered) ==="
grep -E "backend|renderer|output|error|Error|fail" \
	"$XDG_RUNTIME_DIR/weston.log" 2>/dev/null | head -n 25

echo "=== weston gate window finished ==="
exit 0
