# pmOS 6.12 OnePlus 3 S1302 capacitive-key patch series

This series targets the formal pmOS MSM8996 Linux 6.12.1 baseline at
`67b0bbc3cbf46bae712a2606a43361756fcbd829`. It is prepared on the recovery
implementation branch because the formal kernel checkout is assigned to a
different agent; do not apply it directly to that agent's working tree.

Apply the patches in lexical order in a private kernel worktree:

```sh
git am --3way patches/pmos612-op3-capkeys/0001-*.patch \
  patches/pmos612-op3-capkeys/0002-*.patch
```

Merge the configuration fragment into the owner's existing 6.12 config:

```sh
scripts/kconfig/merge_config.sh -m .config \
  /home/kai/src/oneplus3-mainline/kernel/configs/oneplus3-s1302-capkey.fragment
make olddefconfig
```

The hardware mapping comes from the historical OnePlus 3 DTS: S1302 is on
BLSP2 QUP2 (`blsp_i2c8`), IRQ is TLMM GPIO 132, reset is TLMM GPIO 76, and
the controller is powered by PMIC L13/S4. The driver reports the two physical
chin keys as `KEY_APPSELECT` (580, left/recent) and `KEY_BACK` (158, right).
The center fingerprint/home button is separate and is intentionally not
claimed by this driver.

This is source preparation only. No kernel build or device test was run by
the recovery agent. The owner must build and boot the exact patched kernel,
then verify the input inventory and key events before accepting the result.
