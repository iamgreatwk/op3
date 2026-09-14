# OP3 live camera preview no-frame diagnostic (2026-09-14)

Task / GitHub Issue: Camera live-preview follow-up to the OP3 autofocus test
line; issue reference was not supplied in this run.
Role: implementation/test
Baseline commit: top-level `4fa5cd9` (`test: add DRM camera live preview`)
Working branch: `agent/implementation/recovery-browser-001`
Changed files: `scripts/op3-v4l2-live-preview.c`
Commit SHA: `350bce0`, `7d5ef6c`, `ef85820`, `91472ab`, `a96e86c`

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

The next candidate `a96e86c` goes one step further: it does not open
`/dev/dri/card0` or allocate a DRM dumb buffer until after the first valid
camera buffer has been dequeued. Its host-built ARM64 executable is:

```text
60c85dcc61d9ae388092d6c1c095684306a27ee7cd61e7a9972667eaed128210  out/recovery/op3-v4l2-live-preview-diagnostics
```

It requires another clean boot because the `91472ab` test ended with VFE/SMMU
faults.

## Same-boot repeat-open validation (2026-09-14)

This test isolates the owner's hypothesis that the camera can be opened only
once during one boot. It started from a fresh `fastboot boot` of the current
recovery image, loaded the complete ordered camera module chain once, and ran
the known-good AF helper twice consecutively. No DRM preview was started and
no camera module was unloaded between runs.

The first open passed:

```text
af1-rc=0
result frames_good=8 frames_error=0 focus_result=0 focus_position=512
G1 result=PASS capture-ready-and-focus-ioctl-returned
af1-raw-count=8
```

The second open failed in the same boot:

```text
af2-rc=1
capture poll timeout frame=0
G1 result=FAIL rc=-110 errno=110(Connection timed out)
af2-raw-count=0
```

The second run produced repeated `qcom-camss ... VFE0 rdi0 overflow` messages;
the first run's power-off path completed normally. The media nodes remained
present, so this is not a missing-module or missing-device-node failure. The
same-boot repeat-open hypothesis is therefore **CONFIRMED** for the current
CAMSS/VFE path. The likely fault domain is incomplete stream teardown or stale
CAMSS/SMMU state after the first capture, not DRM startup ordering.

Device evidence was collected through SSH at `172.16.42.1`:

```text
/tmp/op3-af-repeat1.log
/tmp/op3-af-repeat2.log
dmesg | grep -iE 'imx298|camss|cci|csiphy|csid|vfe|smmu|context fault|overflow|fault|reset|panic'
```

The next code experiment must remain in the camera kernel/userspace camera
layer: audit and repair the stream-off/close teardown so a second open starts
from a clean VFE/ISPIF/CSID/CSI-PHY state. Do not change DRM, Buildroot, or
rootfs in that experiment. Reboot before each new trial; do not use `rmmod`
against CAMSS or CCI as a substitute for a clean reset.
