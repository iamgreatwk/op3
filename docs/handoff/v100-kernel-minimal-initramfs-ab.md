# OnePlus 3 v100-kernel / minimal-initramfs cross A/B

Task / GitHub Issue: Owner-authorized boot/early-boot cross A/B; no GitHub Issue supplied
Role: Implementation agent
Baseline commit: `d9635c3b01b294051d168120993e8ec166733094`
Working branch: `agent/implementation/s6e3fa5-linux72-port`
Changed files: `docs/handoff/v100-kernel-minimal-initramfs-ab.md`
Commit SHA: pending

Layer: Boot / early boot
Hypothesis tested: The UUID-free minimal initramfs is accepted by the OnePlus 3 boot path when paired with the known-good v100 kernel payload. If the owner no longer observes an immediate fastboot return, the Linux 7.2 failure is attributable to the Linux 7.2 kernel/DTB payload rather than the minimal initramfs, rootfs UUID, or default boot profile.
Only variable changed: kernel payload (the gzip Image plus its appended raw DTB) changes from Linux 7.2 to the known-good v100 payload. The default profile, UUID-free minimal initramfs, and cmdline remain identical to OP3-BOOT-002.

## Inputs

- v100 gzip Image SHA256: `1dedadca810464f7c51e05544aefa87e455d5c0ba2ef6901a37f2356c09c6310`
- v100 raw OnePlus 3 DTB SHA256: `21667a04bcbb8be4e9935968abd869cd549d88606b745779cba868ebef422926`
- UUID-free minimal initramfs SHA256: `a107e55323dba324b28fa2185d1e92dcdb1ed666c94b49effafaa8feaa47763b`
- Default cmdline: `fbcon=nodefault console=tty0 pmos.debug-shell`

The v100 old pmOS root UUIDs are deliberately absent. The static `/init` does
not mount a root filesystem, so the test cannot rely on the v100 persistent
rootfs.

Build run by project owner: NOT_RUN (no compilation is part of this task)
Build result: NOT_RUN
Artifacts and SHA256:

- `artifacts/boot-oneplus3-v100-kernel-minimal-initramfs.img`
- SHA256 `5c3a7ec75f41c9ad76261c2cec09049698243362f12d7e4a24f8dad3fe73ced8`
- Verified Android boot header v0, 4096-byte pages, kernel address
  `0x80008000`, ramdisk address `0x81000000`, tags address `0x80000100`,
  and the UUID-free cmdline.

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: owner must record whether the device returns to
fastboot after `fastboot boot`.

Conclusion: INCONCLUSIVE pending owner-run device test
Uncertainties: With no UART or USB gadget evidence, a non-fastboot result only
establishes that the boot path remains alive; it does not prove minimal PID 1
execution.
Recommended next experiment: owner runs the generated artifact by `fastboot boot`
only. PASS is no immediate return to fastboot; FAIL is the same fastboot result
as OP3-BOOT-002.
