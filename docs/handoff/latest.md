# Latest handoff

```text
PROJECT MODE: CLEAN REBUILD
TARGET KERNEL: Linux 7.2 pristine upstream
LEGACY KERNEL: Linux 6.3.1 pmOS-derived
DO NOT BUILD LEGACY unless explicitly requested
HOST TARGET: Ubuntu 26.04 LTS x86_64
DEVICE: OnePlus 3 / MSM8996
```

## Current state (2026-08-27) — ROOT CAUSE FOUND

**mainline v6.4 + strict v74 full config + ported S6E3FA5 panel boots**
(OP3-BOOT-028 PASS).

**Root cause of "all new kernels return to fastboot" = configuration
completeness + panel driver, NOT kernel early-boot code.**

- 6.3.1 boots because of the **complete v74 config** (1748 =y items) + the
  **S6E3FA5 panel driver**, not because of old-style `soc`.
- New kernels (6.4/6.8/6.12/6.16/7.2) failed with hand-made fragments because
  those fragments were incomplete (only ~20 QCOM items vs v74's 1748) and/or
  the S6E3FA5 panel driver was absent.

See `docs/handoff/root-cause-config-completeness.md`.

## Next action (7.2)

1. Configure 7.2 with the **strict v74 config** (cp v74-full.config +
   olddefconfig) — `source/linux-7.2` already has S6E3FA5 driver + panel DTS.
2. Provide `extfw/ath10k/QCA6174/hw3.0/` firmware in `source/linux-7.2`.
3. Compile, pack with the full initramfs, `fastboot boot`.

## Key artifacts

- 6.4 strict config: `kernel/configs/pmos631/v64-v74strict-full.config`
- 6.4 bootable image: `artifacts/boot-oneplus3-mainline-64-v74strict-fa5.img`
- Panel driver source: `source/linux-mainline-6.4/drivers/gpu/drm/panel/panel-samsung-s6e3fa5.c`
