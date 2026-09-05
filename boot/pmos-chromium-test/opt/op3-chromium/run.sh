#!/bin/sh
# ============================================================
# OP3 chromium boot launcher (OP3-PMOS-CHROMIUM-002, initramfs side).
#
# Replaces cog/WPE as the boot-default browser: the boot image's
# sbin/run_recovery.sh executes /newroot/opt/op3-browser/run.sh, which is
# a shim exec-ing this script (original cog launcher kept as run-cog.sh).
#
# Sequence (all steps device-validated 2026-09-03/04):
#   udevd (op3-browser bundle, libinput needs the udev database)
#   -> Wi-Fi association + clock step (op3-wifi CLI, retries)
#   -> GPU runtime PM forced on AFTER wifi (ordering is a hard-reset guard)
#   -> weston DRM backend kiosk-shell rotate-270 (op3-browser bundle)
#   -> chromium (pmOS/Alpine chroot at /newroot/pmos) on the panel,
#      CDP 127.0.0.1:9222 for the PC driver; homepage from
#      /newroot/opt/op3-chromium/homepage (editable on sda15)
#
# Chromium flags rationale:
#   --no-sandbox            root chroot
#   --disable-gpu           SwANGLE/Vulkan missing -> GPU init failure killed
#                           the browser; CPU raster is stable (GPU attempt
#                           hard-reset the SoC 2026-09-04, dummy regulators)
#   --disable-dev-shm-usage no tmpfs at chroot /dev/shm
#   NO --headless           headless forces the headless ozone platform and
#                           overrides --ozone-platform=wayland (no panel)
# ============================================================

BASE=/newroot/opt/op3-browser
CH=/newroot/pmos
CHR=/newroot/opt/op3-chromium
LOG=/var/log/op3-chromium-boot.log
PERSIST_LOG=/newroot/var/log/op3-chromium-boot.log

mkdir -p /var/log
: > "$LOG"

log() {
	line="$(date '+%H:%M:%S') $*"
	printf '%s\n' "$line" >> "$LOG"
	printf '%s\n' "op3-chromium: $line" > /dev/kmsg 2>/dev/null
}
sync_log() {
	mkdir -p /newroot/var/log 2>/dev/null
	cat "$LOG" > "$PERSIST_LOG" 2>/dev/null
	sync
}

log "chromium boot launcher start pid=$$"

# ---- op3-browser bundle dynamic loader --------------------------------------
set -- "$BASE"/lib/ld-linux-*.so*
LOADER=$1
[ -f "$LOADER" ] || LOADER=""
LIBPATH="$BASE/usr/lib:$BASE/usr/lib/libweston-14:$BASE/lib"
if [ -z "$LOADER" ]; then
	log "FATAL: no bundle loader at $BASE/lib"
	sync_log
	exit 1
fi

# ---- bundle bridges (verbatim requirements from the cog launcher) -----------
# weston/WebKit locate helpers, modules and data files through
# compile-time-absolute paths; bridge them into the initramfs /usr.
export GBM_BACKENDS_PATH="$BASE/usr/lib/gbm"
for m in weston libweston-14 udev cog gio; do
	rm -rf "/usr/lib/$m"
	ln -sfn "$BASE/usr/lib/$m" "/usr/lib/$m"
done
for d in libinput X11 weston fontconfig wpe-webkit-2.0 fonts mime p11-kit; do
	[ -e "$BASE/usr/share/$d" ] || continue
	rm -rf "/usr/share/$d"
	ln -sfn "$BASE/usr/share/$d" "/usr/share/$d"
done
export XDG_DATA_DIRS="$BASE/usr/share:/usr/share"
export TZ=CST-8
rm -rf /usr/libexec
ln -sfn "$BASE/usr/libexec" "/usr/libexec"

# ---- udev: libinput needs the udev database AND input classification --------
RULES="$BASE/usr/lib/udev/rules.d"
mkdir -p "$RULES"
cat > "$RULES/60-op3-input-id.rules" <<'EOF'
ACTION=="remove", GOTO="op3_input_id_end"
SUBSYSTEM=="input", ENV{ID_INPUT}=="", IMPORT{builtin}="input_id"
LABEL="op3_input_id_end"
EOF
if [ -x "$BASE/sbin/udevd" ]; then
	"$LOADER" --library-path "$LIBPATH" "$BASE/sbin/udevd" --daemon 2>&1
	"$LOADER" --library-path "$LIBPATH" "$BASE/usr/bin/udevadm" trigger >/dev/null 2>&1
	"$LOADER" --library-path "$LIBPATH" "$BASE/usr/bin/udevadm" settle --timeout=10 >/dev/null 2>&1
	log "udev: $(ls /run/udev/data 2>/dev/null | wc -l) device records"
else
	log "WARN: no bundled udevd"
fi

# ---- Wi-Fi association (retry; clock step from HTTP Date) -------------------
NET_TRIES=${NET_TRIES:-3}
n=0
while [ "$n" -lt "$NET_TRIES" ]; do
	if ip -4 addr show dev wlan0 2>/dev/null | grep -q 'inet ' && \
	   ping -c 1 -W 3 192.168.1.1 >/dev/null 2>&1; then
		log "network: wlan0 already up ($(ip -4 addr show dev wlan0 | grep 'inet '))"
		break
	fi
	n=$((n + 1))
	log "network: wifi auto (try $n/$NET_TRIES)"
	/newroot/opt/op3-wifi/wifi auto >> "$LOG" 2>&1
	sleep 2
