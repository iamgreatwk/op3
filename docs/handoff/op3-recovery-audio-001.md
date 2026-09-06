# OP3 recovery voice capture and playback handoff

```text
Task / GitHub Issue: pending owner issue for recovery voice audio
Role: Implementation
Baseline commit: 67b0bbc3cbf46bae712a2606a43361756fcbd829
Working branch: agent/implementation/recovery-browser-001
Changed files: recovery/recovery_mainline.c,
  boot/recovery-browser-test/README.md
Commit SHA: 2eab778

Layer: recovery userspace audio session lifecycle
Hypothesis tested: The existing OP3 audio route can be made usable from the
recovery voice key by keeping the verified AMIC4 capture controls, detecting
the captured WAV before playback, and using an available TinyALSA playback
tool when the optional pcm-wav helper is absent.
Only variable changed: recovery userspace capture/playback command lifecycle
and diagnostics; no kernel, DTS, Buildroot audio payload, Wi-Fi, DRM, input,
or browser changes.

Build run by project owner: NOT_RUN
Build result: NOT_RUN (the owner must run the formal kernel/Buildroot builds)
Artifacts and SHA256: Agent static recovery compile PASS:
  out/recovery/recovery_mainline-audio
  a82e2ff356e0447ecce422b372dbc134c23658c9bd08d4f5ebd48db41368f300
  Staged package:
  artifacts/op3-recovery-browser-audio-bundle.tar.gz
  a744c2ea4112d3551681e6aaa42ed2ec86ca14c712ecab284760134a76832fe8

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: No device test was run by this agent. The
runtime audio payload audit found /usr/bin/tinycap, /usr/bin/tinymix, and
/usr/bin/tinyplay in artifacts/op3-audio-rootfs.tar.gz; pcm-wav is absent.
The existing capture route is the previously verified AMIC4 -> MultiMedia1
path on hw:0,0. Recovery writes diagnostics to
/tmp/op3-recovery-audio.log and the capture to /tmp/voice.wav.

Conclusion: INCONCLUSIVE
Uncertainties:
  - The tinyplay fallback does not provide the mono-to-stereo conversion of
    the optional historical pcm-wav helper, so actual external-speaker
    playback must be checked on the complete device image.
  - The prior audio-agent handoff reported that route.sh speaker plus
    tinyplay was not yet a passing external-speaker test; this change does
    not claim to fix the separate kernel/route issue.
  - The user interaction is a two-second hold on the microphone key to start
    recording, followed by a tap to stop and play the WAV.

Static verification: git diff --check and the small static AArch64 recovery
compile passed. The staged bundle contains the new recovery binary and the
existing browser runners. No kernel/Buildroot build or device test was run by
this agent.

Recommended next experiment: The owner should deploy the staged bundle to
/newroot, boot the current all-input image, verify the three audio tools are
available through the recovery PATH, hold the microphone key for two seconds,
make a short recording, tap again, and inspect /tmp/op3-recovery-audio.log,
the WAV size/header, and audible playback. If capture passes but playback is
silent, open a separate audio route/kernel investigation; do not broaden this
userspace checkpoint.
```
