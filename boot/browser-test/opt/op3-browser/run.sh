#!/bin/sh
# ============================================================
# OnePlus 3 browser gate runner (lives on sda15).
#
# Layer 07: cog (WPE WebKit browser) renders a local HTML page on the panel
# through WPEBackend-fdo → Wayland → weston → freedreno. JavaScript execution
# is verified by a page-side counter.
#
# Same deployment model as the EGL/Weston bundles: the initramfs launcher only
# waits for this file and executes it, so the test can be changed on sda15
# without repacking or re-flashing the boot image.
#
# Layout next to this file (produced by scripts/stage-browser-rootfs.sh):
#   the WHOLE Buildroot target tree plus test-page.html and run.sh.
#
# All device lessons from the Weston gate apply here (see
# docs/handoff/pmos612-wayland-weston.md): LD_LIBRARY_PATH must never be
# exported; every bundle binary goes through the bundled glibc loader;
# compile-time-absolute paths are bridged with symlinks; stale processes are
# swept by full cmdline; input_id rules must exist before udevd starts.
#
# Additionally, WebKit itself forks helper processes by exec'ing absolute
# paths (/usr/libexec/WebKitWebProcess, WebKitNetworkProcess); those are
# wrapped through the loader the same way as the weston helpers.
# ============================================================

BASE=$(dirname "$0")

export GBM_BACKENDS_PATH="$BASE/usr/lib/gbm"
unset DISPLAY WAYLAND_DISPLAY XDG_RUNTIME_DIR LD_LIBRARY_PATH

BROWSER_SECONDS=${BROWSER_SECONDS:-60}

# Pure-shell glob so that no external command runs before the loader is known.
set -- "$BASE"/lib/ld-linux-*.so*
LOADER=$1
[ -f "$LOADER" ] || LOADER=""

LIBPATH="$BASE/usr/lib:$BASE/usr/lib/libweston-14:$BASE/lib"

run() { # run <bundle-binary> [args...] — through the bundled loader
	echo "+ $*"
	if [ -n "$LOADER" ]; then
		"$LOADER" --library-path "$LIBPATH" "$@"
	else
		"$@"
	fi
}

echo "browser bundle:     $BASE"
echo "browser:            cog (WPE WebKit)"
echo "hold seconds:       $BROWSER_SECONDS"
echo "loader:             ${LOADER:-MISSING}"

if [ -z "$LOADER" ]; then
	echo "FATAL: no bundled dynamic loader in $BASE/lib"
	exit 1
fi

# --- kill leftovers from previous runs ---------------------------------------
# A weston forked twin survives a TERM aimed at the tracked pid; the survivor
# keeps DRM master and the old socket, so the next run renders nothing.
kill_stale() {
	leftovers=""
	for p in /proc/[0-9]*; do
		[ "$p" = "/proc/$$" ] && continue
		c=$(tr '\0' ' ' < "$p/cmdline" 2>/dev/null)
		case "$c" in
			*"/usr/bin/weston "*|*"/sbin/udevd "*|*"/usr/libexec/weston-"*|*"/usr/bin/cog "*|*"/usr/libexec/WebKit"*)
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

# --- Bridge compile-time-absolute paths into the bundle ----------------------
mkdir -p /usr/lib /usr/share /usr/libexec /run
for m in weston libweston-14 udev; do
	ln -sfn "$BASE/usr/lib/$m" "/usr/lib/$m"
done
for d in libinput X11 weston fontconfig; do
	[ -e "$BASE/usr/share/$d" ] && ln -sfn "$BASE/usr/share/$d" "/usr/share/$d"
done
[ -e "$BASE/usr/libexec" ] && ln -sfn "$BASE/usr/libexec" "/usr/libexec"

# --- Wrap helper binaries that weston/WebKit exec by absolute path -----------
# The initramfs has no dynamic loader, so raw execs die with status 127/1.
# Always rewrite the wrapper: a bundle redeploy (tar -x) restores the original
# binaries (observed 2026-08-30).
for helper in "$BASE"/usr/libexec/weston-desktop-shell \
              "$BASE"/usr/libexec/weston-keyboard \
              "$BASE"/usr/libexec/WebKit*; do
	[ -x "$helper" ] || continue
	case "$helper" in *.real|*\.sh) continue;; esac
	if [ ! -e "$helper.real" ]; then
		mv "$helper" "$helper.real"
	fi
	{
		echo "#!/bin/sh"
		echo "exec \"$LOADER\" --library-path \"$LIBPATH\" \"$helper.real\" \"\$@\""
	} > "$helper"
	chmod +x "$helper"
done

# --- udev: libinput and Mesa's device discovery need the udev database -------
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
	echo "WARN: no bundled udevd"
fi

# --- weston ------------------------------------------------------------------
export XDG_RUNTIME_DIR=/run/op3-weston
rm -rf "$XDG_RUNTIME_DIR"
mkdir -p "$XDG_RUNTIME_DIR" && chmod 0700 "$XDG_RUNTIME_DIR"

echo "=== starting weston (DRM backend, GL renderer) ==="
run "$BASE/usr/bin/weston" \
	--backend=drm-backend.so \
	--idle-time=0 \
	--log="$XDG_RUNTIME_DIR/weston.log" &
weston_pid=$!

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
	echo "=== weston log (tail) ==="
	tail -n 10 "$XDG_RUNTIME_DIR/weston.log" 2>/dev/null
	kill_stale
	exit 1
fi

# --- the browser: cog on WPEBackend-fdo --------------------------------------
# Local page: deterministic, no network needed. The page itself proves the
# engine: a JS seconds counter, CSS animation and a canvas.
PAGE_URI="file://$BASE/test-page.html"
echo "=== starting cog for ${BROWSER_SECONDS}s: $PAGE_URI ==="
run "$BASE/usr/bin/cog" --platform=fdo --fullscreen "$PAGE_URI" \
	> "$XDG_RUNTIME_DIR/cog.log" 2>&1 &
cog_pid=$!
sleep "$BROWSER_SECONDS"

echo "=== stopping browser ==="
kill -TERM "$cog_pid" 2>/dev/null
sleep 2
kill_stale
wait "$cog_pid" 2>/dev/null
echo "cog exit=$?"

echo "=== stopping weston ==="
kill -TERM "$weston_pid" 2>/dev/null
sleep 2
kill_stale
wait "$weston_pid" 2>/dev/null
echo "weston exit=$?"

echo "=== cog output (filtered) ==="
grep -iE "error|fail|warning|backend|display" "$XDG_RUNTIME_DIR/cog.log" 2>/dev/null | head -n 15

echo "=== browser gate window finished ==="
exit 0
