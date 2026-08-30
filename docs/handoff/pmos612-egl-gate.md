# OnePlus 3 pmOS 6.12 EGL gate

```text
Task / GitHub Issue: Owner-authorized no-Issue EGL smoke test
Role: Implementation
Formal baseline: Linux v7.2 pristine upstream (unchanged)
Diagnostic kernel: pmOS 6.12 (v6.12-v74strict) full-initrd control
Baseline commit: 2073241 (repository HEAD when this task started)
Working branch: agent/implementation/s6e3fa5-linux72-port
Changed files: scripts/prepare-a530-firmware.sh; scripts/make-drm-test-initrd.sh;
  boot/egl-test/sbin/run_recovery.sh; boot/egl-test/opt/op3-egl/run.sh;
  buildroot/op3-egl.defconfig; scripts/stage-egl-rootfs.sh

Layer: 05 EGL, one layer above 04 DRM RGB
Previous PASS milestone: OP3-DRM-005, DRM dumb-buffer RGB gate PASS,
  red → green → blue → red on the pmOS 6.12 control
Sole hypothesis: With the Adreno 530 firmware present and a Buildroot-built
  Mesa staged on sda15, an EGL/GBM program can create a surface on
  /dev/dri/card0 and render to the panel.
Only variable changed per test:
  1. GPU firmware step: qcom/a530_pm4.fw and qcom/a530_pfp.fw added to the
     initramfs overlay (35 KB; kernel needs them during GPU probe).
  2. EGL step: the EGL bundle on sda15 (Mesa, libdrm, kmscube, run.sh). The
     boot image carries only the small launcher plus the firmware.

Build run by project owner: Buildroot (Mesa, libdrm, mesa-demos). Large build;
  agents do not run it.
Build result: NOT_RUN
Artifacts and SHA256: filled in by the owner after the Buildroot build

Device test run by project owner: 2026-08-30, two consecutive boots; the
  launcher auto-ran kmscube on each boot. Two further manual runs (30 s each)
  confirmed the visible output.
Device result: all three criteria met — EGL 1.5, GL renderer `FD530`
  (freedreno hardware), OpenGL ES 3.1 Mesa 26.0.1, kmscube 30 s at
  59.83 fps (1680 frames) in both manual runs, cube visible and rotating
  (owner confirmed twice), no GL errors. Two run.sh bugs were fixed during
  this gate: exporting `LD_LIBRARY_PATH` broke every busybox applet the script
  called and made the test mix loaders and crash with SIGBUS, and kmscube
  treats any readable stdin (including /dev/null EOF) as `user interrupted!`
  and exits after one frame; run.sh now feeds kmscube's stdin from a
  `sleep "$TEST_SECONDS"` pipe. The launcher also disables A530 runtime PM
  before running the test (see docs/known-issues.md).
Evidence links / log paths: /newroot/var/log/op3-egl.log (persistent on sda15),
  /var/log/op3-egl.log (session copy), dmesg, kernel console on ACM
  (`console=ttyGS0`, captured host-side with `cat /dev/ttyACM0`)

Conclusion: all three PASS criteria met on the pmOS 6.12 control. The EGL
  hardware-rendering path is SUPPORTED by evidence. The runtime-PM resume hang
  remains as a known issue (mitigated at boot time by the launcher, root cause
  is the DTB's dummy GPU regulators)
Uncertainties: the runtime-PM resume hang has no console trace (hard reset) and
  no pstore record; a late manual run over SSH must keep runtime PM disabled
Recommended next experiment: closed — next layer is 06 (Wayland/compositor),
  which is a new gate and out of scope here
```

## Firmware step (prepared, image ready)

The kernel requests `qcom/a530_pm4.fw` and `qcom/a530_pfp.fw` while the GPU
probes, which happens before any user-space root filesystem exists, so those two
files must be inside the initramfs, and so does `qcom/a530v3_gpmu.fw2`.

GPMU is loaded even though `a5xx_gpmu_init()` tolerates its absence: the owner
observed GPU runtime PM blanking the panel when the GPMU microcode is missing,
so it is treated as required, not optional.

Zap is already covered and is not part of this change: the v74 DTB contains

