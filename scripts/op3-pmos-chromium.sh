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

[ "$#" -ge 1 ] && case "$1" in
  deploy|run|run-gui|stop|status|tunnel) shift ;; # action only
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
    ssh "${ssh_opts[@]}" "root@$host" "$(declare -f device_run); device_run 1"
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
