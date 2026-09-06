# OP3 external input bundle handoff

```text
Task / GitHub Issue: owner-authorized external input organization
Role: Implementation
Baseline commit: f8f43d33fb70f6a5424f28f9bf0ec968e8cebb34
Working branch: agent/implementation/recovery-browser-001
Changed files: manifests/op3-recovery-audio-full.env;
  scripts/restore-op3-recovery-kernel.sh;
  scripts/extract-reference-initrd.sh;
  scripts/prepare-a530-firmware.sh;
  scripts/stage-msm8996-oneplus3-firmware.sh;
  scripts/stage-browser-rootfs.sh;
  scripts/verify-op3-external-inputs.sh;
  docs/rebuild-recovery.md; docs/handoff/latest.md
Commit SHA: 2a49235

Layer: external input and build provenance
Hypothesis tested: A single directory outside the checkout can hold every
external binary/tool input required by the recovery rebuild, and the tracked
scripts can consume it without relying on old ignored worktrees or PATH state.
Only variable changed: external input location and tool-path resolution; no
kernel, DTS, recovery runtime, browser runtime, Wi-Fi, DRM, audio, or device
behavior was changed.

Build run by project owner: NOT_RUN
Build result: NOT_RUN
Artifacts and SHA256: no repository artifact was created or replaced. The
external directory `/home/kai/op3-recovery-external-inputs` was populated and
verified with its SHA256/SHA512 manifests. Firmware staging was run in a
temporary output directory and matched the tracked firmware manifest.

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: `scripts/verify-op3-external-inputs.sh`; reference
initrd extraction, A530 staging, and Qualcomm firmware staging checks passed.

Conclusion: PASS for input organization; device/runtime validation is not
applicable to this source-only checkpoint.
Uncertainties: the external directory is local storage and must be backed up
independently. The proprietary/historical inputs are not committed to GitHub.

Recommended next experiment: after a clean checkout, export
`OP3_EXTERNAL_INPUTS`, run the verifier, and follow
`docs/rebuild-recovery.md` through the owner-run Buildroot and kernel builds.
```
