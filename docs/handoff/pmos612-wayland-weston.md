# OnePlus 3 pmOS 6.12 Wayland/Weston gate

```text
Task / GitHub Issue: Owner-authorized no-Issue Wayland/Weston smoke test
Role: Implementation
Formal baseline: Linux v7.2 pristine upstream (unchanged)
Diagnostic kernel: pmOS 6.12 (v6.12-v74strict) full-initrd control
Baseline commit: 2073241 (repository HEAD when the EGL task started)
Working branch: agent/implementation/s6e3fa5-linux72-port
Changed files: buildroot/op3-weston.defconfig; scripts/stage-weston-rootfs.sh;
  boot/weston-test/sbin/run_recovery.sh; boot/weston-test/opt/op3-weston/run.sh

Layer: 06 Wayland/compositor, one layer above 05 EGL
Previous PASS milestone: OP3-EGL-001, kmscube on freedreno, pmOS 6.12 control
Sole hypothesis: With the validated DRM+GPU+firmware stack below it, weston
  (DRM backend, GL renderer) can composite on the panel and weston-simple-egl
  can render a Wayland client through the compositor.
Only variable changed per test:
  1. The Wayland bundle on sda15 (weston, eudev, seatd, clients, all libs).
     The boot image carries only the launcher (same pattern as the EGL gate;
     editing run.sh on sda15 changes the test without a re-flash).

Build run by project owner: Buildroot 2026.02.x incremental on the EGL output
  (toolchain + Mesa reused); weston 14.0.2, eudev, seatd, demo/simple clients.
  Run by the agent with owner authorization, same as the Mesa build. One
  follow-up Mesa rebuild (mesa3d-dirclean) was required: the first Mesa build
  predated BR2_PACKAGE_WAYLAND, so libEGL shipped WITHOUT the wayland platform
  ("Native platform type: surfaceless") and clients could not get usable
  configs. With BR2_PACKAGE_WAYLAND=y mesa rebuilds with -Dplatforms=wayland.
Build result: PASS
Artifacts and SHA256:
  bundle  0a8163cb7e01e20deadab058bd710573425093abbe6109d8ace8fc06d6f867fd
          (op3-weston-bundle.tar.gz, whole Buildroot target, 22 MB)
  image   ea6043cc4c507783e7e96c6b4e1bdff3483f6c6da60dbbcbfe1c6673a1a27ed5
          (boot-oneplus3-pmos612-v74dtb-weston.img)
  initrd  402dd21da27c0faeb346b52edff0b39c63090cd333a0b8bdbafd0a522d72a6c9

Device test run by project owner: 2026-08-30, two full runs plus a live
  session, plus one cold-boot verification (fastboot boot of the weston image;
  the launcher ran the whole window autonomously); owner confirmed the weston
  desktop and the rotating weston-simple-egl triangle on the panel
Device result: PASS (all three criteria)
  - weston 14 DRM backend + GL renderer on /dev/dri/card0, DSI-1 enabled
    1080x1920@60, GL renderer FD530 (freedreno), no atomic-commit errors
  - panel shows the weston desktop (desktop-shell) and the rotating
    weston-simple-egl client for the whole window; client killed by the
    runner at window end (exit 143), zero assertions
  - reproduced across two runs + a live interactive session
Evidence links / log paths: /newroot/var/log/op3-weston-run*.log,
  /run/op3-weston/weston.log (session), ACM console relay

Conclusion: all three PASS criteria met on the pmOS 6.12 control. The Wayland
  hardware-compositing path is SUPPORTED by evidence; promotion to accepted is
  the Integration role's decision. This does NOT yet cover the browser
  (layer 07 Cog/WPE) or Linux 7.2.
Uncertainties: weston-flower (cairo client) displayed white/static in one
  session — cairo-toytoolkit animation path was not investigated further since
  the EGL client (simple-egl) is the gate's criterion. The desktop clock
  animation was not checked. Cosmetic data-file warnings (fontconfig, xkb
  compose, weston cursors) appear because /usr/share symlinks cover only what
  was needed; harmless for the gate.
Recommended next experiment: layer 07 browser gate (Cog + WPE WebKit) — large
  Buildroot build (WebKit), owner-run per AGENTS.md or with explicit owner
  authorization. Also worth checking the weston top-bar clock animation as a
  free extra data point.
```

## PASS / FAIL record

PASS requires all of the following:

1. weston starts with the DRM backend and the GL renderer on `/dev/dri/card0`
   (`gl-renderer` in its log, no EGL/GBM errors).
2. The panel shows the compositor output (weston desktop) for the hold
   interval, and the weston-simple-egl spinning triangle is visible — a
   Wayland client rendered end-to-end through the compositor and the GPU.
3. The result is reproducible in a second run.

FAIL is a weston abort, a missing `gl-renderer`/`drm-backend` module, an
EGL/GBM initialisation error inside weston, a black panel, or a hang. A
software-renderer result (llvmpipe instead of freedreno in the weston log) is
evidence, not a PASS.

A PASS here does not by itself accept the browser; layer 07 (Cog / WPE
WebKit) is a separate gate and a separate Buildroot build.

## Result (2026-08-30): PASS

Reached over six device iterations; each fix is in
`boot/weston-test/opt/op3-weston/run.sh` with a comment:

