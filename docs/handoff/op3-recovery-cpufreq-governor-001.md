# Agent handoff: recovery balanced CPU governor

Task / GitHub Issue: OP3 recovery three-position CPU policy follow-up
Role: Implementation
Baseline commit: `851a74237e959e863883c38c1f5ffba236c2762d` top-level project
Working branch: `agent/implementation/recovery-browser-001`
Changed file: `recovery/recovery_mainline.c`
Commit SHA: `4dff12f`

Layer: Recovery userspace CPU-frequency policy
Hypothesis tested: the balanced three-position mode is not restored after a
screen wake because it requests the unsupported `interactive` governor. The
device exposes `schedutil`, but not `interactive`.
Only variable changed: balanced-mode governor, `interactive` -> `schedutil`.

Pre-change device evidence: `/root/tri_mode=1`; both cpufreq policies reported
`scaling_governor=userspace`, with current frequency and minimum frequency at
307200 kHz after the screen had been woken. The available governors were
`conservative ondemand userspace powersave performance schedutil`. The source
silently ignored the failed write to `scaling_governor`, so the unsupported
balanced mode left the wake path in the screen-off userspace policy.

Owner Buildroot/recovery build: NOT RUN
Device test for commit `4dff12f`: NOT RUN
Artifact hashes: pending owner build

Expected PASS condition: with `/root/tri_mode=1`, both policies report
`scaling_governor=schedutil` after boot and after a power-key wake; the
screen-off path still returns both clusters to the existing 307200 kHz
userspace policy; `performance` and `powersave` remain selectable; and the
recovery UI and physical-key paths remain functional.

No kernel, DTS, DRM, S1302 polling, or Buildroot configuration was changed by
this commit. The owner must rebuild the recovery userspace/Buildroot artifact
before device testing.
