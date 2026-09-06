# OP3 recovery clean-rebuild provenance handoff

```text
Task / GitHub Issue: owner-authorized recovery rebuild organization
Role: Implementation
Baseline commit: d43d238137ce0e85916a15132dc990935f7d99cb
Working branch: agent/implementation/recovery-browser-001
Changed files: manifests/op3-recovery-audio-full.env;
  buildroot/op3-browser.defconfig; buildroot/package-patches/cog/;
  boot/base-initramfs/reference-boot.sha256;
  boot/audio-test/opt/op3-audio/route.sh;
  scripts/prepare-op3-buildroot.sh; scripts/extract-reference-initrd.sh;
  scripts/stage-op3-audio-rootfs.sh; scripts/stage-browser-rootfs.sh;
  docs/rebuild-recovery.md
Commit SHA: pending

Layer: source and build-input provenance
Hypothesis tested: A fresh checkout of the current top-level GitHub branch,
the pinned external Buildroot commit, the archived kernel patch series, and
explicitly hash-verified external binary inputs provide a complete, repeatable
recipe for rebuilding the current recovery/browser/Wi-Fi/audio payloads.
Only variable changed: build-source preparation and artifact-staging
provenance; no kernel, DTS, recovery runtime, Wi-Fi, DRM, or audio behavior
was changed.

Build run by project owner: NOT_RUN
Build result: NOT_RUN
Artifacts and SHA256: no generated artifact was created or replaced. The
Buildroot source is pinned to 679b9ead7620bbf193620d1ebf56f53c1764d37a. The
project Buildroot defconfig is pinned to SHA256
38652c8c71f2a5d762b35e5d274700dd5144157fd2e619fcedf793f945078358. The
known external reference boot image is pinned to
29ccd3eb8b093b29fc44435bd6e5f98367cf3794c117f9527a6bf3c1ebc5d781 and its
extracted reference initrd to
c3358a1cadb747996ddaa492e636827f2d72974040e8fd40d81f8a213e676366.

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: static shell checks and Cog patch dry-run only;
no device action was taken.

Conclusion: INCONCLUSIVE
Uncertainties: Buildroot, the historical reference boot/initrd, ath10k
extfw, Qualcomm firmware inputs, and the CJK font remain external inputs. A
true GitHub-only rebuild is not claimed until the owner completes a clean
checkout build and verifies the generated payloads.
Recommended next experiment: from a fresh clone, run
`docs/rebuild-recovery.md` through the owner Buildroot and kernel build steps,
then compare the resulting image and bundle manifests before deleting the old
generated directories.
```
