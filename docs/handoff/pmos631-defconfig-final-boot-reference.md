# Project pmOS MSM8996 v6.3.1 defconfig reference result

```text
Task / GitHub Issue: Owner-authorized project-tree v6.3.1 comparison build;
no GitHub Issue supplied
Role: Implementation agent
Baseline: Linux 7.2 upstream remains the formal target; this is legacy
reference evidence only.
Working branch: agent/implementation/s6e3fa5-linux72-port
Changed files:
- docs/test-matrix.md
- docs/handoff/pmos631-defconfig-final-boot-reference.md
Commit SHA: 4d94dae621fa153040d37caacbb43c0f9dc5b3ab

Reference source:
- source/linux-pmos-msm8996-6.3.1
- commit: 9895e7e38b829a810b9f75d1f98c9e4349ae454a

Build method: defconfig plus the committed oneplus3-s6e3fa5.fragment; final
FA5 DTB and complete final initramfs; UUID-free debug-shell command line.
Artifact:
- artifacts/boot-oneplus3-pmos631-fa5-final-companions.img
- SHA256: 4b1a8eea22bb974865b63f3d6286aa6437dab9365dc9dd3f10c64baa765112d2
- Android boot v0; page size 4096; kernel 0x80008000; ramdisk 0x81000000;
  tags 0x80000100

Build run by project owner: PASS
Device test run by project owner: FAIL. Owner reports return/remain in
fastboot; no Linux/display PASS is claimed.

Critical validity finding:
- The newly built kernel release is `6.3.1-g9895e7e38b82`.
- The final initramfs contains modules under `usr/lib/modules/6.3.1-msm8996`.
- Therefore that initramfs cannot supply modules to this new kernel.
- Unlike the known-good 6.3.1 configuration, the defconfig test has
  SCSI_UFS_QCOM=m, PHY_QCOM_QMP_UFS=m, USB gadget/configfs components =m,
  QCOM remoteproc components =m, and no pstore RAM/console. The known-good
  build configures the cited UFS/PHY/USB/remoteproc components built-in.
- The defconfig test also lacks MFD_QCOM_RPM, REGULATOR_QCOM_RPM, and
  QCOM_CLK_RPM that are enabled in the known-good configuration.

Conclusion: REJECTED as a kernel-code comparison. This result does not show
that project-tree 6.3.1 source fails with a known-good 6.3.1 startup
configuration; it shows that defconfig plus only the FA5 display fragment is
not a complete pmOS MSM8996 boot configuration.
Recommended next experiment: source audit only. Derive a narrowly justified,
non-GPU boot-critical configuration set from the known-good 6.3.1 config;
do not copy the legacy A5XX no-preempt or any prohibited workaround into Linux
7.2.
```
