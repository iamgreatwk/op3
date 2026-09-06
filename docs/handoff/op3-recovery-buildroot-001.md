# OP3 recovery Buildroot integration handoff

```text
Task / GitHub Issue: owner-authorized recovery bundle Buildroot integration
Role: Implementation
Baseline commit: 6f8bab4
Working branch: agent/implementation/recovery-browser-001
Changed files: buildroot/package-patches/op3-recovery/{Config.in,op3-recovery.mk};
  buildroot/op3-recovery.defconfig; scripts/prepare-op3-buildroot.sh;
  scripts/stage-op3-audio-rootfs.sh; docs/rebuild-recovery.md;
  boot/recovery-browser-test/README.md; docs/handoff/latest.md
Commit SHA: pending

Layer: Buildroot recovery package and persistent payload staging
Hypothesis tested: The tracked recovery C/libtsm sources and browser-session
helpers can be installed by a project-owned Buildroot package, so one
recovery target contains recovery, audio, and Wi-Fi without relying on an
older binary under out/recovery or a standalone recovery bundle.
Only variable changed: recovery ownership moved from the standalone stager
to the Buildroot target; kernel, initramfs firmware, browser package
selection, and recovery source behavior were not changed.

Build run by project owner: NOT_RUN
Build result: NOT_RUN
Artifacts and SHA256: no generated project artifact was created or replaced.
The package source is copied into a fresh Buildroot checkout by
scripts/prepare-op3-buildroot.sh from tracked recovery/, third_party/libtsm/,
and runner sources. The recovery defconfig hash is recorded in
manifests/op3-recovery-audio-full.env.

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: a clean temporary Buildroot checkout at commit
`679b9ead7620bbf193620d1ebf56f53c1764d37a` accepted the recovery package
registration and resolved `BR2_PACKAGE_OP3_RECOVERY=y`. Preparation checks
confirmed the copied package source, all libtsm sources, and runner scripts.
A synthetic combined target passed the audio/Wi-Fi/recovery staging checks.
The Buildroot package dry-run reached the host dependency gate but was
blocked by the host's uutils `install` dependency warning/error; no target
package compilation occurred.

Conclusion: INCONCLUSIVE
Uncertainties: the owner must run the real Buildroot recovery build with the
matching kernel modules root. The `op3-recovery` static link must be verified
by that buildroot toolchain, and the owner must retest boot, DRM GUI, keys,
audio, vibration, and Wi-Fi using the resulting combined payload.

Recommended next experiment: on a clean prepared Buildroot source, run:

  OP3_WIFI_MODULES_ROOT="$PWD/artifacts/op3-wifi-modules-root" \
    make -C source/buildroot O="$PWD/out/buildroot-op3-recovery" \
    op3_recovery_defconfig
  OP3_WIFI_MODULES_ROOT="$PWD/artifacts/op3-wifi-modules-root" \
    make -C source/buildroot O="$PWD/out/buildroot-op3-recovery" \
    BR2_JLEVEL=3
  ./scripts/stage-op3-audio-rootfs.sh \
    out/buildroot-op3-recovery/target \
    artifacts/op3-recovery-audio-rootfs.tar.gz

Deploy that persistent target together with the existing recovery-browser
initrd. Build the browser profile and browser bundle separately only when
browser testing is explicitly requested.
```
