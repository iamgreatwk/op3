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

## Debug log (in progress, 2026-08-30)

The stack was brought up over several device iterations. Fixes so far, all in
`boot/browser-test/opt/op3-browser/run.sh` unless noted:

1. **Build**: `op3_browser_defconfig` verified (wpewebkit 2.50.5 + cog 0.18.5 +
   wpebackend-fdo 1.16.1 + icu + harfbuzz + webp; gstreamer multimedia left
   OFF). Owner ran the 3–5 h build (OOM on `-j13`: buildroot auto parallelism
   is nproc+1 and overrides a command-line `-j`; fixed with
   `BR2_JLEVEL=3` + 19 GB swap + `BR2_CCACHE=y`).
2. **HarfBuzz missing ICU**: first wpewebkit CMake configure failed —
   staged harfbuzz predated `BR2_PACKAGE_ICU` and lacked `libharfbuzz-icu`.
   `make harfbuzz-dirclean` + resume fixed it.
3. **Mesa wayland platform**: client-side EGL had no wayland platform
   ("surfaceless") — same incremental trap as the weston gate;
   `mesa3d-dirclean` + rebuild with `-Dplatforms=wayland`.
4. **Mesa bind-wayland-display**: cog's wl platform asserts on
   `eglCreateWaylandBufferFromImageWL` — the entry point is gated by
   `#ifdef HAVE_BIND_WL_DISPLAY` (mesa `-Dlegacy-wayland=bind-wayland-display`,
   `BR2_PACKAGE_MESA3D_LEGACY_BIND_WAYLAND_DISPLAY=y`, auto-selected by
   wpewebkit but Mesa had been built before it was selected). Second
   `mesa3d-dirclean`. Diagnosis made with a cross-compiled
   `eglGetProcAddress` probe (`/tmp/egltest.c`) run on the device.
5. **cog platform name**: it is `wl` in cog 0.18 (module
   `libcogplatform-wl.so`); run.sh tries wl → fdo → wayland and keeps the
   first that stays alive. `--fullscreen` does not exist in cog 2.x and
   aborts argument parsing — removed (cog is fullscreen-ish by default; it
   maps to a window on the desktop, not truly fullscreen).
6. **ln -sfn gotcha**: `mkdir -p /usr/libexec` created a real directory which
   `ln -sfn` refused to replace (it nested the link inside) → weston helper
   execs failed with ENOENT → weston quit. All bridge paths now `rm -rf`
   before linking. Same fix applied to the weston gate's run.sh.
7. **Fonts**: no fonts in the target → added `BR2_PACKAGE_DEJAVU=y` (text
   would render blank otherwise).
8. **WebKit helpers**: they live in `/usr/libexec/wpe-webkit-2.0/`
   (`WPEWebProcess`, `WPENetworkProcess`, `WPEGPUProcess`) and are wrapped
   through the bundled loader like the weston helpers.

Current state: cog runs on the `wl` platform for the full window and loads
the page ("Loaded successfully" in cog.log). Owner observation: the page
appears as a window on the weston desktop, but the HTML source is shown as
plain text instead of being rendered.

NEXT ISSUE (diagnosed, fix pending): file:// HTML displayed as text — the
target has no MIME database (`shared-mime-info` not installed), so WebKit's
GLib MIME sniffing cannot classify `.html` as `text/html`. Fix: add
`BR2_PACKAGE_SHARED_MIME_INFO=y`, rebuild, restage.

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
the long pole (3–5 hours observed on this host; icu/harfbuzz/webkit dominate).
Always tee the output to a log — with a multi-hour build, a silent failure is
undebuggable after the fact:

```bash
cd /home/kai/src/oneplus3-mainline
PATH="$HOME/gnubin:$PATH" make -C source/buildroot O=$PWD/out/buildroot-op3-egl \
  -j"$(nproc)" 2>&1 | tee /tmp/buildroot-browser.log
```

Progress from another terminal:

```bash
grep -E ">>> (wpewebkit|cog|icu|harfbuzz|glib|libpsl|libsoup3)" \
  /tmp/buildroot-browser.log | tail -10
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
