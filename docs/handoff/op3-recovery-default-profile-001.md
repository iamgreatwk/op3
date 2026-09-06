# OP3 recovery default Buildroot profile handoff

```text
Task / GitHub Issue: owner-authorized default profile change
Role: Implementation
Baseline commit: 7e64169
Working branch: agent/implementation/recovery-browser-001
Changed files: buildroot/op3-recovery.defconfig;
  buildroot/op3-browser.defconfig; manifests/op3-recovery-audio-full.env;
  scripts/prepare-op3-buildroot.sh; scripts/stage-browser-rootfs.sh;
  scripts/stage-op3-audio-rootfs.sh; docs/rebuild-recovery.md;
  docs/handoff/latest.md
Commit SHA: 80faba3

Layer: Buildroot profile selection
Hypothesis tested: The final recovery default can omit the browser graphics
stack while preserving an explicit browser profile for later testing.
Only variable changed: default Buildroot package selection; no kernel, DTS,
recovery runtime, browser runtime, Wi-Fi, DRM, or audio behavior changed.

Build run by project owner: NOT_RUN
Build result: NOT_RUN
Artifacts and SHA256: no project artifact was created or replaced. A clean
temporary Buildroot checkout was prepared for both profiles. The recovery
profile passed a negative check for Mesa3D, Weston, WPE WebKit, WPEWebDriver,
and Cog; the browser profile installed all three tracked Cog patches.

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: temporary profile-preparation checks; no large
Buildroot build and no device action were taken.

Conclusion: PASS for profile preparation; owner Buildroot compilation is
pending.
Uncertainties: the existing browser bundle remains a separately built
optional artifact and is not part of the new default recovery target.

Recommended next experiment: build
`out/buildroot-op3-recovery` from `op3_recovery_defconfig`, then stage the
audio bundle from its target directory. Build the browser profile only when
an explicit browser test is requested.
```
