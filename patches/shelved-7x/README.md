# Shelved 7.x line — archived kernel patches (2026-08-30)

The Linux 7.x port line was shelved on 2026-08-30 (see
`docs/handoff/linux70rc1-minimal-ab.md`): the device returns to fastboot on
every 7.x boot and, without a physical UART, every attempt is blind. The
project baseline is the pmOS 6.12.1 LTS kernel.

The kernel source trees below were removed from `source/` (kept only:
`linux-mainline-6.12.1`, `linux-pmos-msm8996-6.12`, `linux-pmos-msm8996-6.3.1`).
Their unpushed local work is preserved here so nothing was lost.

## linux-7.2 (base v7.2; three local commits, none pushed anywhere)

| File | Content |
|---|---|
| `0001-dt-bindings-display-panel-add-Samsung-S6E3FA5.patch` | dt-binding for the OP3 panel |
| `0002-drm-panel-add-Samsung-S6E3FA5-panel-support.patch` | S6E3FA5 panel driver (from 6.3.1-v74full) |
| `0003-arm64-dts-qcom-msm8996-oneplus-enable-S6E3FA5-panel.patch` | DTS enable |
| `0004-arm64-dts-qcom-oneplus3-A-B-div1-clk-as-fixed-clock-.patch` | DSI div1-clk boot-fix A/B |
| `0005-Revert-arm64-dts-qcom-oneplus3-A-B-div1-clk-as-fixed.patch` | revert of the above (A/B result) |
| `0006-arm64-boot-omit-HCR_ATA-for-MSM8996-A-B.patch` | head.S HCR_ATA omit for ARMv8.0 (A/B, untested — line shelved before this build) |

## linux-mainline-6.16 (base v6.16; two local commits)

| File | Content |
|---|---|
| `0001-drm-panel-add-Samsung-S6E3FA5-support-for-v6.16-test.patch` | panel driver port to 6.16 |
| `0002-arm64-dts-qcom-add-S6E3FA5-panel-for-v6.16-test.patch` | DTS enable |

## linux-mainline-6.19.5 (base v6.19.5; one local commit)

| File | Content |
|---|---|
| `0001-drm-panel-add-Samsung-S6E3FA5-support-for-v6.19.5-te.patch` | panel driver port to 6.19.5 |

(Note: `linux-mainline-6.19.5` also carried an untracked `extfw/` directory;
not archived — it duplicated the firmware staging of the other trees.)

## pmOS trees (no local commits; uncommitted DTS tweaks only)

| File | Content |
|---|---|
| `linux-pmos-6.16-uncommitted.patch` | `msm8996-oneplus3.dts` worktree diff |
| `linux-pmos-6.19.5-uncommitted.patch` | `msm8996-oneplus3.dts` worktree diff |

The S6E3FA5 driver/bindings patches are the reusable asset here: if a newer
kernel line is ever picked up again (6.18 LTS or 7.x with a UART), they are
the starting point, together with the upstream DTS/fragment notes in the
handoff doc.
