# OP3 live camera preview no-frame diagnostic (2026-09-14)

Task / GitHub Issue: Camera live-preview follow-up to the OP3 autofocus test
line; issue reference was not supplied in this run.
Role: implementation/test
Baseline commit: top-level `4fa5cd9` (`test: add DRM camera live preview`)
Working branch: `agent/implementation/recovery-browser-001`
Changed files: `scripts/op3-v4l2-live-preview.c`
Commit SHA: `350bce0`, `7d5ef6c`, `ef85820`, `91472ab`

Layer: userspace camera preview diagnostic
Hypothesis tested: the gray screen and apparent self-exit might be caused by
the preview program hiding a V4L2/poll/input failure.
Only variable changed: the temporary ARM64 preview executable; kernel, DTB,
camera modules, rootfs, and Buildroot were unchanged.

Build run by project owner: NO; host-side cross-build only
Build result: PASS
Artifacts and SHA256:

```text
64854388df6ead33093aeac14a844547dadc25fa04e2b0be44110321ad3dc2b8  out/recovery/op3-v4l2-live-preview-diagnostics
```

Device test run by project owner: NO; test executed through the authorized
device SSH session
Device result: FAIL (no camera frame)
Evidence links / log paths:

```text
device: /tmp/op3-live-preview-diagnostics.log
device: dmesg | grep -iE 'imx298|camss|vfe|smmu|overflow|fault'
```

The clean-boot AF-helper control run passed before testing the live preview:
it captured `8/8` RAW10 frames, completed the VCM write at position `512`, and
introduced no new VFE/SMMU error. The old-order live preview then reported:

```text
preview frame timeout after 3002182 us
preview result=FAIL reason=frame-timeout rc=-110 errno=110(Connection timed out) frames=0
```

That live-preview run produced `qcom-camss ... VFE0 rdi0 overflow` and repeated ARM
SMMU `Unhandled context fault` messages with `WNR=1`. The previously validated
AF helper (`e29d96f8ff44972ba5418ced1379fa83b90d9f448e4d699f40eea53c3e768b4f`)
also timed out with `capture poll timeout frame=0` in that post-failure state.
The AF helper had passed immediately before the old-order preview on the clean
boot, so the live-preview startup ordering is now the primary hypothesis.

Conclusion: INCONCLUSIVE
Uncertainties: The old preview started `STREAMON` before DRM modesetting. The
fresh-boot test of candidate `ef85820` moved DRM setup before `STREAMON`, but
still timed out after the AF helper had passed and reproduced VFE/SMMU faults.
`PASS frames=0` from the old preview was a reporting bug; the new diagnostic
distinguishes a signal/key stop from a frame timeout.
Recommended next experiment: candidate `91472ab` defers `SETCRTC` until after
the first valid camera buffer. Perform another fresh `fastboot boot`, load the
complete ordered module chain, run the known-good AF helper first, and only
then run the `91472ab` diagnostic preview. Do not unload CAMSS/CCI or reuse the
camera stream after an SMMU/VFE fault. If the reordered preview still fails,
investigate the CAMSS/SMMU buffer path before DRM.
