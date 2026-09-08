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

Owner Buildroot build: NOT RUN. A small userspace-only candidate was built
with `scripts/build-recovery-mainline.sh`; its transient device-test SHA256 is
`de52b2e0e4b8b24a55348547e36c3a91160b64963f337099b32201c264519ae7`.
The previous on-device binary was backed up as
`/newroot/sbin/recovery_mainline.before-schedutil-20260908` with SHA256
`d92cd0d63e17f0b81eb66d06b003149ca6a14fce2c3d4c4d1e5280dae6d0a9c5`.

Device test for commit `4dff12f`: PASS for the scoped userspace replacement.
The new process was verified as `/newroot/sbin/recovery_mainline`. With
`/root/tri_mode=1`, both `policy0` and `policy2` reported
`scaling_governor=schedutil` while the screen was on. A physical power-key
cycle changed brightness `0 -> 102 -> 0`; the wake state restored `schedutil`
on both policies, and the screen-off state set both policies to the existing
`userspace`/`307200` kHz policy. Recovery logs retained power, volume,
three-position, capacitive-key, and touch input paths. Final Buildroot
integration and artifact replacement remain pending.

Artifact hashes: pending owner Buildroot build

Expected PASS condition: with `/root/tri_mode=1`, both policies report
`scaling_governor=schedutil` after boot and after a power-key wake; the
screen-off path still returns both clusters to the existing 307200 kHz
userspace policy; `performance` and `powersave` remain selectable; and the
recovery UI and physical-key paths remain functional.

No kernel, DTS, DRM, S1302 polling, or Buildroot configuration was changed by
this commit. The transient binary replacement is not a final artifact; after
the remaining test series, the owner must rebuild Buildroot and package the
same source change into the locked initramfs.