```text
zap-shader {
	memory-region = <0x3f>;
	firmware-name = "qcom/msm8996/oneplus3/a530_zap.mbn";
};
```

and the reference initramfs already ships that file at exactly that path
(`lib/firmware/qcom/msm8996/oneplus3/a530_zap.mbn`). The driver only prints on
zap failure, and no zap error appears in `dmesg`.

```bash
scripts/prepare-a530-firmware.sh    # verifies SHA256 against pinned values
scripts/make-drm-test-initrd.sh
scripts/pack-boot.sh \
  out/pmos-msm8996-v6.12-v74strict/arch/arm64/boot/Image.gz \
  out/pmos-msm8996-6.3.1-v74full/arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb \
  artifacts/initrd-op3-drm-test.cpio.gz \
  artifacts/boot-oneplus3-pmos612-v74dtb-gpu-fw-gpmu.img
```

```text
a530_pm4.fw      6419f35956ec7307af83723fedfba752520bacd8389eda0d0120e185e4cb1d3f  19572 bytes
a530_pfp.fw      7ab3cd917e1f875f6a8387f8bc5efcf11ce9c88542ef2fc3cbda7d4b7b163286  16144 bytes
a530v3_gpmu.fw2  3124b0c8cf84fd6db5cb063b37c51386dc64b5416c4ba2713e12106d9ffa5daf   8184 bytes

image             cf4b0d49b8ce8b4b352fcb5b672cbf35339c4f91ecf739b6fd955b5889592d40
                  (boot-oneplus3-pmos612-v74dtb-gpu-fw-gpmu.img, 62967808 bytes)
kernel payload    50ffea424e6b7625b30acd5b14673ea287d2b04c7c283ee145f895247d8e881a (unchanged)
ramdisk           2a25597df0061faf3d94173624b9f0a616538ab99b681ed243580ad2f6510505
previous image    ae108bc8aefeb34dcd2c1f07a952b6067469049ff3ed00c422575df3cfc6d004
                  (pm4 + pfp only; superseded, kept as the evidence artifact)
```

Adding GPMU grew the boot image by exactly one 4096-byte page.

This image uses the **DRM RGB launcher**, so it also runs the colour sequence
and dumps `dmesg` into `/newroot/var/log/op3-drm-dumb.log`.

### Result (2026-08-30): PASS

The owner booted `ae108bc8…`; the RGB sequence still ran correctly, and:

```text
[    1.956857] [drm:adreno_request_fw] loaded qcom/a530_pm4.fw from new location
[    1.956971] [drm:adreno_request_fw] loaded qcom/a530_pfp.fw from new location
/sys/class/devfreq/b00000.gpu/  cur=19200000  max=624000000  governor=simple_ondemand
```

Both firmware files on the device hash to
`6419f35956ec7307af83723fedfba752520bacd8389eda0d0120e185e4cb1d3f` and
`7ab3cd917e1f875f6a8387f8bc5efcf11ce9c88542ef2fc3cbda7d4b7b163286`, matching the
staged copies. The devfreq node for the GPU is the decisive evidence: it only
exists once the Adreno core has initialised.

That run also showed one remaining failure:

```text
[drm:adreno_request_fw] *ERROR* failed to load qcom/a530v3_gpmu.fw2: -2
```

`a5xx_gpmu_init()` tolerates it (`if (!a5xx_gpu->gpmu_dwords) return 0;`), so the
GPU still came up. The owner nevertheless required it, because GPU runtime PM
has been observed to blank the panel without the GPMU microcode. `a530v3_gpmu.fw2`
was therefore added in the image above; the EGL gate must run with the GPMU
image, not with `ae108bc8…`.

### GPMU addition (2026-08-30): PASS

Booting `cf4b0d49…`:

```text
[    2.050191] [drm:adreno_request_fw] loaded qcom/a530_pm4.fw from new location
[    2.050412] [drm:adreno_request_fw] loaded qcom/a530_pfp.fw from new location
[    2.050585] [drm:adreno_request_fw] loaded qcom/a530v3_gpmu.fw2 from new location
```

No `failed to load` line remains. All four firmware files (pm4, pfp, gpmu and
the DTB-named zap image) are present and match the staged SHA256 values.

GPU runtime PM is alive and the panel stays lit, which is the point of this
step:

