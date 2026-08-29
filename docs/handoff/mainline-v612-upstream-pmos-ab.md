# 主线 v6.12 / pmOS v6.12 启动 A/B 记录

```text
Task / GitHub Issue: Linux 7.2 bring-up diagnostic; upstream v6.12 control test
Role: implementation
Baseline commit: Linux v7.2 pristine upstream (formal target; no v7.2 source changed here)
Working branch: agent/implementation/s6e3fa5-linux72-port
Changed files: this handoff only
Commit SHA: this documentation-only checkpoint (see Git history)

Layer: kernel early boot / source-tree A/B
Hypothesis tested: pure upstream v6.12.x can boot OnePlus 3 when supplied the
same known-good FA5 DTB, complete initramfs, and cmdline as pmOS v6.12.
Only variable changed: kernel Image.gz source tree (with follow-up resolved-
config A/B as documented below).

Build run by project owner: PASS (all listed Image.gz builds completed)
Build result: PASS
Artifacts and SHA256: listed below

Device test run by project owner: PASS / FAIL as listed below
Device result: mixed; pure upstream v6.12/v6.12.1 failed black-screen while
pmOS v6.12 controls booted.
Evidence links / log paths: owner visual reports; no UART/USB COM log was
available for these tests.

Conclusion: SUPPORTED — the boot delta remains after fixing boot format, DTB,
complete initramfs, UUID cmdline variants, EFI state, and effective config.
Uncertainties: the required pmOS downstream code delta has not yet been
audited or forward-ported; no kernel console log is available.
Recommended next experiment: read-only audit pmOS MSM8996 v6.12.1 downstream
commits against official stable v6.12.1, then test one minimal source delta at
a time on a fresh upstream test tree.
```

## Fixed test inputs

- Device: OnePlus 3 / MSM8996; owner has confirmed the physical panel is
  Samsung S6E3FA5.
- Known-good legacy reference DTB:
  `out/pmos-msm8996-6.3.1-v74full/arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb`
  — SHA256 `463b2c7203e28359ce4039c8e4aa9b0d211171879db8bbea07834d3cb8b2bde3`.
  It contains `samsung,s6e3fa5`.
- Complete reference initramfs: `artifacts/reference-initrd.img` — SHA256
  `c3358a1cadb747996ddaa492e636827f2d72974040e8fd40d81f8a213e676366`
  (50,705,116 bytes).
- Standard test cmdline:
  `fbcon=nodefault console=tty0 pmos.debug-shell`.
- The v100 reference cmdline additionally supplies `pmos_boot_uuid`,
  `pmos_root_uuid`, and `pmos_rootfsopts=defaults`. These legacy UUIDs were
  used only for the diagnostic command-line variant below; they must not be
  reused for the formal Linux 7.2/new-rootfs outcome.

## Source trees and minimal FA5 test commits

| Tree | Upstream base | Local FA5 commits |
| --- | --- | --- |
| pure mainline v6.12 | `adc218676eef25575469234709c2d87185ca223a` | `f57575de9` driver; `3e54b5825` DTS |
| official stable v6.12.1 | `d390303b28dabbb91b2d32016a4f72da478733b9` | `34f89e63a` driver; `41e7062f6` DTS |
| pmOS MSM8996 v6.12.1 | `67b0bbc3cbf46bae712a2606a43361756fcbd829` | no source patch in A/B clean clone |

The FA5 test additions were limited to the panel driver, panel Kconfig and
Makefile entries, and the OnePlus common DSI panel node. No DRM/MSM, GPU, PM,
SMMU, clock, or legacy workaround was imported.

## Effective configuration A/B

The pure v6.12 resolved `.config` was copied into a clean pmOS v6.12.1 tree
and normalized with `olddefconfig`. Both resulting configs contained exactly
1,848 `CONFIG_*=y` settings. The only content differences were pmOS-only
symbols represented as disabled in the pmOS tree (for example unavailable
charger/panel/input options) and the kernel version text. In both cases:

```text
# CONFIG_EFI is not set
CONFIG_EFI_PARTITION=y
CONFIG_DRM=y
CONFIG_DRM_MSM=y
CONFIG_BACKLIGHT_CLASS_DEVICE=y
CONFIG_DRM_PANEL_SAMSUNG_S6E3FA5=y
```

The pmOS v6.12.1 kernel built using this pure-mainline resolved config booted.
That rejects effective Kconfig selection as the explanation for the pure
upstream black screen.

## Owner device results

| Test image | Result | SHA256 |
| --- | --- | --- |
| pure mainline v6.12 + v74 FA5 DTB + full initramfs | black screen | `09b8e733b8b6b9ef2ae11dc811c676854dcf28dd309b11cbcd55f12af05cbbfd` |
| pmOS v6.12 Image + same v74 FA5 DTB + full initramfs | booted | `8ba6a368b43be664df3c2c8d79cd968ffcf9cbae8c33639ebb8cc14dfa705d9a` |
| pmOS v6.12 Image + pure upstream resolved config + same DTB/initramfs | booted | `7cda0cec96520a32d94f0528504f767a195cf25a3936cbb654ee10e72f820b10` |
| official stable v6.12.1 + v74 FA5 DTB + full initramfs | black screen | `c045edc90c8bdf99b8ce24d71e047c0cf7836d8d24bde42563f576aedc6dd930` |
| v6.12.1 + full v100 UUID cmdline | black screen | `7df54ff772812113759f8364b4bc18db692c4c6bce3813d5ef20a8483cac1eec` |
| v6.12.1 + full v100 UUID cmdline without `pmos.debug-shell` | black screen | `1cd87a1cb62f56901de986a50458d786a46990460bdc723c5e08ea3ede113132` |

