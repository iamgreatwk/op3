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
		"$LOADER" --library-path "$BASE/usr/lib:$BASE/usr/lib/libweston-14:$BASE/lib" "$@"
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
# libweston-14 holds the backends/renderers, weston/ holds the shells, and
# eudev's rules live in usr/lib/udev — all addressed by absolute paths.
mkdir -p /usr/lib /usr/share /run
# GOTCHA: ln -sfn does not replace an existing real directory; rm the bridge
# path first (observed in the browser gate, 2026-08-30).
for m in weston libweston-14 udev; do
	rm -rf "/usr/lib/$m"
	ln -sfn "$BASE/usr/lib/$m" "/usr/lib/$m"
done
for d in libinput X11 weston; do
	[ -e "$BASE/usr/share/$d" ] || continue
	rm -rf "/usr/share/$d"
	ln -sfn "$BASE/usr/share/$d" "/usr/share/$d"
done
rm -rf /usr/libexec
ln -sfn "$BASE/usr/libexec" "/usr/libexec"

# weston launches its helpers (weston-desktop-shell, weston-keyboard) by
# exec'ing /usr/libexec/<name> directly. The initramfs has no dynamic loader,
# so those helpers died with status 1 and weston quit ("apparently cannot run
# at all"). Replace each helper with a wrapper script that execs the real
# binary through the bundled loader. fd 3 (the WAYLAND_SOCKET) and the
# environment survive the exec chain (observed 2026-08-30).
LIBPATH="$BASE/usr/lib:$BASE/usr/lib/libweston-14:$BASE/lib"
for helper in weston-desktop-shell weston-keyboard; do
	real="$BASE/usr/libexec/$helper"
	[ -x "$real" ] || continue
	# A bundle redeploy (tar -x over this tree) restores the original binary
	# and leaves the old .real behind, so this must be unconditional.
	if [ ! -e "$real.real" ]; then
		mv "$real" "$real.real"
	fi
	{
		echo "#!/bin/sh"
		echo "exec \"$LOADER\" --library-path \"$LIBPATH\" \"$real.real\" \"\$@\""
	} > "$real"
	chmod +x "$real"
done

# --- kill leftovers from previous runs ---------------------------------------
# A weston forked twin survives a plain TERM of the tracked pid: the survivor
# keeps DRM master ("Could not make device fd drm master: Device or resource
# busy") and the old socket, so the next weston renders nothing and the client
# cannot connect (observed 2026-08-30). Sweep /proc by full cmdline — busybox
# ps truncates, and our own command lines contain neither usr/bin/weston nor
# sbin/udevd, so this cannot kill us.
kill_stale() {
	leftovers=""
	for p in /proc/[0-9]*; do
		[ "$p" = "/proc/$$" ] && continue
		c=$(tr '\0' ' ' < "$p/cmdline" 2>/dev/null)
		case "$c" in
			*"/usr/bin/weston "*|*"/sbin/udevd "*|*"/usr/libexec/weston-"*)
				leftovers="$leftovers ${p#/proc/}"
				;;
		esac
	done
	if [ -n "$leftovers" ]; then
		echo "killing stale processes:$leftovers"
		kill -9 $leftovers 2>/dev/null
		sleep 1
	fi
}
kill_stale

# --- udev: libinput needs the udev database to enumerate devices -------------
# Buildroot's eudev does not ship the input_id rules file, so devices like the
# Synaptics touchscreen get no ID_INPUT tags and libinput rejects every device
# ("not tagged as supported input device") — weston then aborts because it has
# zero input devices. udevd has the input_id builtin compiled in; this rule
# invokes it (observed 2026-08-30).
RULES="$BASE/usr/lib/udev/rules.d"
mkdir -p "$RULES"
cat > "$RULES/60-op3-input-id.rules" <<'EOF'
ACTION=="remove", GOTO="op3_input_id_end"
SUBSYSTEM=="input", ENV{ID_INPUT}=="", IMPORT{builtin}="input_id"
LABEL="op3_input_id_end"
EOF

echo "=== starting udevd ==="
if [ -x "$BASE/sbin/udevd" ]; then
	run "$BASE/sbin/udevd" --daemon 2>&1
	run "$BASE/usr/bin/udevadm" trigger 2>&1 | tail -n 1
	run "$BASE/usr/bin/udevadm" settle --timeout=10 2>&1
	echo "udev: $(ls /run/udev/data 2>/dev/null | wc -l) device records"
else
	echo "WARN: no bundled udevd; libinput will find no devices (OK for smoke test)"
fi

# --- weston ------------------------------------------------------------------
# libseat: running as root, the builtin backend needs no seatd daemon.
export XDG_RUNTIME_DIR=/run/op3-weston
rm -rf "$XDG_RUNTIME_DIR"
mkdir -p "$XDG_RUNTIME_DIR" && chmod 0700 "$XDG_RUNTIME_DIR"

echo "=== starting weston (DRM backend, GL renderer) ==="
run "$BASE/usr/bin/weston" \
	--backend=drm-backend.so \
	--idle-time=0 \
	--log="$XDG_RUNTIME_DIR/weston.log" &
weston_pid=$!

# Wait for the Wayland socket, then export the actual socket name: stale lock
# files from crashed runs can push weston to wayland-1/2, and the client must
# follow (observed 2026-08-30: simple-egl asserted on wayland-0 vs wayland-1).
i=0
sock=""
while [ "$i" -lt 20 ]; do
	sock=$(ls "$XDG_RUNTIME_DIR"/wayland-* 2>/dev/null | head -n 1)
	[ -n "$sock" ] && break
	if ! kill -0 "$weston_pid" 2>/dev/null; then
		break
	fi
	sleep 1
	i=$((i + 1))
done
if [ -n "$sock" ]; then
	sock=${sock##*/}
	export WAYLAND_DISPLAY=$sock
	echo "weston is ready after ${i}s; Wayland socket: $sock"
else
	echo "FATAL: no Wayland socket after ${i}s (weston pid alive: $(kill -0 "$weston_pid" 2>/dev/null && echo yes || echo no))"
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
sleep 2
# The forked twin (if any) must go too, or it keeps DRM master and the socket.
kill_stale
wait "$weston_pid" 2>/dev/null
echo "weston exit=$?"

echo "=== weston log (filtered) ==="
grep -E "backend|renderer|output|error|Error|fail" \
	"$XDG_RUNTIME_DIR/weston.log" 2>/dev/null | head -n 25

echo "=== weston gate window finished ==="
exit 0