```text
runtime_status          suspended
control                 auto
autosuspend_delay_ms    250
runtime_suspended_time  227337 ms
runtime_active_time    310 ms
devfreq b00000.gpu      cur 27 MHz, max 624 MHz, simple_ondemand
card0-DSI-1             connected, enabled; backlight 255
```

The RGB sequence again exited 0 for red, green and blue. The EGL gate runs on
this firmware set.

## Layout: what goes where

| Location | Contents | Why |
| --- | --- | --- |
| initramfs overlay | `sbin/run_recovery.sh` (a few KB), `lib/firmware/qcom/a530_*.fw` (35 KB) | The launcher must run before sda15 is usable, and the kernel needs the firmware during GPU probe. Nothing else. |
| sda15 `/opt/op3-egl/` | `bin/kmscube`, `lib/` (libEGL, libGLESv2, libgbm, libdrm, `libgallium-26.0.1.so`, glibc loader and libraries), `lib/gbm/dri_gbm.so`, `run.sh` | Tens of megabytes; the OnePlus 3 boot.img size limit forbids putting it in the boot image. Editing `run.sh` here changes the test without repacking or re-flashing. Mesa 26 has no `libglapi` and no per-driver `msm_dri.so`: every Gallium driver, freedreno included, is inside `libgallium-26.0.1.so`. The bundle carries its own glibc loader and libc, and `run.sh` invokes the test through that loader, so it does not depend on the libc of the sda15 rootfs. |
| sda15 `/var/log/op3-egl.log` | Test output, sysfs state, `dmesg` | Survives reboot. |

## Host environment: uutils coreutils

This host (Ubuntu 26.04) ships uutils implementations for many coreutils
commands: `install`, `stat`, `dd`, `mkdir`, `ln`, `sort`, `cut`, `tr`, `wc`, `od`,
`split`, `uniq`, `basename` and `dirname` all resolve to
`/usr/lib/cargo/bin/coreutils/`. Buildroot's
`support/dependencies/dependencies.sh` rejects uutils `install` outright:

```text
You have an uutils 'install' version installed which is affected by:
  https://github.com/uutils/coreutils/issues/12166
```

GNU coreutils are installed alongside them as `/usr/bin/gnu<name>`
(`/usr/bin/gnuinstall` is GNU coreutils 9.7). Put them first on `PATH` for the
build; this needs no root and changes no system file:

```bash
mkdir -p "$HOME/gnubin"
for gnu in /usr/bin/gnu*; do
  n=${gnu#/usr/bin/gnu}
  case "$n" in '['|test) continue;; esac
  ln -sf "$gnu" "$HOME/gnubin/$n"
done
PATH="$HOME/gnubin:$PATH" install --version | head -1   # expect "GNU coreutils"
```

Then prefix every Buildroot `make` invocation with that `PATH`, as in step 1
below. The system-wide alternative is
`sudo update-alternatives --install /usr/bin/install install /usr/bin/gnuinstall 100`,
but `/usr/bin/install` is a plain symlink that alternatives does not manage, so
that command may refuse.

## Owner step 1: build the EGL bundle with Buildroot

Use the **2026.02.x** branch, not 2025.02. Buildroot 2025.02 ships host-m4
1.4.19, whose bundled gnulib does not build with the host GCC 15.2
(`GL_OSET_INLINE _GL_ATTRIBUTE_NODISCARD int` → `expected identifier or '('
before 'int'`): GCC 15 defaults to `-std=gnu23`, which enables the C23
`[[nodiscard]]` spelling at a position gnulib does not expect. 2026.02.x ships
m4 1.4.21 and builds cleanly. The four symbols this defconfig needs were
verified to exist on that branch on 2026-08-30.

```bash
git clone --depth 1 -b 2026.02.x https://gitlab.com/buildroot.org/buildroot.git source/buildroot
cp buildroot/op3-egl.defconfig source/buildroot/configs/op3_egl_defconfig
PATH="$HOME/gnubin:$PATH" make -C source/buildroot O="$PWD/out/buildroot-op3-egl" op3_egl_defconfig
PATH="$HOME/gnubin:$PATH" make -C source/buildroot O="$PWD/out/buildroot-op3-egl" -j"$(nproc)" \
  2>&1 | tee /tmp/buildroot-egl.log
```

