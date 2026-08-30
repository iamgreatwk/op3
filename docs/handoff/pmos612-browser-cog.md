# OnePlus 3 pmOS 6.12 browser gate (Cog + WPE WebKit)

```text
Task / GitHub Issue: Owner-authorized no-Issue browser smoke test
Role: Implementation
Formal baseline: Linux v7.2 pristine upstream (unchanged)
Diagnostic kernel: pmOS 6.12 (v6.12-v74strict) full-initrd control
Baseline commit: 2073241 (repository HEAD when the EGL task started)
Working branch: agent/implementation/s6e3fa5-linux72-port
Changed files: buildroot/op3-browser.defconfig; scripts/stage-browser-rootfs.sh;
  boot/browser-test/sbin/run_recovery.sh; boot/browser-test/opt/op3-browser/run.sh;
  boot/browser-test/opt/op3-browser/test-page.html

Layer: 07 browser, one layer above 06 Wayland/Weston
Previous PASS milestone: OP3-WAYLAND-001, weston + simple-egl, pmOS 6.12 control
Sole hypothesis: cog (WPE WebKit) can render a local HTML page on the panel
  through WPEBackend-fdo → Wayland → weston → freedreno, with JavaScript
  executing.
Only variable changed per test:
  1. The browser bundle on sda15 (cog, WPE WebKit, weston, all libs + a local
     test page). The boot image carries only the launcher.

Build run by project owner: Buildroot 2026.02.x incremental on the existing
  out/buildroot-op3-egl output (toolchain, Mesa, weston reused). wpewebkit
  pulls icu, harfbuzz, webp, libpsl, libsoup3; multimedia (gstreamer) is left
  OFF to keep the build smaller — video playback is out of scope for this
  gate.
Build result: NOT_RUN
Artifacts and SHA256: filled in after the build

Device test run by project owner: pending
Device result: NOT_RUN
Evidence links / log paths: /newroot/var/log/op3-browser.log (persistent on
  sda15), /run/op3-weston/weston.log (session), ACM console relay

Conclusion: pending
Uncertainties: WebKit helper processes (WebKitWebProcess /
  WebKitNetworkProcess) are exec'd via absolute paths and are wrapped through
  the bundled loader by run.sh; dbus is installed (cog selects it) but no
  session bus is started — if WebKit requires it, add a dbus-daemon start to
  run.sh. Network is NOT needed: the gate loads a local file:// page.
Recommended next experiment: after the owner's build, stage + deploy + boot
  the browser image; the page's JS seconds counter must advance.
```

## PASS / FAIL record

PASS requires all of the following:

1. cog starts under weston (fdo platform) and renders the local test page on
   the panel: the "OP3 BROWSER" heading is visible with its CSS colour
   animation cycling.
2. JavaScript executes: the page's "JS seconds" counter advances.
3. The result is reproducible in a second run.

FAIL is a cog abort, a WebKit process crash, a black panel with the page never
appearing, or the JS counter stuck at 0. The CSS colour animation pausing is a
defect worth recording even if the counter works.

A PASS here does not establish network browsing (no network in this test), the
GPU-accelerated compositing path inside WebKit (vs software), or Linux 7.2
readiness.

## The test page

`boot/browser-test/opt/op3-browser/test-page.html` renders:

- a large "OP3 BROWSER" heading with a CSS colour animation (proves CSS +
  continuous repaint),
- a static sentence (proves text layout + fonts),
- a JavaScript seconds counter (proves JS execution + DOM updates).

Loaded as `file:///opt/op3-browser/test-page.html` — no network required.

## Owner step 1: build the browser stack

The defconfig is already applied to the existing output directory; the
toolchain, Mesa and weston are reused incrementally. The wpewebkit build is
the long pole (expect hours):

```bash
cd /home/kai/src/oneplus3-mainline
PATH="$HOME/gnubin:$PATH" make -C source/buildroot O=$PWD/out/buildroot-op3-egl -j"$(nproc)"
```

If a host tool error appears (e.g. cmake), do NOT switch branches; install the
host package or let Buildroot build the host variant.

## Agent steps after the build (rest of the flow)

```bash
scripts/stage-browser-rootfs.sh out/buildroot-op3-egl/target \
  artifacts/op3-browser-bundle.tar.gz
# deploy via ssh (tar), then pack the boot image:
OVERLAY_SOURCE="$PWD/boot/browser-test" scripts/make-drm-test-initrd.sh \
  artifacts/reference-initrd.img artifacts/op3-drm-dumb \
  artifacts/initrd-op3-browser.cpio.gz
scripts/pack-boot.sh \
  out/pmos-msm8996-v6.12-v74strict/arch/arm64/boot/Image.gz \
  out/pmos-msm8996-6.3.1-v74full/arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb \
  artifacts/initrd-op3-browser.cpio.gz \
  artifacts/boot-oneplus3-pmos612-v74dtb-browser.img
```

CRITICAL: `OVERLAY_SOURCE="$PWD/boot/browser-test"` is mandatory (see the
weston gate for what happens without it). Verify the built initramfs by
decompressing ALL gzip members (zcat only decompresses the first) and checking
that the overlay member contains the browser launcher.

## Known pitfalls carried over from the weston gate

All of the run.sh defence lines in `boot/browser-test/opt/op3-weston/run.sh`
are replicated here: never export LD_LIBRARY_PATH; bundle loader for every
binary; `/usr/lib/libweston-14` + `/usr/lib/weston` + `/usr/lib/udev` +
`/usr/share/*` symlinks; input_id udev rule before udevd; `/usr/libexec`
helper wrappers (now also for `WebKitWebProcess`/`WebKitNetworkProcess`);
stale-process sweep by full cmdline at start and stop; runtime-dir cleanup and
real-socket detection for the client.
