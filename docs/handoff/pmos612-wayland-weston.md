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
  Run by the agent with owner authorization, same as the Mesa build.
Build result: NOT_RUN (in progress while this doc was written)
Artifacts and SHA256: filled in after the build

Device test run by project owner: pending
Device result: NOT_RUN
Evidence links / log paths: /newroot/var/log/op3-weston.log (persistent on
  sda15), ACM console (kernel log relay + debug shell, see
  docs/known-issues.md)

Conclusion: pending
Uncertainties: weston DRM backend module paths are compile-time-absolute
  (/usr/lib/weston); run.sh bridges them with symlinks on the live initramfs
  rootfs. libinput needs the udev database; the bundle starts udevd and runs
  udevadm trigger/settle. No input devices may exist (touch panel driver
  untested) — the gate does not require input.
Recommended next experiment: boot the weston image with the ACM capture
  running, watch the 40 s window (weston desktop + 25 s weston-simple-egl
  cube), then record the result row
```

## PASS / FAIL record

PASS requires all of the following:

1. weston starts with the DRM backend and the GL renderer on `/dev/dri/card0`
   (`gl-renderer` in its log, no EGL/GBM errors).
2. The panel shows the compositor output (weston desktop) for the hold
   interval, and the weston-simple-egl cube is visible — a Wayland client
   rendered end-to-end through the compositor and the GPU.
3. The result is reproducible in a second run.

FAIL is a weston abort, a missing `gl-renderer`/`drm-backend` module, an
EGL/GBM initialisation error inside weston, a black panel, or a hang. A
software-renderer result (llvmpipe instead of freedreno in the weston log) is
evidence, not a PASS.

A PASS here does not by itself accept the browser; layer 07 (Cog / WPE
WebKit) is a separate gate and a separate Buildroot build.

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