After `op3_egl_defconfig`, verify that the Mesa symbols survived; kconfig drops
a symbol silently when a dependency is unmet, and without
`BR2_TOOLCHAIN_BUILDROOT_CXX`/`BR2_INSTALL_LIBSTDCPP` that is exactly what
happens to `BR2_PACKAGE_MESA3D` (only libdrm gets built):

```bash
grep -E "^BR2_PACKAGE_MESA3D=y|^BR2_PACKAGE_MESA3D_GALLIUM_DRIVER_FREEDRENO=y|^BR2_PACKAGE_MESA3D_OPENGL_EGL=y|^BR2_PACKAGE_KMSCUBE=y|^BR2_PACKAGE_LIBDRM_FREEDRENO=y|^BR2_INSTALL_LIBSTDCPP=y" \
  out/buildroot-op3-egl/.config
```

All six lines must be present. After the build, check the target before staging:

```bash
ls out/buildroot-op3-egl/target/usr/lib | grep -iE "libEGL|libgbm|libGLES|libglapi"
```

If a different host package hits the same class of error, force the older
language standard instead of switching branches again:

```bash
make -C source/buildroot O="$PWD/out/buildroot-op3-egl" \
  HOST_CFLAGS="-O2 -std=gnu17" HOST_CXXFLAGS="-O2 -std=gnu++17" -j"$(nproc)"
```

Set both variables: Buildroot makes `HOST_CXXFLAGS` inherit `HOST_CFLAGS`, and
`-std=gnu17` is not valid for g++. Downgrading the host compiler is not an
option on this machine; no gcc-14 candidate exists in the current repositories.

Symbol names in `buildroot/op3-egl.defconfig` were checked against Buildroot
master on 2026-08-30. Two traps were already handled:

- `kmscube` is its own package (`BR2_PACKAGE_KMSCUBE`). Older Buildroot shipped
  it inside `mesa3d-demos`; if your release does, use `BR2_PACKAGE_MESA3D_DEMOS=y`
  plus `BR2_PACKAGE_MESA3D_DEMOS_KMSCUBE=y` instead.
- `BR2_PACKAGE_MESA3D_OPENGL_EGL` selects `BR2_PACKAGE_MESA3D_GBM`, so gbm needs
  no separate symbol, and `BR2_PACKAGE_MESA3D_GALLIUM_DRIVER_FREEDRENO` selects
  `BR2_PACKAGE_LIBDRM_FREEDRENO` and requires arm/aarch64.

If a symbol still does not exist in the release you check out, Buildroot reports
it during `op3_egl_defconfig`; correct it there and record the change. Record the
Buildroot release and the resulting SHAs here.

```bash
scripts/stage-egl-rootfs.sh out/buildroot-op3-egl/target artifacts/op3-egl-bundle.tar.gz
```

That produces a tarball of `opt/op3-egl/` and refuses to build it if
`libEGL`, `libGLESv2`, `libgbm`, `libdrm`, `libglapi`, `msm_dri.so` or
`kmscube` are missing.

## Progress (2026-08-30)

- EGL bundle built and deployed to sda15 by the agent, with owner authorisation:
  `artifacts/op3-egl-bundle.tar.gz`,
  SHA256 `e7b1df232524b93abc01749100369d7d6e098071d714d9d789b35f59166cdba2`
  (7093126 bytes, self-contained: glibc loader, libc, libstdc++, Mesa 26 stack,
  kmscube, run.sh). Deployed as `/newroot/opt/op3-egl/`.
  Deployment note: the device busybox tar has no gzip support, so the transfer
  used `cat | ssh 'cat > file'` plus `zcat file | tar -x -C /newroot`.
- EGL boot image built by the agent:
  `artifacts/boot-oneplus3-pmos612-v74dtb-egl.img`,
  SHA256 `284936e65b53d947e6ecbdbb2fcbce005c7861ad3cfdd0d4aa2cc1c9976803f2`
  (62967808 bytes). Kernel payload unchanged
  (`50ffea424e6b7625b30acd5b14673ea287d2b04c7c283ee145f895247d8e881a`), ramdisk
  `artifacts/initrd-op3-egl.cpio.gz`
  (`648ab291d61f444c1f07bf8a695cb2f5879efbbc4476db0ccc90ff7115075fdf`) = EGL
  launcher plus the three GPU firmware files, plus the small RGB test binary
  kept for on-device comparison.
