# OnePlus 3 v100-kernel / USB-ACM-initramfs cross A/B

Task / GitHub Issue: Owner-authorized early-boot observability A/B; no GitHub Issue supplied
Role: Implementation agent
Baseline commit: `d1cbb3d6c27e83eb32a9dfa0f822db5fc0e5f6c6`
Working branch: `agent/implementation/s6e3fa5-linux72-port`
Changed files: `docs/handoff/v100-kernel-usb-acm-initramfs-ab.md`
Commit SHA: pending

Layer: Boot / early boot observability
Hypothesis tested: The rootfs-free USB-ACM diagnostic initramfs can establish
a host-visible ACM console when paired with the known-good v100 kernel/DTB.
Only variable changed: initramfs changes from the black-screen-only minimal
PID 1 of OP3-BOOT-005 to the USB-ACM diagnostic initramfs. The v100
kernel+DTB payload, default boot profile, and UUID-free cmdline remain fixed.

## Inputs

- v100 gzip Image SHA256: `1dedadca810464f7c51e05544aefa87e455d5c0ba2ef6901a37f2356c09c6310`
- v100 raw OnePlus 3 DTB SHA256: `21667a04bcbb8be4e9935968abd869cd549d88606b745779cba868ebef422926`
- USB-ACM diagnostic initramfs SHA256: `c2c70a0637cb989b2766b2665601171dd5f334acfdaf12657d4c149873b8c0f1`
- Default UUID-free cmdline: `fbcon=nodefault console=tty0 pmos.debug-shell`

The diagnostic PID 1 does not mount a root filesystem or reuse any pmOS UUID.
It mounts configfs, creates a CDC ACM function, binds the first UDC, and sends
the diagnostic banner to `/dev/ttyGS0`.

Build run by project owner: NOT_RUN (no compilation is part of this task)
Build result: NOT_RUN
Artifacts and SHA256:

- `artifacts/boot-oneplus3-v100-kernel-usb-acm-initramfs.img`
- SHA256 `9da7e692af190eb31238d0c798a8f2c3d9f4f68199cb5b5612e6f4ef55295d55`
- Verified Android boot header v0, 4096-byte pages, kernel address
  `0x80008000`, ramdisk address `0x81000000`, tags address `0x80000100`,
  and the UUID-free cmdline.

Device test run by project owner: `fastboot boot` only; no flash
Device result: FAIL for the expected USB-ACM diagnostic observation. The phone
left fastboot but remained black-screened, and the owner observed no new
`/dev/ttyACM*` device or diagnostic banner.

Evidence: host `dmesg` pasted on 2026-08-27 is retained at
`/home/kai/.codex/attachments/712615e3-4a30-429f-9ab6-daaadaab914b/pasted-text.txt`.
It contains an earlier `1d6b:0104` CDC-ACM/RNDIS enumeration with no product
strings, consistent with the known full v100 userspace boot. The later test
events are repeated `18d1:d00d` Android fastboot enumerations; the log contains
no identifiable enumeration from this ACM-only diagnostic image. The earlier
ACM/RNDIS event must not be treated as evidence that this A/B passed.

Conclusion: FAIL for the expected host-visible ACM diagnostic endpoint; overall
kernel-entry/PID-1 state remains INCONCLUSIVE. A missing ACM endpoint may occur
before PID 1, in the diagnostic initramfs, or while binding the USB gadget/UDC;
it does not prove a Linux 7.2 DRM, GPU, PM, or panel failure.
Recommended next experiment: a new, initramfs-only v100-payload A/B that
reproduces v100's proven configfs shape (RNDIS plus ACM and its descriptors),
while still mounting no root filesystem and reusing no pmOS root UUID. Do not
change kernel, DTS, boot-header parameters, DRM/MSM, GPU, PM, or flash storage.
