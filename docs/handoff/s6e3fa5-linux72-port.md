# S6E3FA5 Linux 7.2 forward-port handoff

## Scope

This task forward-ports only Samsung S6E3FA5 panel support to the Linux v7.2
upstream tree for the OnePlus 3. It does not modify DRM/MSM, Adreno, IOMMU,
runtime PM, clocks, regulators outside the panel consumer, or userspace.

## Baseline

- Kernel: Linux v7.2 pristine upstream
- Baseline commit: `8d3ae59288f1e7d58d76558a6ee96d533bc5019f`
- Device: OnePlus 3 / MSM8996
- Panel identity: owner confirmed S6E3FA5
- Kernel work branch: `agent/implementation/s6e3fa5-linux72-port`
- Kernel commit: `d73a640c6af7ad461bce8f54967c0abdf44d1204`
- Config fragment: `kernel/configs/oneplus3-s6e3fa5.fragment`

## pmOS reference

- Local reference: `source/linux-pmos-msm8996-6.3.1`
- Local reference commit: `9895e7e38b829a810b9f75d1f98c9e4349ae454a`
- Remote: `https://gitlab.com/msm8996-mainline/linux.git`
- Newer reference audited: `msm8996-stable-6.19.y`

The newer reference retains the FA5 command sequence and adds
`panel.prepare_prev_first = true`. No legacy DRM/MSM or GPU change was ported.

## Commits

- `f01bc7b46` `dt-bindings: display: panel: add Samsung S6E3FA5`
- `7ecc3b8d6` `drm/panel: add Samsung S6E3FA5 panel support`
- `d73a640c6` `arm64: dts: qcom: msm8996-oneplus: enable S6E3FA5 panel`

## Files changed

- `Documentation/devicetree/bindings/display/panel/samsung,s6e3fa5.yaml`
- `drivers/gpu/drm/panel/panel-samsung-s6e3fa5.c`
- `drivers/gpu/drm/panel/Kconfig`
- `drivers/gpu/drm/panel/Makefile`
- `arch/arm64/boot/dts/qcom/msm8996-oneplus-common.dtsi`
- `kernel/configs/oneplus3-s6e3fa5.fragment`

## API adaptations

- Replaced legacy `drm_panel_init()` allocation with v7.2
  `devm_drm_panel_alloc()`.
- Replaced the local legacy DCS write macro with v7.2 MIPI-DSI multi-context
  helpers, preserving the FA5 command sequence and delays.
- Used `backlight_get_brightness()` instead of reading the backlight property
  directly.
- Set `prepare_prev_first = true`, matching the newer pmOS reference.
- Added attach-failure cleanup and regulator cleanup after failed panel setup.

## DTS delta

The upstream OnePlus common DTS remains the base. Its only added panel delta is
the S6E3FA5 node below `mdss_dsi0` and its reciprocal graph endpoint.

| Property | Reason |
| --- | --- |
| `compatible`, `reg` | Bind the FA5 DSI peripheral on virtual channel 0. |
| `vddio-supply`, `vdda-supply` | Required by the driver’s two regulator consumers. |
| `enable-gpios`, `reset-gpios` | Required by the driver’s power/reset sequence. |
| `pinctrl-*` | Reuses existing upstream reset and TE pin states. |
| graph endpoints | Connect the MSM DSI host output to the panel. |

The unsupported legacy `disp-te-gpios` property was not carried forward.

## Build result

- `git describe`: `v7.2-3-gd73a640c6`
- Kernel source status: clean on `agent/implementation/s6e3fa5-linux72-port`
- Compiler prepared: `aarch64-linux-gnu-gcc-11` 11.5.0
- DTB target: `msm8996-oneplus3.dtb`
- Static checks: `git diff --check` clean; `checkpatch --strict` reports no
  errors (only the generic new-file MAINTAINERS advisory).
- Owner-run build completed successfully with the corrected fragment.
- Final configuration SHA256:
  `84e811949e5d8cab5e55109384711190ecdfe86eac94d7d8c45bf9a85c1b6ff1`
- Final configuration confirms `CONFIG_DRM=y`, `CONFIG_DRM_MSM=y`,
  `CONFIG_BACKLIGHT_CLASS_DEVICE=y`, and
  `CONFIG_DRM_PANEL_SAMSUNG_S6E3FA5=y`.
