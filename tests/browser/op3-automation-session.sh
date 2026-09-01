#!/bin/sh
# OP3 automation session: weston kiosk + cog --automation + WPEWebDriver.
# File-based process sweep (cannot match its own cmdline).
LOG=/tmp/automation.log
: > "$LOG"

for p in /proc/[0-9]*; do
	[ "$p" = "/proc/$$" ] && continue
	c=$(tr '\0' ' ' < "$p/cmdline" 2>/dev/null)
	case "$c" in
		*"/usr/bin/weston "*|*"/usr/bin/cog "*|*"/usr/bin/WPEWebDriver"*|*"/usr/libexec/wpe-webkit-2.0/"*|*"/usr/libexec/weston-"*|*"/usr/bin/dbus-daemon "*|*"/sbin/udevd "*)
			kill -9 "${p#/proc/}" 2>/dev/null ;;
	esac
done
sleep 1

B=/newroot/opt/op3-browser
L=$B/lib/ld-linux-aarch64.so.1
P=$B/usr/lib:$B/usr/lib/libweston-14:$B/lib
export GBM_BACKENDS_PATH=$B/usr/lib/gbm
export XDG_RUNTIME_DIR=/run/op3-weston
mkdir -p "$XDG_RUNTIME_DIR"; chmod 0700 "$XDG_RUNTIME_DIR"
rm -f "$XDG_RUNTIME_DIR"/wayland-*

# GPU: force on while the display is in use (known resume hard-reset issue).
echo on > /sys/bus/platform/devices/b00000.gpu/power/control 2>/dev/null
echo "GPU control=$(cat /sys/bus/platform/devices/b00000.gpu/power/control 2>/dev/null)" >> "$LOG"

# bring up loopback (the inspector server binds 127.0.0.1)
ip link set lo up 2>/dev/null

RULES=$B/usr/lib/udev/rules.d
mkdir -p "$RULES"
cat > "$RULES/60-op3-input-id.rules" <<'EOF'
ACTION=="remove", GOTO="op3_input_id_end"
SUBSYSTEM=="input", ENV{ID_INPUT}=="", IMPORT{builtin}="input_id"
LABEL="op3_input_id_end"
EOF

# udev for libinput
$L --library-path $P $B/sbin/udevd --daemon >> "$LOG" 2>&1
$L --library-path $P $B/usr/bin/udevadm trigger >> "$LOG" 2>&1
$L --library-path $P $B/usr/bin/udevadm settle --timeout=10 >> "$LOG" 2>&1

# Wrap helpers exec'd by absolute path (same as run.sh): weston helpers and
# the WPE Web/Network/GPU processes need the bundled loader.
for helper in "$B"/usr/libexec/weston-desktop-shell \
              "$B"/usr/libexec/weston-keyboard \
              "$B"/usr/libexec/wpe-webkit-2.0/WPE*; do
	[ -x "$helper" ] || continue
	case "$helper" in *.real|*\.sh) continue;; esac
	[ -e "$helper.real" ] || mv "$helper" "$helper.real"
	{
		echo "#!/bin/sh"
		echo "exec \"$L\" --library-path \"$P\" \"$helper.real\" \"\$@\""
	} > "$helper"
	chmod +x "$helper"
done

# session bus (cogctl / WPEWebDriver)
rm -f /run/op3-dbus
export DBUS_SESSION_BUS_ADDRESS=unix:path=/run/op3-dbus
$L --library-path $P $B/usr/bin/dbus-daemon --fork \
	--config-file=$B/usr/share/dbus-1/session.conf \
	--address="$DBUS_SESSION_BUS_ADDRESS" >> "$LOG" 2>&1
echo "dbus: $?" >> "$LOG"

# compositor
$L --library-path $P $B/usr/bin/weston \
	--backend=drm-backend.so --idle-time=0 --shell=kiosk-shell.so \
	--log=$XDG_RUNTIME_DIR/weston.log >> "$LOG" 2>&1 &
i=0; sock=""
while [ $i -lt 15 ]; do
	sock=$(ls "$XDG_RUNTIME_DIR"/wayland-* 2>/dev/null | head -1)
	[ -n "$sock" ] && break
	sleep 1; i=$((i + 1))
done
[ -n "$sock" ] || { echo "FATAL: no wayland socket" >> "$LOG"; exit 1; }
export WAYLAND_DISPLAY=${sock##*/}
echo "weston ready: $WAYLAND_DISPLAY" >> "$LOG"

# browser in automation mode; the automation channel is the WebKit remote
# inspector server that WPEWebDriver connects to via --target.
export WEBKIT_INSPECTOR_SERVER=127.0.0.1:9222
$L --library-path $P $B/usr/bin/cog --platform=wl --automation \
	https://www.baidu.com > "$XDG_RUNTIME_DIR/cog.log" 2>&1 &
sleep 6
grep -E "automation|Automation|Loaded|error" "$XDG_RUNTIME_DIR/cog.log" | tail -4 >> "$LOG"

# WebDriver daemon, reachable from the PC (host=all)
$L --library-path $P $B/usr/bin/WPEWebDriver --port=7000 --host=all \
	-t 127.0.0.1:9222 > /tmp/webdriver.log 2>&1 &
sleep 2
echo "webdriver started" >> "$LOG"
echo "=== automation session up ===" >> "$LOG"
