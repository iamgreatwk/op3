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
- Initial panel-driver configuration/build: owner-run, but FA5 resolved to
  module (`=m`); not accepted for built-in panel bring-up.
- Initial DTB/Image build: owner-run; the artifacts below must be replaced
  after rebuilding with the corrected fragment.
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
make -C "$kernel" O="$output" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 -j"$(nproc)" Image.gz qcom/msm8996-oneplus3.dtb
```

It does not apply or modify DRM/MSM, GPU, runtime PM, or legacy patches. The
agent did not execute this command.

The owner initially ran the command with
`arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb`; Kbuild treated that as a
relative DTB target and looked for the path twice, so the invocation failed
before producing the requested targets. Use the `qcom/msm8996-oneplus3.dtb`
target above to continue with the existing output directory.

The first two successful builds resolved `CONFIG_DRM_PANEL_SAMSUNG_S6E3FA5=m`.
The first was limited by `CONFIG_BACKLIGHT_CLASS_DEVICE=m`; the second had
backlight built in, but the parent `CONFIG_DRM=m` still limited all panel
drivers to modules. The fragment now also sets `CONFIG_DRM=y` and
`CONFIG_DRM_MSM=y`, which is required to build the MSM DSI host and FA5 panel
into the kernel. Rerun the configuration and build command before using these
artifacts; its exact-value checks must all pass.

The current `scripts/build-kernel.sh` intentionally rejects any source commit
other than pristine `8d3ae592...`; it will therefore reject this port branch.
Do not weaken that guard as part of this panel task. Integration must provide
an approved patched-kernel build invocation or adjust the build policy in a
separate, scoped task before the owner builds this branch.

## Artifact SHA256

- Initial `Image.gz`: `633eb8d63b2ebc00ad8b8adb2da06812d0fb8497a320c80e6958370759748347`
- Initial `msm8996-oneplus3.dtb`: `87963b9340d437abb6bfe387e05327bcb24766e81d515df9513ce0da1a4eea45`

These artifacts must not be used for panel testing because FA5 resolved as a
module in that build.

## Device test result

Not tested. No OnePlus 3 boot, DRM/MSM probe, DSI attach, panel prepare/enable,
KMS, or dumb-buffer RGB result is claimed.

## USB COM evidence

None collected. USB COM logging must be enabled before the first owner-run boot.

## Remaining issues

- The corrected fragment requests `CONFIG_DRM=y`, `CONFIG_DRM_MSM=y`,
  `CONFIG_BACKLIGHT_CLASS_DEVICE=y`, and
  `CONFIG_DRM_PANEL_SAMSUNG_S6E3FA5=y`; the owner must rebuild and verify that
  all four survive Kconfig resolution.
- The binding and DTS require owner-run DT schema validation.
- The existing pristine-only build script requires an approved separate build
  policy change or a documented patched-kernel invocation before it can build
  this branch.
- Panel timing, regulator sequencing, reset state, DSI attach, TE behavior and
  RGB output need device evidence.

## Next recommended step

First obtain Integration approval for a patched-kernel build invocation without
weakening the pristine-source guard in this task. Then enable the FA5 driver in
the OnePlus 3 kernel configuration, build `msm8996-oneplus3.dtb`, and collect
USB COM output. Validate in order: boot, DRM/MSM probe, DSI probe, S6E3FA5
probe, prepare/enable, KMS connector/mode, then a dumb-buffer sequence of solid
red, green, blue, white and black. Do not investigate GPU, Weston, Cog, or WPE
until this DRM RGB milestone has evidence.
