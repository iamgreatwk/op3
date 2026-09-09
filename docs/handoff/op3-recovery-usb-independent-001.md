# OP3 recovery must not depend on a USB host — A/B candidate

```text
Task / GitHub Issue: user-reported fastboot-flash boot bug; Issue URL pending
Role: Implementation
Baseline commit: 24f369b1e14f4bb7cac32753687cdaffca42965
Working branch: agent/implementation/recovery-browser-001
Changed files: boot/initramfs/sbin/init_mainline.sh;
  docs/handoff/latest.md; docs/handoff/op3-recovery-usb-independent-001.md
Commit SHA: e3e9edd

Layer: 02 recovery boot userspace / initramfs integration
Hypothesis tested: optional USB gadget/ACM initialization in the synchronous
  BusyBox sysinit path can prevent recovery from being launched when no USB
  host is attached. Moving that optional path to the background makes recovery
  independent of USB VBUS/extcon/UDC/ttyGS0 state.
Only variable changed: whether optional USB debug initialization gates the
  recovery launch. Kernel, DTS, DRM/GPU, rootfs contents, Wi-Fi, audio,
  recovery binary, and boot cmdline are unchanged.

Build run by project owner: NOT_RUN
Build result: source static check PASS; Buildroot rebuild NOT_RUN; transient
  initramfs repack and boot-image packaging PASS
Artifacts and SHA256:
  `artifacts/initrd-op3-recovery-buildroot-usb-independent-ab-20260909.cpio.gz`
  `123bfec8cb818d69aa689ef86c0edf2c7a4cf4d98de07fcf3cd951f677344837`;
  `artifacts/boot-oneplus3-pmos612-recovery-buildroot-usb-independent-ab-20260909.img`
  `9ed9caf7efc083494c37865bf80a064b59ebcf5e8ea80a3c720f669fd9f6f267`.
  The initramfs was derived from the existing Buildroot rootfs and only its
  `sbin/init_mainline.sh` was replaced; it is an A/B test artifact, not yet a
  Buildroot reproducibility artifact.

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: source inspection found USB setup, ttyGS0 wait,
  and ACM shell startup in init_mainline.sh before the inittab recovery
  respawn. A no-USB boot log is still required to distinguish a USB-stage
  stall from a bootloader/power or display failure.

Conclusion: INCONCLUSIVE
Uncertainties:
  - The previous script had a finite ten-second ttyGS0 wait, so a short delay
    alone would not explain a permanent blank screen.
  - DWC3/extcon behavior without VBUS must still be observed on the device.
  - This candidate intentionally keeps USB/RNDIS/ACM available as an optional
    service; it does not change USB role policy or kernel configuration.
Recommended next experiment: owner boots the transient image above with no
  computer USB host attached, then repeats with the host attached. If the
  candidate passes, rebuild the same change through Buildroot before making it
  the locked artifact. Capture /root/boot_mainline.log,
  /tmp/op3-recovery.log, /root/boot_status.txt, and filtered DWC3/FUSB301/UDC
  dmesg. The PASS condition is a visible recovery UI with no USB host attached
  and no regression of RNDIS/ACM when a host is later connected.
```

Implementation note: `init_mainline.sh` now launches `setup_usb_debug` in the
background and no longer forwards its boot-critical `log()` calls directly to
`/dev/ttyGS0`. The parent sysinit can therefore return to BusyBox init and
start `run_recovery.sh` even if gadget binding or ACM registration is delayed,
unsupported, or stuck.
