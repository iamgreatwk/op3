# OnePlus 3 Linux 7.0-rc1 blind boot A/B + 7.x line shelved

```text
Task / GitHub Issue: Owner-authorized 7.0-rc1 bisect midpoint boot test;
no GitHub Issue supplied
Role: Implementation agent
Working branch: agent/implementation/s6e3fa5-linux72-port
Changed files:
- kernel/configs/mainline-7.0/op3.fragment (commit acc29fd)
- docs/handoff/linux70rc1-minimal-ab.md
- docs/test-matrix.md (OP3-BOOT-040, OP3-BOOT-041)
- patches/shelved-7x/ (archived unpushed kernel-tree work)
Commit SHA: pending

Context: 7.2 fails even with the known-good v74 DTB (OP3-BOOT-038/039) while
pmOS 6.19.5 passes (OP3-BOOT-037). The regression window was narrowed with a
7.0-rc1 midpoint, borrowing the proven recipe of the external
gemini-mainline-linux project (Linux 7.0 boots on Mi 5, same MSM8996 SoC):
arm64 defconfig + gemini-derived fragment (commit acc29fd: verbatim gemini
fragment with the panel driver line swapped to S6E3FA5), EFI off, 48-bit VA,
DRM_MSM=m, built-in g_serial + U_SERIAL_CONSOLE, minimal initramfs, appended
DTB, ramdisk offset 0x04000000.

OP3-BOOT-040 — 7.0-rc1 + own upstream DTB (msm8996-oneplus3.dtb from
v7.0-rc1), cmdline `fbcon=nodefault console=tty0 console=ttyGS0`:
  image: artifacts/boot-oneplus3-linux70rc1-upstreamdtb-minimal.img
  sha256: 9dc6db22a80d42bf7c6436897f3d9693c3f5e03fbad8a1c8231b6a7e1f5373d8
  result: FAIL. Device returned to fastboot. No USB enumeration, no ACM log
  (watcher armed, /dev/ttyACM0 never appeared), no pstore. Kernel died before
  the DWC3 gadget came up.

Fragment cross-check against the gemini reference (owner-requested):
- `CONFIG_DRM_PANEL_SAMSUNG_S6E3FA5=y` is silently dropped by
  merge_config.sh: the symbol does not exist upstream (pmOS out-of-tree
  driver). gemini's Mi 5 panel (JDI R63452) is mainlined, OP3's is not.
  Consequence: even a successful 7.x boot would show nothing on the panel.
- The gemini blocks we omitted (PCI/WiFi, haptics/LEDs, Docker/netfilter/
  cgroups) are not boot-critical; the SMBCHG/FG charger lines are dead in
  torvalds 7.0 per gemini's own comment.
- Worth adopting from gemini's build-bootimg.sh: cmdline
  `console=ttyGS0,115200 console=tty0 ignore_loglevel loglevel=8 maxcpus=4`
  (maxcpus=4 is their big.LITTLE workaround).

DTB-append mechanism note (corroborated, not the cause): upstream arm64
removed the CONFIG_ARM_APPENDED_DTB kernel-side scan somewhere between 6.12
and 6.19 — but pmOS 6.12.1 and 6.19.5 trees have no APPENDED_DTB either and
still boot on this device, confirming OP3's LK itself extracts the appended
DTB and passes it in x0 (8996 dev_tree_appended). The compiled upstream and
v74 DTBs carry identical `qcom,msm-id <246 0x30001>` / `qcom,board-id
<8 0 15801 15/16>`, so LK DTB selection is ruled out as a differentiator.

OP3-BOOT-041 — 7.0-rc1 + v74 DTB (single-variable A/B vs OP3-BOOT-040),
cmdline `console=ttyGS0,115200 console=tty0 ignore_loglevel loglevel=8
maxcpus=4`:
  image: artifacts/boot-oneplus3-linux70rc1-v74dtb-minimal.img
  sha256: c8abd288d50a7593931897d2d62eeb57be8ca898ec0328bae44128ba18d7fc57
  result: FAIL. Same signature: fastboot return, no ACM, no pstore.

Conclusion: with config/DTB/initramfs/packing held at known-good values, the
failure follows the kernel source across 6.19.5 (PASS) -> 7.0-rc1 (FAIL).
The regression is inside the 6.19.5..7.0-rc1 merge window. A config-control
build (6.19.5 + the same gemini fragment) was prepared but NOT run: the owner
shelved the 7.x line before it could disambiguate config vs code.

LINE SHELVED (owner decision, 2026-08-30): every 7.x boot is blind — OP3's
LK produces no early-boot output without a physical UART (the Mi 5 project
has one; we do not). Continuing costs a ~14-round bisect with no diagnostics
beyond pass/fail. Resume criteria: physical MSM8996 UART trace available, or
an upstream analysis pinpointing the msm8996-early-boot change in that
window (head.S, clk-msm8996, smem, rpm-proc are the standing candidates from
the own-DTB failures).

Baseline decision (same date): the long-term project kernel is the pmOS
6.12.1 LTS line (LTS EOL 2028-12-31), already validated end-to-end through
the layer-07 browser gate. source/ was reduced to `linux-mainline-6.12.1`,
`linux-pmos-msm8996-6.12`, `linux-pmos-msm8996-6.3.1` (+ `buildroot`);
unpushed work from the removed trees is archived in `patches/shelved-7x/`.
```