- Device test run by project owner: 2026-08-30, two consecutive boots of the
  EGL image; the launcher auto-ran the bundle on each boot.
- Device result: **all three PASS criteria met**. EGL 1.5 on `/dev/dri/card0`,
  GL renderer `FD530` (freedreno, hardware — not llvmpipe), OpenGL ES 3.1 Mesa
  26.0.1, GLSL ES 3.10; two launcher-run boots exited 0, and two further
  30-second manual runs each rendered 1680 frames at 59.83 fps with the cube
  visible and rotating (owner confirmation, twice).
- Two run.sh bugs found and fixed while reaching the visible output:
  1. Exporting `LD_LIBRARY_PATH` put the bundle's glibc on every busybox
     applet's path (crashes) and made kmscube mix loaders and die with SIGBUS.
     run.sh now runs its own diagnostics in a clean environment and passes the
     bundle libraries only through the bundled loader's `--library-path`.
  2. kmscube polls its stdin and exits `user interrupted!` as soon as stdin is
     readable — /dev/null, a closed channel and pipe EOF all count. Every
     earlier "exit=0" run had rendered about one frame. run.sh now feeds
     stdin from `sleep "$TEST_SECONDS" |`, which also provides the clean
     end-of-window exit.
  These live in the sda15 copy of `run.sh`; no image rebuild or re-flash was
  needed.
- Separately observed: running kmscube manually over SSH after the GPU had
  runtime-suspended rebooted the device (see docs/known-issues.md; the DTB GPU
  node has no vdd/vddcx supplies, so runtime resume has no real power
  sequence). The boot-time runs are unaffected because the GPU is still active
  then. This is recorded as OP3-EGL-001's caveat and is a candidate for its own
  task.

## Owner step 2: deploy the bundle to sda15

Boot the firmware image (or any image with USB RNDIS), then from the host:

```bash
cat artifacts/op3-egl-bundle.tar.gz | ssh root@172.16.42.1 'mkdir -p /newroot/opt && tar -xzf - -C /newroot'
```

## Owner step 3: build the EGL boot image

```bash
scripts/prepare-a530-firmware.sh
OVERLAY_SOURCE="$PWD/boot/egl-test" scripts/make-drm-test-initrd.sh \
  artifacts/reference-initrd.img artifacts/op3-drm-dumb \
  artifacts/initrd-op3-egl.cpio.gz
scripts/pack-boot.sh \
  out/pmos-msm8996-v6.12-v74strict/arch/arm64/boot/Image.gz \
  out/pmos-msm8996-6.3.1-v74full/arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb \
  artifacts/initrd-op3-egl.cpio.gz \
  artifacts/boot-oneplus3-pmos612-v74dtb-egl.img
```

## Owner step 4: boot and collect evidence

```text
fastboot boot artifacts/boot-oneplus3-pmos612-v74dtb-egl.img
```

The launcher waits up to 120 s for `/newroot/opt/op3-egl/run.sh`, logs the
kernel identity, `/dev/dri`, connector state and filtered `dmesg`, runs the
bundle for 30 s, then idles. Watch the panel during the 30 s window.

```text
cat /newroot/var/log/op3-egl.log
```

## PASS / FAIL record

PASS requires all of the following:

1. The bundle reports EGL initialisation on `/dev/dri/card0` with the freedreno
   hardware driver (`msm_dri.so`), not a software renderer.
2. The panel shows the rendered output (for `kmscube`, the rotating cube) for the
   hold interval.
3. The result is reproducible in a second run.

FAIL is an EGL or GBM initialisation error, a missing `msm_dri.so`, a GPU
firmware error in `dmesg`, a black panel, or a hang. Record the exact output and
the observed panel state. A software-rendering result (llvmpipe) is evidence,
not a PASS: it means the GPU path is still not usable.

Only the Integration role may promote a result to accepted; this handoff records
evidence. A PASS here does not establish Wayland, Weston, Cog, WPE, or Linux 7.2
readiness.
