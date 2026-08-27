# Linux 7.2 early-boot / known-display legacy source audit

```text
Task / GitHub Issue: Owner-authorized read-only comparison of known-display
linux-fa5 with the existing 6.3.1 legacy evidence; no GitHub Issue supplied
Role: Implementation agent
Baseline commit: d73a640c6af7ad461bce8f54967c0abdf44d1204 (Linux v7.2-3-gd73a640c6)
Working branch: agent/implementation/s6e3fa5-linux72-port
Changed files: docs/handoff/linux72-early-boot-legacy-audit.md
Commit SHA: 4b4941ece579a692b72f2084fdf7d29e27aade8d

Layer: Source audit only; no Linux 7.2 source, DTS, configuration, build, or
device action was changed.

Reference trees:
- Existing legacy evidence:
  source/linux-pmos-msm8996-6.3.1
- Known-display legacy working tree:
  /home/kai/下载/home-sqwan-20260826/sqwan/linux-fa5
- Both resolve to Git commit
  9895e7e38b829a810b9f75d1f98c9e4349ae454a from
  https://gitlab.com/msm8996-mainline/linux.git, Linux 6.3.1.
- linux-fa5 is dirty. Its existing built vmlinux identifies itself as
  Linux 6.3.1-msm8996+ #15, built 2026-08-20 with GCC 11.4.0.

Additional newer pmOS/MSM8996 reference (2026-08-27):
- Remote discovery shows the highest `*-msm8996` tag as `v6.19.5-msm8996`.
- A depth-1, read-only reference checkout is available at
  source/linux-pmos-msm8996-v6.19.5, commit
  `1aed438cb5f49d7a61593f764d4a82f83b14114f` (2025-03-07), with Makefile
  version Linux 6.19.5.
- The default remote branch `msm8996-staging` is currently Linux 6.16-rc2;
  it is not the highest version and is not the selected comparison reference.
- The v6.19.5 tree contains the S6E3FA5 panel driver, but its standard
  msm8996-oneplus3.dts still selects `samsung,s6e3fa3`. It therefore does
  not replace the device-specific FA5 compatible delta already applied to
  Linux 7.2. Its S6E3FA5 driver differs from Linux 7.2 and must be audited
  before any code is considered; no code was copied.

Display-relevant result:
- panel-samsung-s6e3fa5.c is byte-identical in the two 6.3.1 trees
  (SHA256 b3cd1eb1d07911793a245c8a5973ca5b38e3d30729d80ade38ef1743806c17e2).
- The only dirty display-relevant DTS delta in linux-fa5 is in
  arch/arm64/boot/dts/qcom/msm8996-oneplus3.dts:
  `compatible = "samsung,s6e3fa3"` changes to `"samsung,s6e3fa5"`.
- Linux 7.2 already has the S6E3FA5 driver, Kconfig/Makefile selection, and
  OnePlus 3 panel compatible from the completed minimal port. Therefore this
  known-display tree supplies no additional panel-driver code to copy.

Other dirty linux-fa5 changes:
- msm8996-oneplus-common.dtsi changes are codec MCLK, amplifier supply,
  audio routes/DAIs, MI2S pins, and an amplifier reset GPIO.
- The only changed C files are sound/soc/codecs/wcd9335.c,
  sound/soc/qcom/apq8096.c, sound/soc/qcom/qdsp6/q6asm-dai.c, and
  sound/soc/qcom/qdsp6/q6asm.c.
- They are not legitimate candidates for an early fastboot-return fix and
  must not be imported into Linux 7.2 for this task.

Early-boot configuration differences worth auditing, not importing:
- Both configurations build ARM64, ARCH_QCOM, 4K pages, initrd gzip support,
  DRM/MSM, S6E3FA5, pstore RAM/console, and CONFIG_SERIAL_MSM built-in.
- Known-display 6.3.1 uses ARM64_VA_BITS=48 and QCOM_RPROC_COMMON=y.
- current Linux 7.2 pstore build uses ARM64_VA_BITS=52 and
  QCOM_RPROC_COMMON=m.
- These are hypotheses only; no causal evidence supports changing any of
  them yet. QCOM_WDT is explicitly excluded: the owner confirms it was added
  for a later browser-build context, not the minimum boot path.

Build run by project owner: NOT_RUN
Device test run by project owner: NOT_RUN
Conclusion: The known-display linux-fa5 source confirms the FA5 DTS compatible
already ported to Linux 7.2, but does not identify a new display or boot patch.
Recommended next experiment: a source-only 7.2 ARM64/Qcom early-boot audit
using the 6.19.5 tree as an intermediate reference, then selecting exactly
one evidence-backed configuration or code hypothesis;
do not modify DRM/MSM, GPU, panel, audio, PM, or legacy DTS wholesale.
```