- Build log confirms `panel-samsung-s6e3fa5.o` was compiled and linked into
  `drivers/gpu/drm/panel/built-in.a`.
- `dtbs_check`: not run; owner action required.

## Owner manual build command

The following command uses the current Linux 7.2 FA5 branch, applies only the
tracked FA5 config fragment to an out-of-tree configuration, and builds only
`Image.gz` plus `msm8996-oneplus3.dtb`:

```bash
project=/home/kai/src/oneplus3-mainline
kernel="$project/source/linux-7.2"
output="$project/out/linux-7.2-oneplus3-s6e3fa5"
mkdir -p "$output" && \
make -C "$kernel" O="$output" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 defconfig && \
"$kernel/scripts/kconfig/merge_config.sh" -m -O "$output" "$output/.config" "$project/kernel/configs/oneplus3-s6e3fa5.fragment" && \
make -C "$kernel" O="$output" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 olddefconfig && \
grep -qx 'CONFIG_DRM=y' "$output/.config" && \
grep -qx 'CONFIG_DRM_MSM=y' "$output/.config" && \
grep -qx 'CONFIG_BACKLIGHT_CLASS_DEVICE=y' "$output/.config" && \
grep -qx 'CONFIG_DRM_PANEL_SAMSUNG_S6E3FA5=y' "$output/.config" && \
grep -qx '# CONFIG_QCOM_LLCC is not set' "$output/.config" && \
grep -qx '# CONFIG_QCOM_OCMEM is not set' "$output/.config" && \
make -C "$kernel" O="$output" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 -j"$(nproc)" Image.gz qcom/msm8996-oneplus3.dtb
```

It does not apply or modify DRM/MSM, GPU, runtime PM, or legacy patches. The
agent did not execute this command.

The owner initially ran the command with
`arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb`; Kbuild treated that as a
relative DTB target and looked for the path twice, so the invocation failed
before producing the requested targets. Use the `qcom/msm8996-oneplus3.dtb`
target above to continue with the existing output directory.

The first two owner-run builds resolved `CONFIG_DRM_PANEL_SAMSUNG_S6E3FA5=m`.
The first was limited by `CONFIG_BACKLIGHT_CLASS_DEVICE=m`; the second had
backlight built in, but the parent `CONFIG_DRM=m` still limited all panel
drivers to modules. The final panel-only build set `CONFIG_DRM=y` and verified
FA5 resolves to `=y`, but it retained `CONFIG_DRM_MSM=m` and therefore cannot
provide a rootfs-independent DSI probe.

For the boot.img-only test, the fragment now sets `CONFIG_DRM_MSM=y`. Its only
module-valued direct dependencies in the defconfig are `QCOM_LLCC` and
`QCOM_OCMEM`; neither is referenced by the MSM8996 OnePlus DTS, so the
fragment explicitly disables them. This permits built-in MSM DRM without any
legacy patch or DRM/MSM source modification.

The current `scripts/build-kernel.sh` intentionally rejects any source commit
other than pristine `8d3ae592...`; it will therefore reject this port branch.
Do not weaken that guard as part of this panel task. Integration must provide
an approved patched-kernel build invocation or adjust the build policy in a
separate, scoped task before the owner builds this branch.

## Artifact SHA256

- `Image.gz`: `4a636bc445a000f8f57208220472109752b0f8e3799e0c7778151d1700e48b56`
- `msm8996-oneplus3.dtb`: `87963b9340d437abb6bfe387e05327bcb24766e81d515df9513ce0da1a4eea45`

## Boot-image packaging result

- Packer profile: `boot/oneplus3-fa5.env`
- Kernel payload: `Image.gz` followed by the raw OnePlus 3 DTB, as required
  by the profile.
- Initrd: local ignored temporary reference artifact
  `artifacts/reference-initrd.img`
  (`c3358a1cadb747996ddaa492e636827f2d72974040e8fd40d81f8a213e676366`).
  It was extracted from the known-good `boot_fa5_v100_auto.img`; it is only
  for minimal bring-up and is not committed.
