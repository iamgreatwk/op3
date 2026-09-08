# Agent handoff: OP3 PSCI CPU idle

Task / GitHub Issue: OP3 recovery PSCI CPU idle enablement
Role: Implementation
Baseline commit: `47840ea74941fed8f5bf7c35d84b40eef8de2e3d` top-level project;
kernel tree `21a64a8c2a20` (`afa264a577f7f531159816f2d62c5173c87af735`)
Working branch: `agent/implementation/recovery-browser-001`
Changed files: `kernel/configs/oneplus3-recovery-audio-full.config`,
`manifests/op3-recovery-audio-full.env`
Commit SHA: pending

Layer: Kernel configuration / CPU idle
Hypothesis tested: The recovery image remains unnecessarily warm because the
kernel has CPU idle support and PSCI firmware, but the PSCI cpuidle driver is
disabled; enabling it will allow blocked recovery threads to enter the DT
described `standalone-power-collapse` state and reduce idle current.
Only variable changed: `CONFIG_ARM_PSCI_CPUIDLE=y`.

The MSM8996 device tree already describes `CPU_SLEEP_0` with PSCI entry
method and parameter `0x00000004`. The current kernel log reports PSCIv1.0,
but the running device reports no cpuidle driver. The PM8941 power-key driver
marks the key as wakeup-capable and enables its IRQ as a wake source during
system suspend.

Build run by project owner: NOT_RUN
Build result: NOT_RUN
Artifacts and SHA256: The existing integrated artifacts are not valid for
this changed kernel configuration. The manifest marks the new kernel config,
Image.gz, and boot image as `OWNER_BUILD_REQUIRED`; the existing initrd and
DTB remain fixed inputs.

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: Prior boot showed
`/sys/devices/system/cpu/cpuidle/current_driver` as `none` and
`CONFIG_ARM_PSCI_CPUIDLE` disabled.

Conclusion: INCONCLUSIVE
Uncertainties: Firmware previously logged `failed to set PC mode: -1`; the
driver may register but fail to enter the deepest state. This experiment does
not enable full system suspend and does not change DRM refresh or userspace
services.

Recommended next experiment: The owner builds and boots the new kernel with
the existing Buildroot initrd, then records `current_driver`, every CPU idle
state's `usage/time`, recovery CPU time, battery current, and thermal zones.
Press the power key after the device has entered idle and verify that the
input event wakes recovery and restores the display. Do not use
`echo mem > /sys/power/state` in this first test.
