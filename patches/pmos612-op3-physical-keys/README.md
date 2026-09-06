# pmOS 6.12 OnePlus 3 volume and tri-state key patch

This patch targets the formal pmOS MSM8996 Linux 6.12.1 baseline at
`67b0bbc3cbf46bae712a2606a43361756fcbd829`.  It is source preparation on the
recovery implementation branch; apply it in the assigned/private formal
kernel worktree, not in this recovery checkout and not in another agent's
kernel worktree.

Apply it from the kernel worktree:

```sh
git am \
  /home/kai/src/oneplus3-mainline/patches/pmos612-op3-physical-keys/0001-*.patch
```

The formal OP3 6.12 base configuration already contains
`CONFIG_KEYBOARD_GPIO=y`.  Reuse the validated OP3 kernel configuration used
for the S1302 capkey build; no new config fragment is required for this
patch.

The retained historical mapping is:

| Device | PMIC GPIO | EV_KEY |
| --- | ---: | ---: |
| Volume up | 3, active-low | 115 |
| Volume down | 2, active-low | 114 |
| Tri-state top/performance | 6, active-low | 600 |
| Tri-state middle/balanced | 4, active-low | 601 |
| Tri-state bottom/powersave | 5, active-low | 602 |

The three tri-state GPIOs use PMIC input pull-ups.  The standard `gpio-keys`
driver emits each selected position as a key press; recovery already maps
115/114 to cursor up/down and 600/601/602 to the three performance modes.

This patch must be tested independently from the haptics change.  The owner
should build and boot the resulting kernel/DTB, then verify that
`/proc/bus/input/devices` contains a `gpio-keys` device and that recovery
reports `volume` and `tri-state` descriptors.  Physical testing must record
both press and release events for volume up/down and all three switch
positions.
