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

# --- target URL --------------------------------------------------------------
# Selection order: environment (set by the boot/browser-net-test launcher) >
# sda15 file $BASE/op3-url (editable over SSH without repacking) > the built-in
# local test page. The default keeps the layer-07 local-page gate unchanged.
PAGE_URI=${OP3_BROWSER_URL:-}
if [ -z "$PAGE_URI" ] && [ -f "$BASE/op3-url" ]; then
	PAGE_URI=$(head -n 1 "$BASE/op3-url")
fi
[ -n "$PAGE_URI" ] || PAGE_URI="file://$BASE/test-page.html"

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
echo "target URL:         $PAGE_URI"
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
			*"/usr/bin/weston "*|*"/sbin/udevd "*|*"/usr/libexec/weston-"*|*"/usr/bin/cog "*|*"/usr/libexec/wpe-webkit-2.0/"*|*"/usr/libexec/WebKit"*)
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
mkdir -p /usr/lib /usr/share /run /etc
# GOTCHA (observed 2026-08-30): ln -sfn does NOT replace an existing real
# directory — a mkdir of the bridge path earlier made /usr/libexec a real
# empty dir and weston's helper execs then failed with ENOENT. rm -rf the
# bridge path first; if it is a symlink, rm removes only the link.
for m in weston libweston-14 udev cog; do
	rm -rf "/usr/lib/$m"
	ln -sfn "$BASE/usr/lib/$m" "/usr/lib/$m"
done
for d in libinput X11 weston fontconfig wpe-webkit-2.0 fonts mime; do
	[ -e "$BASE/usr/share/$d" ] || continue
	rm -rf "/usr/share/$d"
	ln -sfn "$BASE/usr/share/$d" "/usr/share/$d"
done
# GLib MIME sniffing (file:// → text/html) needs the mime db in XDG_DATA_DIRS.
export XDG_DATA_DIRS="$BASE/usr/share:/usr/share"
rm -rf /usr/libexec
ln -sfn "$BASE/usr/libexec" "/usr/libexec"
# fontconfig reads /etc/fonts/fonts.conf; the initramfs has no /etc/fonts.
if [ -e "$BASE/etc/fonts" ]; then
	rm -rf /etc/fonts
	ln -sfn "$BASE/etc/fonts" "/etc/fonts"
fi

# --- Wrap helper binaries that weston/WebKit exec by absolute path -----------
# The initramfs has no dynamic loader, so raw execs die with status 127/1.
# Always rewrite the wrapper: a bundle redeploy (tar -x) restores the original
# binaries (observed 2026-08-30).
for helper in "$BASE"/usr/libexec/weston-desktop-shell \
              "$BASE"/usr/libexec/weston-keyboard \
              "$BASE"/usr/libexec/wpe-webkit-2.0/WPE*; do
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

# --- network bring-up (only when the target is remote) -----------------------
# The OP3-WIFI-001 CLI lives on sda15 (/newroot/opt/op3-wifi) and drives the
# static initramfs wpa_supplicant/udhcpc, so calling `wifi auto` from here
# needs no initramfs wifi overlay and no repack — see docs/handoff/op3-wifi-001.md.
# `wifi auto` uses the saved default profile; association, DHCP and
# /etc/resolv.conf are all handled by the CLI.
NET_WAIT_SECONDS=${NET_WAIT_SECONDS:-45}

need_network() {
	case "$PAGE_URI" in
		http://*|https://*) return 0 ;;
	esac
	[ "$OP3_BROWSER_NET" = "1" ]
}

have_wlan_ip() {
	ip -4 addr show dev wlan0 2>/dev/null | grep -q "inet "
}