- Actual cmdline: `fbcon=nodefault console=tty0 pmos.debug-shell`
  (the reference image's old pmOS boot/root UUID arguments were not reused).
- Output: local ignored
  `artifacts/boot-oneplus3-fa5-linux72.img`
  (`88c91c749c87f32d78df374dd3bcc57af81512b1c2dc1b67aee7bd8f6fd8ad59`).
- `abootimg -i` verified an Android boot header v0, 4096-byte pages, kernel
  address `0x80008000`, ramdisk address `0x81000000`, tags address
  `0x80000100`, and the cmdline above.
- Packaging only was performed: no fastboot command, device boot, or device
  test was executed by the agent.

## Minimal-initramfs A/B artifact

After the first owner `fastboot boot` returned to fastboot, a second boot image
was prepared with the initrd as the sole changed variable. The hypothesis is
that the legacy pmOS initrd, not the kernel or DTB, triggered the return by
depending on its old root selection.

- Source: `boot/minimal-initramfs/init.c`
- Behaviour: static ARM64 PID 1; mounts devtmpfs/procfs/sysfs when available,
  writes an `op3-minimal-init` marker to `/dev/kmsg`, and remains alive. It
  does not mount a root filesystem or read any pmOS UUID.
- Source compiler: `aarch64-linux-gnu-gcc-11 -static`
- Initramfs format: gzip-compressed `newc`, containing only `/init`.
- `op3-minimal-init` SHA256:
  `8441a06dc1b97abe4d291352ecc293e7b96eaf9490cab6fc979e88163ec1375b`
- `initrd-op3-minimal.cpio.gz` SHA256:
  `a107e55323dba324b28fa2185d1e92dcdb1ed666c94b49effafaa8feaa47763b`
- Output boot image: local ignored
  `artifacts/boot-oneplus3-fa5-linux72-minimal-init.img`
  (`d3a2f539893019309f706a5bfb4af7d684625f8faa1d271526f5cc884282b87a`).
- Profile and cmdline are unchanged:
  `fbcon=nodefault console=tty0 pmos.debug-shell`.
- `abootimg -i` verified header v0, 4096-byte pages, kernel address
  `0x80008000`, ramdisk address `0x81000000`, and tags address `0x80000100`.

PASS for this A/B is that the device no longer returns to fastboot when the
owner executes the same non-persistent `fastboot boot` procedure. FAIL means
the failure is before this init executes (or otherwise independent of the old
initrd); neither result by itself establishes panel output. No fastboot or
device operation was executed by the agent.

## Device test result

Not tested. The owner reported that the first boot image was accepted by
fastboot (`Sending` and `Booting` both `OKAY`) but the device returned to
fastboot. A second boot image with the minimal initramfs is ready for the next
owner-run A/B. No OnePlus 3 DRM/MSM probe, DSI attach, panel prepare/enable,
KMS, or dumb-buffer RGB result is claimed.

## USB COM evidence

None collected. USB COM logging must be enabled before the first owner-run boot.

## Remaining issues

- The owner build now verifies `CONFIG_DRM=y`, `CONFIG_DRM_MSM=y`,
  `CONFIG_BACKLIGHT_CLASS_DEVICE=y`, and
  `CONFIG_DRM_PANEL_SAMSUNG_S6E3FA5=y`, while disabling the unreferenced
  optional `QCOM_LLCC` and `QCOM_OCMEM` dependencies.
- The temporary initrd is legacy pmOS early userspace and is not a formal
  rootfs solution; replace it with Buildroot's `rootfs.cpio.gz` for the
  long-term boot path.
- The minimal initramfs is diagnostic-only and intentionally has no shell,
  storage mounts, or display userspace.
- The binding and DTS require owner-run DT schema validation.
- The existing pristine-only build script requires an approved separate build
  policy change or a documented patched-kernel invocation before it can build
  this branch.
- Panel timing, regulator sequencing, reset state, DSI attach, TE behavior and
  RGB output need device evidence.

## Next recommended step

The owner may test
`artifacts/boot-oneplus3-fa5-linux72-minimal-init.img` with the same
non-persistent fastboot boot procedure. If it remains out of fastboot, record
that result before replacing this diagnostic initramfs with Buildroot. If it
still returns, investigate only early kernel/DTB boot evidence; do not change
GPU, Weston, Cog, or WPE.
