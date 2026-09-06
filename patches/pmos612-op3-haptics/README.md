# pmOS 6.12 OnePlus 3 haptics patch series

This series targets the formal pmOS MSM8996 Linux 6.12.1 baseline at
`67b0bbc3cbf46bae712a2606a43361756fcbd829`.  It is source preparation on the
recovery implementation branch; apply it in the assigned/private formal
kernel worktree, not in this recovery checkout and not in another agent's
kernel worktree.

Apply both patches from the kernel worktree:

```sh
git am \
  /home/kai/src/oneplus3-mainline/patches/pmos612-op3-haptics/0001-*.patch \
  /home/kai/src/oneplus3-mainline/patches/pmos612-op3-haptics/0002-*.patch
```

The validated OP3 6.12 configuration already contains
`CONFIG_INPUT_QCOM_SPMI_HAPTICS=y`; this series does not change Kconfig.
Patch 1 allows the mainline driver to accept the ERM actuator type. Patch 2
enables the existing `pmi8994_haptics` SPMI peripheral and supplies a 5 ms
wave rate. The current driver handles the LRA-only auto-resonance path by
skipping it for ERM.

Historical OnePlus 3 DTS identifies the motor as an ERM at approximately
2700 mV. The current mainline driver does not implement the old
`qcom,vmax-mv` property, so this first experiment deliberately leaves the
driver's FF magnitude-to-vmax policy unchanged. If the device registers but
does not vibrate, the next experiment must isolate voltage/current policy.

This series must be tested independently from the physical-key DTS patch.
The expected kernel evidence is an input device named `spmi_haptics` and two
SPMI haptics IRQs. Recovery should then log `vibration: input FF -> ...` and
produce its short startup vibration. A registration or electrical failure
must be recorded separately from a recovery userspace failure.
