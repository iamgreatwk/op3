# Agent handoff: S1302 capacitive-key IRQ storm

Task / GitHub Issue: OP3 S1302 low-level IRQ mitigation (follow-up)
Role: Implementation
Baseline commit: `4a486e2ea7e46622d68ae039a2ccf2db909ab99d` kernel worktree
Working branch: `agent/implementation/recovery-browser-audio-full-001`
Changed files: `drivers/input/misc/op3-capkey-s1302.c`,
`Documentation/devicetree/bindings/input/oneplus,s1302-capkey.yaml`,
`arch/arm64/boot/dts/qcom/msm8996-oneplus-common.dtsi`
Commit SHA: `21a64a8c2a20`

Layer: Linux input driver and OP3 device tree
Hypothesis tested: The S1302 GPIO132 line is held low by the controller
firmware, so a level-low IRQ is continuously re-triggered after each threaded
handler. Using the legacy driver's polling fallback avoids the IRQ storm while
preserving the same I2C key-state reads and EV_KEY mappings.
Only variable changed: OP3 S1302 event delivery, level-low IRQ -> 30 ms I2C
polling, selected by `polling-interval-ms = <30>`.

Build run by project owner: YES
Build result: PASS
Artifacts and SHA256:

- Kernel `Image.gz`: `5c89259d9340071c9c8684d361042482ca25c8f76b2de4d33cdacf2105f78861`
- Kernel DTB: `264f981678c1dd8d1d9a52f2db6e2130a0ebccbb9f4485ab8740784f73806db7`
- Buildroot initramfs: `27736d1d662158bedd5170c033f9b73d807d559a564d3fd6c9090438b6d0f968`
- Boot image: `fcd5e6bd476467e09abfbcbea3dcafd206b0ae98e60a17165a697fd87f4239c5`

The 32nd kernel patch is archived as
`patches/pmos612-op3-recovery-audio-full/0032-Input-misc-poll-OP3-S1302-when-IRQ-is-held-low.patch`.

Device test run by project owner: NOT_RUN for this commit; deferred while the
phone is charging
Device result: PENDING
Evidence links / log paths: Before this change, debugfs showed GPIO132 as
`in low`, while `/proc/interrupts` IRQ88 increased by 1,568 in 2 s (about 784
interrupts/s). The S1302 input device and physical key events were otherwise
working with the prior kernel.

Conclusion: INCONCLUSIVE
Uncertainties: This is a targeted mitigation for the observed firmware/line
state, not a proof of the undocumented S1302 interrupt-clear protocol. The
owner must confirm that no IRQ88 storm remains and that both chin keys still
produce press/release events at the expected 30 ms polling cadence.
Recommended next experiment: Build the locked kernel worktree and boot it;
check the S1302 probe log for `polling=30 ms`, sample IRQ88 before and after
10 s, and exercise both chin keys while watching recovery's input log.

## Follow-up: reduce S1302 polling to 100 ms

Date: 2026-09-08

The next isolated change is only the device-tree polling interval:
`polling-interval-ms = <30>` -> `<100>`. The S1302 driver, key mappings,
reset sequence, recovery userspace, and all other kernel settings are fixed.
The committed kernel is `4f8595b13fbd` with tree
`5633301bb0fa5f05254d0d48d0c55cfed81a40e2`; the durable archive is patch
`0033-arm64-dts-qcom-slow-OP3-S1302-polling.patch`.

Owner kernel build: NOT RUN
Device test for the 100 ms candidate: NOT RUN

Current-image reference observation before this change (30 ms polling,
screen off) showed CPU about 92% idle, recovery userspace at 0% CPU, GPU
`auto/suspended`, and 5.5 GB available memory. Linux IRQ87, the
`75b6000.i2c` / BLSP2 I2C2 controller used by S1302, increased from 110969 to
112146 during 10 seconds (about 118 controller IRQs/s). The touch controller's
I2C line did not increase during the same interval. A 30-second screen-on
sample was then obtained after the owner pressed the physical power key.
Backlight was `255`; CPU was about 90% idle at the first sample and 92% idle
at the second; `recovery_mainline` remained at 0% CPU; GPU remained
`control=auto` and `runtime_status=suspended`; and `cpu-sleep-0` accumulated
about 28.4 seconds during the interval. Linux IRQ87, the same BLSP2 I2C2
controller, increased from 180145 to 183773 (about 121 controller IRQs/s),
essentially the same as the screen-off rate. DSI IRQ increased only from 151
to 157. The screen-on sample did not expose a separate high-CPU or active-GPU
consumer; the S1302 I2C polling activity remains the main actionable wakeup
source.

Expected PASS condition after the owner build and boot: the probe log reports
`polling=100 ms`, the S1302 controller IRQ rate is lower than the current
reference, and both chin keys still generate complete press/release events.
The screen-on resource reference is recorded above; the 100 ms candidate
should be checked in both screen states if the owner wants to confirm that the
polling reduction is independent of display state.
