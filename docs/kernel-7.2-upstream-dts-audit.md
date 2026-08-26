# Linux v7.2 upstream OnePlus 3 DTS audit

## Scope and result

This is a static audit for the clean-rebuild boot baseline only.  It verifies
the upstream source identity and the DTS include/build path; it does not claim
that a kernel boots and it does not alter any DTS.

| Item | Result |
| --- | --- |
| Upstream remote | `https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git` |
| Required tag | `v7.2` |
| Tag commit | `8d3ae59288f1e7d58d76558a6ee96d533bc5019f` |
| Commit subject | `Linux 7.2` |
| Source working-tree state at audit | clean, detached at `v7.2` |
| DTS result | All required DTS files exist in pristine upstream. |
| DTB build entry | `arch/arm64/boot/dts/qcom/Makefile` includes `msm8996-oneplus3.dtb` under `CONFIG_ARCH_QCOM`. |

## DTS path and dependency chain

The board file is
`arch/arm64/boot/dts/qcom/msm8996-oneplus3.dts`.  It declares model `OnePlus
3`, compatible values `oneplus,oneplus3` and `qcom,msm8996`, the OnePlus 3
board IDs, and includes `msm8996-oneplus-common.dtsi`.

The direct include chain is:

```text
msm8996-oneplus3.dts
  -> msm8996-oneplus-common.dtsi
       -> msm8996.dtsi
       -> pm8994.dtsi
       -> pmi8994.dtsi
       -> pmi8996.dtsi
```

`msm8996.dtsi` supplies the MSM8996 SoC description, including the UFS, USB,
display, GPU, RPM, pinctrl and related platform nodes.  The OnePlus-common
file supplies board-level PMIC, battery, audio, Bluetooth, touch and peripheral
configuration.  The `.dts` is listed as `msm8996-oneplus3.dtb` in the upstream
QCOM DTB Makefile, so an `ARCH=arm64` build with `CONFIG_ARCH_QCOM=y` selects
it through the normal upstream DTB rules.

## First-build exclusion

No file or patch from the legacy Linux 6.3.1 pmOS tree was read, copied, or
applied for this audit.  In particular, the first build must not carry any
legacy DTS overlay/change, GPU or DRM change, runtime-PM change, `no-preempt`
change, or diagnostic patch.  Any later comparison must be a separately scoped
experiment with owner-run build and device evidence.

## Script guard

`BASELINE.env` records the immutable v7.2 commit above.  The fetch script
checks that a fresh tag checkout resolves to it.  The build script rejects a
non-v7.2 version, a commit other than that pristine tag, or tracked source
modifications unless an explicitly authorized legacy task sets
`ALLOW_LEGACY_KERNEL=1`.

## Owner command (do not run as an agent)

After reviewing the source and ensuring `source/linux-7.2` is the pristine
commit above, run:

```bash
./scripts/build-kernel.sh
```

This command is intentionally not executed by this task.
