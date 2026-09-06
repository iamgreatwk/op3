# pmOS 6.12 OnePlus 3 S1302 capacitive-key patch series

This series targets the formal pmOS MSM8996 Linux 6.12.1 baseline at
`67b0bbc3cbf46bae712a2606a43361756fcbd829`. It is prepared on the recovery
implementation branch because the formal kernel checkout is assigned to a
different agent; do not apply it directly to that agent's working tree.

Apply the patches in lexical order from a private/assigned formal 6.12 kernel
worktree. Do not run these commands from the recovery project root, and do not
use the kernel checkout assigned to another agent:

```sh
git am \
  /home/kai/src/oneplus3-mainline/patches/pmos612-op3-capkeys/0001-*.patch \
  /home/kai/src/oneplus3-mainline/patches/pmos612-op3-capkeys/0002-*.patch
```

The patches intentionally omit fabricated blob hashes, so use plain `git am`
instead of `git am --3way`. They are checked to apply cleanly to the formal
baseline; if the assigned kernel branch has overlapping changes, resolve those
in that kernel branch and keep the resulting commit separate from recovery.

Do not run `merge_config.sh` or `make olddefconfig` against the source-tree
`.config`. If no source-tree `.config` exists, Kconfig may silently fall back
to the host `/boot/config`, which is not an OP3 kernel configuration. Use an
external output directory and the previously validated OP3 6.12 configuration
as the base:

```sh
project=/home/kai/src/oneplus3-mainline
kernel=$project/source/linux-pmos-msm8996-6.12
output=$project/out/pmos-msm8996-6.12-capkey
base=$project/out/pmos-msm8996-6.12-rpm-glink-own-dtb/.config

test -r "$base"
mkdir -p "$output"
cp "$base" "$output/.config"

"$kernel/scripts/kconfig/merge_config.sh" -m -O "$output" \
  "$output/.config" \
  "$project/kernel/configs/oneplus3-s1302-capkey.fragment"

make -C "$kernel" O="$output" ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 \
  olddefconfig
```

The base configuration above has SHA256
`2ebb875b6ed1694e91f51d078f6cb8d17e7c8a23850bc0252d74526de01e45b3`.
The source-tree `.config` generated accidentally from the host configuration
should not be used for a device build; it can be removed after confirming it
contains no owner changes.

The hardware mapping comes from the historical OnePlus 3 DTS: S1302 is on
BLSP2 QUP2 (`blsp_i2c8`), IRQ is TLMM GPIO 132, reset is TLMM GPIO 76, and
the controller is powered by PMIC L13/S4. The driver reports the two physical
chin keys as `KEY_APPSELECT` (580, left/recent) and `KEY_BACK` (158, right).
The center fingerprint/home button is separate and is intentionally not
claimed by this driver.

This is source preparation only. No kernel build or device test was run by
the recovery agent. The owner must build and boot the exact patched kernel,
then verify the input inventory and key events before accepting the result.
