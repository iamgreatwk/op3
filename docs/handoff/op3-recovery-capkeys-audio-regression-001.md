# OP3 recovery S1302 startup retry follow-up

```text
Task / GitHub Issue: OP3 recovery S1302 startup retry follow-up
Role: Implementation
Baseline commit: 67b0bbc3cbf46bae712a2606a43361756fcbd829
Recovery branch: agent/implementation/recovery-browser-001
Kernel worktree: source/linux-pmos-msm8996-6.12-recovery-audio-full
Kernel branch: agent/implementation/recovery-browser-audio-full-001
Kernel commits: c15090406a2a, 4a486e2ea7e4

Layer: formal pmOS 6.12 kernel S1302 input initialization
Hypothesis: In the audio-integrated image, S1302 can still be completing its
power/reset sequence when the I2C client is probed.  The first page-select
transaction then returns -ENXIO even though the same DTB device registration
worked in the standalone capkey image.
Only variable changed: bounded retry of the initial S1302 I2C key-state read.
Audio routing, DTS wiring, recovery userspace, and key mappings are unchanged.

Observed integrated-image result before this follow-up: FAIL for chin keys.
The DTB contains the 75b6000.i2c/capkey@20 node and the configured kernel
contains CONFIG_INPUT_OP3_S1302_CAPKEY=y, but dmesg reports
`op3-capkey-s1302 3-0020: error -ENXIO: failed to read initial key state`.
No `op3-capkey-s1302` input device is registered, so recovery cannot discover
or receive codes 580/158.  The same driver object and capkey DT properties
are present in the earlier standalone image that registered event1 and passed
physical left/right press and release testing.

Source change: after the existing reset-release delay, the probe retries only
transient `-ENXIO`, `-EREMOTEIO`, and `-EIO` results up to eight times with a
25 ms delay.  Runtime IRQ reads remain single-shot.  This is intended to
provide at most 200 ms of additional startup settling time without masking a
runtime bus error.

Owner build: PASS, 2026-09-06.  The project owner compiled the assigned
kernel worktree and booted the repacked recovery image.  The agent did not run
the kernel build or device boot.
Device registration: PASS, 2026-09-06.  The first S1302 read returned `-6`,
retry 1 succeeded, dmesg reported `S1302 capacitive keys ready (irq=88)`,
`/proc/bus/input/devices` contained `op3-capkey-s1302` on `/dev/input/event2`,
and recovery reported `cap=10`.
Physical key test: pending.  The supplied evidence does not yet contain
press/release events for codes 580 and 158.

Required post-build evidence:
  dmesg | grep -iE 's1302|capkey|i2c|gpio|irq'
  cat /proc/bus/input/devices
  grep -E 'capacitive-keys|recovery fds' /tmp/fb.log

Expected result: dmesg reports `S1302 capacitive keys ready`, an
`op3-capkey-s1302` event device exists, recovery opens `cap=...`, and physical
left/right press and release produce codes 580 and 158.  This follow-up is
not accepted until the owner reports the physical press/release result.
```