done
if ! ping -c 1 -W 3 192.168.1.1 >/dev/null 2>&1; then
	log "WARN: no gateway reachability after $NET_TRIES tries (continuing; page may fail to load)"
fi

if [ "$(date +%s)" -lt 1000000000 ]; then
	hdr=$(wget -S -T 8 -O /dev/null http://www.baidu.com 2>&1 |
		sed -n 's/^[[:space:]]*[Dd]ate:[[:space:]]*//p' | head -n 1)
	if [ -n "$hdr" ]; then
		set -- $hdr
		case "$3" in
			Jan) mm=01;; Feb) mm=02;; Mar) mm=03;; Apr) mm=04;;
			May) mm=05;; Jun) mm=06;; Jul) mm=07;; Aug) mm=08;;
			Sep) mm=09;; Oct) mm=10;; Nov) mm=11;; Dec) mm=12;;
			*) mm="";;
		esac
		if [ -n "$mm" ] && date -u -s "$4-$mm-$2 $5" >/dev/null 2>&1; then
			log "network: clock set from '$hdr'"
		fi
	fi
fi

# ---- GPU runtime PM forced on (AFTER wifi — hard-reset guard) ---------------
GPU_POWER=/sys/bus/platform/devices/b00000.gpu/power/control
if [ -f "$GPU_POWER" ]; then
	echo on > "$GPU_POWER" 2>/dev/null
	log "GPU runtime PM disabled: control=$(cat "$GPU_POWER" 2>/dev/null)"
fi

# ---- weston (bundle, DRM backend, kiosk-shell, rotate-270) ------------------
unset DISPLAY WAYLAND_DISPLAY LD_LIBRARY_PATH
XDG_RUNTIME_DIR=/run/op3-weston
export XDG_RUNTIME_DIR
rm -rf "$XDG_RUNTIME_DIR"
mkdir -p "$XDG_RUNTIME_DIR" && chmod 0700 "$XDG_RUNTIME_DIR"

# weston execs its helpers by absolute path — wrap them through the loader
for helper in "$BASE"/usr/libexec/weston-desktop-shell \
              "$BASE"/usr/libexec/weston-keyboard; do
	[ -x "$helper" ] || continue
	case "$helper" in *.real|*.sh) continue;; esac
	if [ ! -e "$helper.real" ]; then
		mv "$helper" "$helper.real"
	fi
	{
		echo "#!/bin/sh"
		echo "exec \"$LOADER\" --library-path \"$LIBPATH\" \"$helper.real\" \"\$@\""
	} > "$helper"
	chmod +x "$helper"
done

mkdir -p "$XDG_RUNTIME_DIR/weston-conf"
cat > "$XDG_RUNTIME_DIR/weston-conf/weston.ini" <<EOF
[output]
name=DSI-1
transform=${WESTON_TRANSFORM:-rotate-270}
EOF

log "starting weston (DRM backend, kiosk-shell, rotate-270)"
"$LOADER" --library-path "$LIBPATH" "$BASE/usr/bin/weston" \
	--backend=drm-backend.so \
	--idle-time=0 \
	--shell=${WESTON_SHELL:-kiosk-shell.so} \
	--config="$XDG_RUNTIME_DIR/weston-conf/weston.ini" \
	--log="$XDG_RUNTIME_DIR/weston.log" &
weston_pid=$!

i=0
sock=""
while [ "$i" -lt 30 ]; do
	sock=$(ls "$XDG_RUNTIME_DIR"/wayland-* 2>/dev/null | grep -v lock | head -n 1)
	[ -n "$sock" ] && break
	kill -0 "$weston_pid" 2>/dev/null || break
	sleep 1
	i=$((i + 1))
done
if [ -z "$sock" ]; then
	log "FATAL: no wayland socket after ${i}s"
	tail -n 10 "$XDG_RUNTIME_DIR/weston.log" >> "$LOG" 2>/dev/null
	sync_log
	exit 1
fi
sock=${sock##*/}
log "weston ready after ${i}s; socket: $sock"

# ---- chromium (pmOS/Alpine chroot) ------------------------------------------
for d in proc sys dev run; do
	mkdir -p "$CH/$d"
	mountpoint -q "$CH/$d" 2>/dev/null || mount --bind "/$d" "$CH/$d"
done
ip link set lo up
sysctl -w net.ipv6.conf.all.disable_ipv6=1 >/dev/null 2>&1
sysctl -w net.ipv6.conf.default.disable_ipv6=1 >/dev/null 2>&1

URL=$(head -n 1 "$CHR/homepage" 2>/dev/null)
[ -n "$URL" ] || URL="https://jw.jnu.edu.cn/jwapp/sys/pkgl/*default/index.do"
log "homepage: $URL"

export XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR"
export WAYLAND_DISPLAY="$sock"
export HOME=/tmp

while :; do
	rm -f "$CH/tmp/cp-gui/Singleton"*
	log "starting chromium (GUI, CDP 9222): $URL"
	chroot "$CH" /usr/lib/chromium/chrome \
		--ozone-platform=wayland \
		--no-sandbox \
		--disable-gpu \
		--disable-dev-shm-usage \
		--user-data-dir=/tmp/cp-gui \
		--remote-debugging-port=9222 \
		--lang=zh-CN \
		--start-fullscreen \
		"$URL" >> "$LOG" 2>&1
	log "chromium exited ($?); respawn in 5s"
	sleep 5
done
