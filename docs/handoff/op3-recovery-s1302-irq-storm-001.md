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

Build run by project owner: NOT_RUN
Build result: NOT_RUN
Artifacts and SHA256: Pending owner kernel, Buildroot initramfs, and boot-image
rebuild. The 32nd kernel patch is archived as
`patches/pmos612-op3-recovery-audio-full/0032-Input-misc-poll-OP3-S1302-when-IRQ-is-held-low.patch`.

Device test run by project owner: NOT_RUN for this commit
Device result: NOT_RUN
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
