# Latest handoff

```text
PROJECT MODE: CLEAN REBUILD
TARGET KERNEL: Linux 7.2 pristine upstream
LEGACY KERNEL: Linux 6.3.1 pmOS-derived
DO NOT BUILD LEGACY unless explicitly requested
HOST TARGET: Ubuntu 26.04 LTS x86_64
DEVICE: OnePlus 3 / MSM8996
```

## Current state (2026-08-27)

Linux 7.2 (and all mainline/msm8996 6.4+ kernels) return to fastboot on the
OnePlus 3 with no early output (no ACM, empty ramoops, no UART). The project
owner runs all builds/flashes; this agent prepares source, config, scripts,
and records diagnostics.

### Established facts

1. **Packaging/boot profile/header are NOT the cause** (OP3-BOOT-003: the same
   packer boots the known-good v100 payload).
2. **panel compatible mismatch was 6.3.1's root cause** (`s6e3fa3`→`s6e3fa5`,
   OP3-BOOT-019/020). 7.2 already uses `s6e3fa5`, so 7.2 has a separate issue.
3. **Configuration is NOT the cause for 7.2**: VA_BITS=48, EFI off, full
   built-in MSM8996 drivers all verified (OP3-BOOT-021/022), still fastboot.
4. **soc@0 is NOT the cause**: mainline v6.4 (old-style `soc`) also fails
   (OP3-BOOT-027). The break is in mainline **v6.3 → v6.4**.

### Root-cause hypothesis (pending device confirmation)

`arch/arm64/mm/proc.S` `.idmap.text` section flags:
- 6.3.1 (boots): `"awx"` (writable + executable)
- 6.4+ / 7.2 (fail): `"a"` (read-only, non-executable) — security hardening in
  mainline v6.4.

Hypothesis: idmap code runs pre-MMU/pre-relocation and writes data inside
`.idmap.text`; the hardened `"a"` flags cause an early permission fault →
watchdog reset → fastboot. 6.3.1's `"awx"` avoids it.

See `docs/handoff/root-cause-idmap-text-section.md`.

## Next action (owner)

In `source/linux-7.2/arch/arm64/mm/proc.S`, change the 3
`.pushsection ".idmap.text", "a"` to `".idmap.text", "awx"` (matching 6.3.1),
rebuild Image.gz + DTB, pack with the full initramfs, `fastboot boot`. If it
leaves fastboot, the root cause is confirmed.