bring_up_network() {
	echo "=== network: remote target $PAGE_URI ==="
	if have_wlan_ip; then
		echo "network: wlan0 already has an IPv4 address (wifi_auto already ran)"
	else
		if [ ! -x /newroot/opt/op3-wifi/wifi ]; then
			echo "FATAL-NET: /newroot/opt/op3-wifi/wifi missing — deploy the OP3-WIFI-001 bundle to sda15 first (stage-op3-wifi-rootfs.sh)"
			return 1
		fi
		echo "network: running /newroot/opt/op3-wifi/wifi auto"
		/newroot/opt/op3-wifi/wifi auto 2>&1 || {
			echo "FATAL-NET: wifi auto failed"
			return 1
		}
	fi
	i=0
	while [ "$i" -lt "$NET_WAIT_SECONDS" ]; do
		have_wlan_ip && break
		sleep 1
		i=$((i + 1))
	done
	if ! have_wlan_ip; then
		echo "FATAL-NET: no IPv4 address on wlan0 after ${NET_WAIT_SECONDS}s"
		/newroot/opt/op3-wifi/wifi current 2>&1 || true
		return 1
	fi
	echo "network: $(ip -4 addr show dev wlan0 | grep 'inet ')"
	sed 's/^/network: resolv: /' /etc/resolv.conf 2>/dev/null
	echo "network: default route: $(ip -4 route show default 2>/dev/null)"
	if ping -c 1 -W 3 223.5.5.5 >/dev/null 2>&1; then
		echo "network: ping 223.5.5.5 OK"
	else
		echo "network: WARN ping 223.5.5.5 failed (continuing anyway)"
	fi
	# TLS certificate validation needs a sane clock; busybox ntpd steps it once.
	# Plain-HTTP targets skip this.
	case "$PAGE_URI" in
		https://*)
			if [ "$(date +%s)" -lt 1000000000 ]; then
				echo "network: clock is pre-2001; stepping with busybox ntpd"
				ntpd -n -q -p ntp.aliyun.com 2>&1 | head -n 3
				echo "network: clock now $(date)"
			fi
			;;
	esac
	return 0
}

if need_network; then
	if ! bring_up_network; then
		echo "FATAL: network bring-up failed; not starting the browser"
		kill_stale
		exit 1
	fi
fi

# --- GPU runtime PM disable (moved here from the launcher for browser-net) ---
# Known defect: the DTB GPU regulators are placeholders, so runtime PM cycles
# blank the panel or hard-reset the SoC (docs/known-issues.md). Force the GPU
# permanently on right before weston. Ordering matters: with the browser-net
# flow, powering the GPU at launcher time caused a reproducible hard reset the
# instant wlan0 finished associating (2026-08-31) — sequence GPU-on AFTER the
# Wi-Fi link is up. Idempotent for the browser-test launcher, which already
# did the same thing earlier.
GPU_POWER=/sys/bus/platform/devices/b00000.gpu/power/control
if [ -f "$GPU_POWER" ]; then
	echo on > "$GPU_POWER" 2>/dev/null
	echo "GPU runtime PM disabled: control=$(cat "$GPU_POWER" 2>/dev/null), cur_freq=$(cat /sys/class/devfreq/b00000.gpu/cur_freq 2>/dev/null)"
else
	echo "GPU runtime PM: $GPU_POWER not found"
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
#
# The cog platform name changed across versions (wl / fdo / wayland); try the
# candidates in order and keep the first that stays alive for 4 s.
cog_pid=""
for plat in wl fdo wayland; do
	echo "=== starting cog (platform $plat) for ${BROWSER_SECONDS}s: $PAGE_URI ==="
	run "$BASE/usr/bin/cog" --platform="$plat" "$PAGE_URI" \
		> "$XDG_RUNTIME_DIR/cog.log" 2>&1 &
	cog_pid=$!
	sleep 4
	if kill -0 "$cog_pid" 2>/dev/null; then
		break
	fi
	echo "cog with platform '$plat' failed:"
	tail -n 6 "$XDG_RUNTIME_DIR/cog.log" 2>/dev/null
	kill -9 "$cog_pid" 2>/dev/null
	cog_pid=""
done
if [ -z "$cog_pid" ]; then
	echo "FATAL: every cog platform attempt failed"
	tail -n 15 "$XDG_RUNTIME_DIR/cog.log" 2>/dev/null
	kill_stale
	exit 1
fi
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
