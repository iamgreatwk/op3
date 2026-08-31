# OP3-AUDIO-MIC-001 — first build checkpoint

```text
Task / GitHub Issue: https://github.com/iamgreatwk/op3/issues/5
Role: Implementation
Baseline commit: 67b0bbc3cbf46bae712a2606a43361756fcbd829
Working branch: agent/implementation/op3-audio-mic-001
Changed files: kernel/configs/oneplus3-audio.fragment; scripts/prepare-op3-audio-kernel-config.sh; scripts/stage-op3-audio-rootfs.sh; boot/audio-test/opt/op3-audio/route.sh; this handoff
Commit SHA: 2cc781600b01b18fbf0a21e394b8bb805577feee (audio integration assets)

Layer: Audio integration
Hypothesis tested: When the already-described OnePlus 3 ASoC topology has its complete ADSP, APR, SLIMbus, WCD9335, MSM8996 machine-driver and TFA9890 dependency chain built in, it registers ALSA controlC0 and the MultiMedia3 playback/capture PCMs after the provisioned ADSP starts.
Only variable changed: The pmOS 6.12 OnePlus 3 audio integration layer: its built-in kernel configuration and device-local, runtime-validating route diagnostic helper.  The DTS topology, boot profile, ADSP firmware payload, GPU/DRM, Wi-Fi and other services remain unchanged.

Build run by project owner: NOT_RUN
Build result: NOT_RUN
Artifacts and SHA256: NOT_RUN

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: Collect /newroot/var/log/op3-audio-route.log, `cat /proc/asound/cards`, `cat /proc/asound/pcm`, and the matching `dmesg` ADSP/APR/SLIM/WCD9335 lines.

Conclusion: INCONCLUSIVE
Uncertainties: No device has yet run this configuration.  The helper intentionally checks the exported mixer controls before setting routes; the exact PCM device IDs are discovered from /proc/asound/pcm rather than assumed.  Speaker output and AMIC4 capture need hardware evidence, including post-suspend/resume repetition, before any acceptance claim.
Recommended next experiment: The owner applies the established own-DTB series, runs `scripts/prepare-op3-audio-kernel-config.sh`, enables `BR2_PACKAGE_TINYALSA=y` and `BR2_PACKAGE_TINYALSA_TOOLS=y` in the owner Buildroot configuration, stages the resulting target with `scripts/stage-op3-audio-rootfs.sh`, then builds/boots the image and records the Issue #5 PASS/FAIL criteria.  First run `route.sh diagnose`; only if it verifies the card/PCMs, run `route.sh speaker` followed by 30-second tinyplay, then `route.sh mic` and 48 kHz/16-bit/mono tinycap.
```