The owner also reports that pmOS MSM8996 v6.16 and v6.19 boot successfully.

## Resulting boundary

With v6.12/v6.12.1 upstream controls failing and pmOS v6.12.1 passing under
the same boot image profile, v74 FA5 DTB, complete initramfs, and effective
mainline config, the remaining explanatory variable is the pmOS MSM8996
downstream kernel source delta. Do not continue changing UUIDs, debug-shell
flags, EFI settings, or boot-image offsets as a substitute for that source
audit.

## Follow-up source audit — identified display delta

The pmOS v6.12.1 branch is directly based on official stable
`v6.12.1` (`d390303b28da`). Its post-base commit list contains a single
OnePlus-relevant DRM/MSM DSI lifecycle change:

- `51f8b001ac1b850f892f965020ca86859c409e4d`
  `drm/msm/dsi: improve power management` (parent: `d390303b28da`).

It changes four files exclusively under `drivers/gpu/drm/msm/dsi/`:

| File | Scope |
| --- | --- |
| `dsi.c`, `dsi.h` | add a PHY-power mutex and expose DSI manager power helpers |
| `dsi_host.c` | use runtime-PM autosuspend and power the manager before command transfers |
| `dsi_manager.c` | move PHY/host/IRQ sequencing into runtime-PM-aware manager helpers |

This is technically relevant to FA5: panel `prepare()` immediately transmits
DSI DCS commands (`exit_sleep_mode`, tear-on, vendor writes, display-on). The
pmOS patch makes those transfers resume the DSI device and power the
PHY/host first. In the unpatched path, transfer preparation only takes runtime
PM and assumes the previous bridge power state is already valid. A failure at
that point produces a black panel while the kernel may otherwise continue
booting; no COM log was available to distinguish this visually.

The pmOS FA5 panel driver has one additional line relative to the v6.12 test
driver:

```c
ctx->panel.prepare_prev_first = true;
```

This requests that the DSI controller first reach LP-11 before panel power-up.
It is a relevant explanation for the *v6.12 test driver* failure. However,
the formal Linux 7.2 FA5 driver already sets this flag, so it is not the
remaining Linux 7.2 delta.

Linux 7.2 still has the old DSI manager flow (`dsi_mgr_bridge_power_on()` and
`pm_runtime_get_sync()` in transfer preparation) and does not contain the
pmOS `dsi_mgr_power_on()` / `dsi_mgr_power_off()` implementation. Therefore
`51f8b001ac1b` is the current highest-confidence missing source delta for the
Linux 7.2 black-screen path.

No source port was attempted: the candidate requires changes in
`drivers/gpu/drm/msm/dsi/`, an area explicitly outside the panel-only task
scope. It must be separately authorized and forward-ported semantically, not
blindly copied from pmOS.

## Authorized v6.12.1 DSI test checkpoint

The owner subsequently authorized a standalone DSI-only control test on
official stable v6.12.1. The test branch is
`agent/implementation/mainline-v6121-dsi-pm-ab` in
`source/linux-mainline-6.12.1`; its tip is
`548b0dc49481bf0c4d6fe63cec76b0e516ec3f91`:

```text
drm/msm/dsi: apply pmOS runtime PM sequencing for v6.12.1 test
```

It carries only the four files from pmOS commit `51f8b001ac1b` applied against
its exact upstream parent (`v6.12.1`), in addition to the already committed
FA5 driver/DTS test support. No GPU, SMMU, regulator framework, clock
framework, DTS, cmdline, or initramfs change is included in this checkpoint.

At this handoff update: **NOT_BUILT, NOT_PACKED, NOT_DEVICE_TESTED**. The
owner build must use the existing v6.12 resolved config, then pack the new
`Image.gz` with the fixed v74 FA5 DTB and complete reference initramfs. The
sole test hypothesis is that DSI runtime-PM/PHY/host sequencing changes the
pure-upstream v6.12.1 black-screen result to a visible boot.

### Result (owner report)

The owner built and packed this exact branch, then reported successful boot to
the `recovery.c` program. This is a **PASS** for the DSI sequencing hypothesis:
the prior pure-upstream v6.12.1 image with all the same control inputs was
black, while only the four DSI files changed here reach recovery.

| Item | Value |
| --- | --- |
| `Image.gz` SHA256 | `ecd6698bd27b8ae3327566f463658be94e05831c706dd057fb99a0ee0272fe71` |
| boot image | `artifacts/boot-oneplus3-mainline-6121-dsi-pm-v74dtb-full-initrd.img` |
| boot image SHA256 | `87bcc4ba5fe76768d8a0b310d68ecde07c2aa2ee0f63fde4c79ebc6abbf65f19` |
| header / cmdline | v0, 4096-byte pages; `fbcon=nodefault console=tty0 pmos.debug-shell` |
| device result | PASS — owner reached `recovery.c` |

This validates the v6.12.1 control experiment only. It does **not** by itself
accept a Linux 7.2 port: the same intent must be forward-ported against Linux
7.2 DSI APIs in a separate reviewed change, then independently built and
tested.
