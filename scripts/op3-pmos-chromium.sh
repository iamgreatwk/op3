#!/usr/bin/env bash
set -euo pipefail

# Deploy and run the pmOS/Alpine chromium bundle on the OP3.
#
#   scripts/op3-pmos-chromium.sh deploy   # push + extract to /newroot/pmos
#   scripts/op3-pmos-chromium.sh run      # bind-mounts + launch headless CDP
#   scripts/op3-pmos-chromium.sh tunnel   # PC-side ssh -L to device CDP
#   scripts/op3-pmos-chromium.sh run-gui  # render on the panel (weston needed)
#   scripts/op3-pmos-chromium.sh stop
#   scripts/op3-pmos-chromium.sh status
#
# Endpoint: RNDIS 172.16.42.1 (default) or wifi 192.168.1.6 — pass as $1
# in deploy (first arg), e.g.: scripts/op3-pmos-chromium.sh deploy 192.168.1.6

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
tarball="$project_root/artifacts/op3-pmos-chromium-rootfs.tar.gz"
host="${OP3_HOST:-172.16.42.1}"
ssh_opts=(-o StrictHostKeyChecking=no -o ConnectTimeout=8)

# password auth: reuse the one-shot askpass helper if present
# (printf '#!/bin/sh\necho <password>\n' > /tmp/op3-askpass.sh && chmod 700 it)
if [ -x /tmp/op3-askpass.sh ]; then
  export SSH_ASKPASS=/tmp/op3-askpass.sh SSH_ASKPASS_REQUIRE=force DISPLAY=:0
fi

[ "$#" -ge 1 ] && case "$1" in
  deploy|run|run-gui|stop|status|tunnel) ;;      # action stays as $1
  *) host="$1"; shift ;;
esac
action="${1:-}"

die() { printf 'FATAL: %s\n' "$*" >&2; exit 1; }

# device-side chroot wrapper: bind mounts, then exec chromium launcher
device_run() { # gui(0|1)
  R=/newroot/pmos
  ip link set lo up   # REQUIRED: DevTools http server cannot bind with lo down
  for d in proc sys dev; do
    mountpoint -q "$R/$d" || mount --bind "/$d" "$R/$d"
  done
  if [ "$1" = 0 ]; then
    OP3_CHROMIUM_HEADLESS=1 chroot "$R" /usr/local/bin/start-chromium.sh \
      >/newroot/var/log/op3-chromium.log 2>&1 &
  else
    OP3_CHROMIUM_HEADLESS=0 chroot "$R" /usr/local/bin/start-chromium.sh \
      >/newroot/var/log/op3-chromium.log 2>&1 &
  fi
  sleep 12
  grep -q "DevTools listening" /newroot/var/log/op3-chromium.log \
    && echo "PASS: CDP up on 127.0.0.1:9222 (PC: op3-pmos-chromium.sh tunnel)" \
    || { echo "FAIL: no DevTools listener"; tail -5 /newroot/var/log/op3-chromium.log; }
}

pc_tunnel() {
  SSH_ASKPASS_REQUIRE=force setsid ssh -N -o ExitOnForwardFailure=yes \
    -L 9222:127.0.0.1:9222 "root@$host" >/tmp/op3-cdp-tunnel.log 2>&1 &
  sleep 3
  curl -s --max-time 6 http://127.0.0.1:9222/json/version \
    && echo "TUNNEL-OK (PC -> http://localhost:9222)" \
    || { cat /tmp/op3-cdp-tunnel.log; echo "TUNNEL-FAILED"; }
}

# device-side GUI hybrid: run.sh brings up udevd+GPU+weston (touch input
# needs the udev database), we replace cog with chromium (panel output +
# CDP). Human drags the slider on the phone screen; the CDP driver fills
# credentials after the captcha passes.
device_run_gui() {
  R=/newroot/pmos
  ip link set lo up
  ps aux | grep -i 'chrom\|/usr/bin/cog' | grep -v grep | awk '{print $1}' | xargs -r kill -9
  sleep 2
  rm -f "$R/tmp/chromium-profile/Singleton"*
  OP3_BROWSER_URL="file:///newroot/opt/op3-browser/test-page.html" \
    BROWSER_SECONDS=99999 NET_WAIT_SECONDS=1 \
    setsid /newroot/opt/op3-browser/run.sh >/newroot/var/log/op3-weston.log 2>&1 &
  i=0
  while [ $i -lt 40 ]; do
    ls /run/op3-weston/wayland-* >/dev/null 2>&1 && break
    sleep 1; i=$((i+1))
  done
  ls /run/op3-weston/wayland-* >/dev/null 2>&1 \
    || { echo "FATAL-GUI: weston socket never appeared"; tail -8 /run/op3-weston/weston.log /newroot/var/log/op3-weston.log 2>/dev/null; return 1; }
  sock=$(ls /run/op3-weston/ | grep '^wayland-' | head -1)
  echo "weston ready: $sock"
  # replace cog with chromium (leave run.sh's weston running)
  ps aux | grep '/usr/bin/cog' | grep -v grep | awk '{print $1}' | xargs -r kill -9
  mountpoint -q "$R/run" || mount --bind /run "$R/run"
  XDG_RUNTIME_DIR=/run/op3-weston WAYLAND_DISPLAY=$sock \
    setsid env OP3_CHROMIUM_HEADLESS=0 chroot "$R" \
    /usr/local/bin/start-chromium.sh >/newroot/var/log/op3-chromium.log 2>&1 &
  sleep 12
  grep -q "DevTools listening" /newroot/var/log/op3-chromium.log \
    && echo "PASS-GUI: chromium on panel (touch enabled) + CDP 127.0.0.1:9222" \
    || { echo "FAIL-GUI:"; tail -6 /newroot/var/log/op3-chromium.log; }
}

case "$action" in
  deploy)
    [ -f "$tarball" ] || die "missing $tarball (run stage-pmos-chromium-rootfs.sh)"
    free_mb=$(ssh "${ssh_opts[@]}" "root@$host" \
      "df -m /newroot | tail -1 | awk '{print \$4}'") || die "device unreachable at $host"
    [ "$free_mb" -ge 900 ] || die "only ${free_mb}MB free on /newroot (need >=900MB)"
    printf 'free %sMB on /newroot — uploading (276MB, takes a while)\n' "$free_mb"
    cat "$tarball" | ssh "${ssh_opts[@]}" "root@$host" \
      'mkdir -p /newroot/pmos && gzip -dc | tar -x -C /newroot/pmos'
    ssh "${ssh_opts[@]}" "root@$host" \
      'test -x /newroot/pmos/usr/bin/chromium-browser && echo DEPLOY-OK'
    ;;
  run)
    ssh "${ssh_opts[@]}" "root@$host" "$(declare -f device_run); device_run 0"
    ;;
  tunnel)
    pc_tunnel
    ;;
  run-gui)
    ssh "${ssh_opts[@]}" "root@$host" "$(declare -f device_run_gui); device_run_gui"
    ;;
  stop)
    ssh "${ssh_opts[@]}" "root@$host" \
      'pkill -f chromium-launcher || true; pkill -f chromium/chromium || true; echo STOPPED'
    ;;
  status)
    ssh "${ssh_opts[@]}" "root@$host" \
      'pgrep -fa chromium | head -3; curl -s http://127.0.0.1:9222/json/version | head -3'
    ;;
  *) die "usage: $0 [host] deploy|run|tunnel|run-gui|stop|status" ;;
esac
