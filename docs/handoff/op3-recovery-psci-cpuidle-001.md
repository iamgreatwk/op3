# Agent handoff: OP3 PSCI CPU idle

Task / GitHub Issue: OP3 recovery PSCI CPU idle enablement
Role: Implementation
Baseline commit: `47840ea74941fed8f5bf7c35d84b40eef8de2e3d` top-level project;
kernel tree `21a64a8c2a20` (`afa264a577f7f531159816f2d62c5173c87af735`)
Working branch: `agent/implementation/recovery-browser-001`
Changed files: `kernel/configs/oneplus3-recovery-audio-full.config`,
`manifests/op3-recovery-audio-full.env`
Commit SHA: `54a3a42`

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

Build run by project owner: YES
Build result: PASS
Artifacts and SHA256:

- Kernel config: `6f8efe25de1c7f64af15002f46e180b8cc0a8c214508e880d5008180bc1c23a9`
- Kernel `Image.gz`: `edb7c939018a4ddfaa816adb34971eb57a23c09801e4b04a6e20438bad7eef42`
- Kernel DTB: `87aff2df7ef853966f88f9bb07488cffb3315dfe407bef801032deb319ff88bd`
- Buildroot initrd (fixed input): `27736d1d662158bedd5170c033f9b73d807d559a564d3fd6c9090438b6d0f968`
- Boot image: `10e34f455707bac3060f5d58edc6d589c8ba6e7b5578a896350326782a93bbaa`

The DTB differs from the previous integrated artifact only by the expected
`polling-interval-ms = <30>` property from the locked S1302 polling patch.
The previous DTB hash in the manifest was stale and has been corrected.

Device test run by project owner: YES
Device result: PASS for CPU-idle registration and power-key wake path;
thermal comparison INCONCLUSIVE
Evidence links / log paths: The owner booted the new image and the device
reported Linux `6.12.1-msm8996+ #1 SMP PREEMPT Tue Sep 8 19:21:12 CST 2026`.
`current_driver=psci_idle` and `current_governor=menu` were present. Over a
15-second idle sample, `cpu-sleep-0` increased from usage `18751` / time
`56772772` to usage `23147` / time `70774588`, showing about 14 seconds in
the deep CPU idle state. The PM8941 power-key node reported
`power/wakeup=enabled`. The GPU remained `auto/suspended`.

The owner then confirmed a physical power-key press to turn the display off
and a second press to wake it. The recovery log contains multiple complete
`KEY_POWER` press/release pairs, and the final backlight value is `255`.

A follow-up read-only sample was run with the phone connected to a wall
charger and SSH kept over Wi-Fi. Before the 30-second screen-off sample,
`bq27541-0` reported `Charging`, capacity `78`, temperature `353` (35.3 C),
current `1252000` uA; the USB supply reported `Charging`, `Fast`, online, with
an input limit of `3000000` uA. The GPU remained `control=auto` and
`runtime_status=suspended`, and the display backlight was `0`. After 30
seconds, the phone still reported `Charging`/`Fast`, capacity `78`, battery
temperature `352` (35.2 C), current `1246000` uA, and the same 3 A input
limit. Thermal zones 0/5 changed from 43.8/45.1 C to 43.8/45.4 C. The
`cpu-sleep-0` counter increased by `4506` uses and `29019190` us, about 29.0
seconds of deep idle during the 30-second interval. Recovery PID `388`
remained alive.

This is a valid charging-condition sample, but it is not yet a same-condition
A/B against the previous kernel. The earlier USB-host sample was invalid for
thermal comparison because it was limited to 500 mA and the battery was
discharging.

Conclusion: INCONCLUSIVE
Uncertainties: Firmware previously logged `failed to set PC mode: -1`; the
driver may register but fail to enter the deepest state. This experiment does
not enable full system suspend and does not change DRM refresh or userspace
services.

Recommended next experiment: repeat the same 30-second or longer screen-off
wall-charger sample on the previous integrated kernel, keeping the charger,
Wi-Fi SSH session, brightness, and userspace state fixed. Do not use
`echo mem > /sys/power/state` in this first A/B test.
