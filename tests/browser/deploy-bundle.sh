#!/usr/bin/env bash
set -euo pipefail
# Versioned bundle deploy: extract to op3-browser.<tag>, re-apply runtime
# assets, verify full manifest, switch symlink. See tests/browser/README.md.
project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
tarball="$1"
device="${2:-root@172.16.42.1}"
sha="$(sha256sum "$tarball" | cut -c1-8)"
tag="$3"
if [ -z "$tag" ]; then tag="$sha"; fi
remote_root="/newroot/opt/op3-browser.$tag"

echo "== 1. push tarball =="
scp -O "$tarball" "$device:/newroot/tmp/bundle-$tag.tar.gz"

echo "== 2. extract into $remote_root (fresh dir, never over the live one) =="
ssh "$device" "rm -rf '$remote_root' && mkdir -p '$remote_root' \
  && zcat /newroot/tmp/bundle-$tag.tar.gz | tar -xf - -C '$remote_root' \
  && test -x '$remote_root/opt/op3-browser/usr/bin/cog' \
  || { echo 'FATAL: extraction incomplete'; exit 1; }"

echo "== 3. re-apply runtime assets (extraction clobbers them) =="
scp -O "$project_root/boot/browser-test/opt/op3-browser/run.sh" \
    "$project_root/boot/browser-test/opt/op3-browser/test-page.html" \
    "$device:$remote_root/opt/op3-browser/"
ssh "$device" "mkdir -p '$remote_root/opt/op3-browser/usr/share/fonts/truetype/wqy-microhei'"
scp -O "$project_root/artifacts/fonts/wqy-microhei.ttc" \
    "$device:$remote_root/opt/op3-browser/usr/share/fonts/truetype/wqy-microhei/" \
    2>/dev/null || echo "WARN: no CJK font in artifacts/fonts, zh pages will render tofu"

echo "== 4. full manifest verification (exclude the three repo-added extras) =="
ssh "$device" "cd '$remote_root/opt/op3-browser' \
  && find . -type f | LC_ALL=C sort | xargs sha256sum \
  | grep -v -e './run.sh' -e './test-page.html' -e 'wqy-microhei.ttc'" > /tmp/op3-deploy-device.txt
( cd "$project_root/out/buildroot-op3-egl/target" \
  && find . -type f | LC_ALL=C sort | xargs sha256sum ) > /tmp/op3-deploy-host.txt
if diff -q /tmp/op3-deploy-host.txt /tmp/op3-deploy-device.txt >/dev/null; then
  echo "manifest OK ($(wc -l < /tmp/op3-deploy-host.txt) files)"
else
  echo "FATAL: manifest mismatch:" >&2
  diff /tmp/op3-deploy-host.txt /tmp/op3-deploy-device.txt | head -20 >&2
  exit 1
fi

echo "== 5. switch symlink =="
ssh "$device" "ln -sfn '$remote_root/opt/op3-browser' /newroot/opt/op3-browser \
  && ls -l /newroot/opt/op3-browser/run.sh"

echo "deployed: $remote_root (tag $tag)"
echo "rollback (on device): ln -sfn /newroot/opt/op3-browser.OLDTAG/opt/op3-browser /newroot/opt/op3-browser"