1. Library path: the runner's `--library-path` must include `usr/lib` and
   `usr/lib/libweston-14` in addition to `lib` (glibc), or nothing loads.
2. Module paths: weston 14 dlopens backends/renderers from
   `/usr/lib/libweston-14` and shells from `/usr/lib/weston`; both are bridged
   with symlinks on the live rootfs.
3. Input: buildroot's eudev ships no `input_id` rules file, so libinput
   rejected every device ("not tagged as supported input device") and weston
   aborted with zero inputs. run.sh writes a rule invoking the `input_id`
   builtin; after that the Synaptics S3508 touchscreen and the power key are
   both attached.
4. Helpers: weston execs `/usr/libexec/weston-desktop-shell` and
   `/usr/libexec/weston-keyboard` directly; the initramfs has no dynamic
   loader, so both exited status 1 and weston quit ("apparently cannot run at
   all"). They are replaced by wrapper scripts execing through the bundled
   loader.
5. Stale processes: a weston forked twin survives a TERM aimed at the tracked
   pid; the survivor keeps DRM master ("Could not make device fd drm master:
   Device or resource busy") and the old socket, so the next run renders
   nothing and clients assert. run.sh sweeps `/proc` by full cmdline at start
   and stop.
6. Mesa wayland platform: the first Mesa build predated
   `BR2_PACKAGE_WAYLAND`, so client-side EGL had no wayland platform
   ("Native platform type: surfaceless") and `eglChooseConfig` returned zero
   configs (simple-egl assert at init_egl). `mesa3d-dirclean` + rebuild with
   `-Dplatforms=wayland` fixed it. Lesson: adding a package that changes
   Mesa's dependencies requires a Mesa rebuild; incremental Buildroot does
   not do it automatically.

Also noted: the runtime dir must be cleaned before weston starts (stale locks
push the socket to `wayland-1`); the runner detects the real socket name and
passes it to the client via `WAYLAND_DISPLAY`.

## Bundle layout and absolute-path bridging

Weston locates its modules through compile-time-absolute paths, and several
libraries read their data from absolute paths too. None of these can be
redirected with an environment variable, so the bundle is staged as a whole
Buildroot target tree (`scripts/stage-weston-rootfs.sh`) and run.sh creates
these symlinks on the live (ephemeral) initramfs rootfs before starting
weston:

| Absolute path used by | Points into the bundle |
| --- | --- |
| `/usr/lib/weston/*.so` (weston dlopen: drm-backend, gl-renderer, desktop-shell) | `/opt/op3-weston/usr/lib/weston` |
| `/usr/lib/udev` (eudev daemon, rules) | `/opt/op3-weston/usr/lib/udev` |
| `/usr/share/libinput` (device quirks) | `/opt/op3-weston/usr/share/libinput` |
| `/usr/share/X11/xkb` (libxkbcommon keymaps) | `/opt/op3-weston/usr/share/X11` |

Environment the run.sh sets: `GBM_BACKENDS_PATH=/opt/op3-weston/usr/lib/gbm`,
`XDG_RUNTIME_DIR=/run/op3-weston`. `LD_LIBRARY_PATH` must never be exported
(see the EGL gate: busybox crashes, kmscube SIGBUS); every bundle binary is
invoked through the bundled glibc loader explicitly.

libseat runs in its builtin root mode — no seatd daemon needed for the smoke
test; the bundle carries seatd as a fallback. eudev is started and
`udevadm trigger`/`settle` populate `/run/udev` before weston starts, because
libinput enumerates through the udev database; missing input devices are
tolerated for this gate (touch panel drivers are a later layer).

## Reproduction

```bash
# build (incremental on the EGL output; toolchain and Mesa are reused)
cp buildroot/op3-weston.defconfig source/buildroot/configs/op3_weston_defconfig
PATH="$HOME/gnubin:$PATH" make -C source/buildroot O=$PWD/out/buildroot-op3-egl \
  op3_weston_defconfig
PATH="$HOME/gnubin:$PATH" make -C source/buildroot O=$PWD/out/buildroot-op3-egl \
  -j"$(nproc)"

# stage + deploy
scripts/stage-weston-rootfs.sh out/buildroot-op3-egl/target \
  artifacts/op3-weston-bundle.tar.gz
cat artifacts/op3-weston-bundle.tar.gz | ssh root@172.16.42.1 \
  'mkdir -p /newroot/opt && tar -xzf - -C /newroot'

# boot image (the launcher waits for /newroot/opt/op3-weston/run.sh)
OVERLAY_SOURCE="$PWD/boot/weston-test" scripts/make-drm-test-initrd.sh \
  artifacts/reference-initrd.img artifacts/op3-drm-dumb \
  artifacts/initrd-op3-weston.cpio.gz
scripts/pack-boot.sh \
  out/pmos-msm8996-v6.12-v74strict/arch/arm64/boot/Image.gz \
  out/pmos-msm8996-6.3.1-v74full/arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb \
  artifacts/initrd-op3-weston.cpio.gz \
  artifacts/boot-oneplus3-pmos612-v74dtb-weston.img
```

CRITICAL: `OVERLAY_SOURCE="$PWD/boot/weston-test"` is mandatory — without it
the script silently bakes in the DRM RGB launcher (observed 2026-08-30).
Verify the built initramfs by decompressing ALL gzip members (zcat only
decompresses the first) and checking that the overlay member contains the
weston launcher.
