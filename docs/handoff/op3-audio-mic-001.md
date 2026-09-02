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
Only variable changed: The pmOS 6.12 OnePlus 3 audio integration layer: its built-in kernel configuration and a runtime-validating route diagnostic helper. The follow-on test image appends only that helper and its launcher to the already firmware-proven initramfs; it does not replace or alter sda15/rootfs. The DTS topology, boot profile, ADSP firmware payload, GPU/DRM, Wi-Fi and other services remain unchanged.

Build run by project owner: Configuration preparation and `Image.gz dtbs`, 2026-08-31
Build result: PASS — the configured kernel Image.gz and OnePlus 3 DTB exist in the requested output directory.
Artifacts and SHA256: The prepared configuration is `out/linux-pmos-msm8996-6.12-defconfig-audio/.config`, SHA256 `56d2f1307bbd9cc502ab4b6f33e5b2b80387a0198f6137f6870c5de996a9ee52`. The build produced `Image.gz` SHA256 `cefc36a899aef73b3af50b53fd7a2d5dcfcc8f10b702b150f35c0eeffc1ed0a1` and `msm8996-oneplus3.dtb` SHA256 `cb29ab658135cd0cfcde3b47c1e115b763f5dbd37b724554590b7a61afcbf32f`. Static verification confirms `QCOM_Q6V5_ADSP`, `QCOM_APR`, `SLIMBUS`, `SLIM_QCOM_NGD_CTRL`, `SND_SOC_MSM8996`, `SND_SOC_WCD9335`, `SND_SOC_TFA989X`, and their QDSP6/REGMAP/RPROC closure resolve to `=y`; the compiled DTB retains `qcom,apq8096-sndcard` and its AMIC4 route.

Owner packaging: PASS (2026-08-31). `artifacts/boot-oneplus3-pmos612-own-dtb-audio.img` SHA256 `7c49bbe60944224b656dd0580b4cb875cbda0463fe3e40040c5ad7e4a814fae9`; full staged persistent rootfs archive `artifacts/op3-audio-rootfs.tar.gz` SHA256 `6006f5ffe09d48da2cb06ee6a93bf56d33cbe54a13c748938aa720a91a101f54`. Packaging PASS establishes only that the declared inputs produced an Android boot image and transfer archive. It does not establish kernel boot, ALSA registration, playback, capture, or suspend/resume behaviour.

Rootfs staging: PASS (owner-run, 2026-08-31). `BR2_PACKAGE_TINYALSA=y` and `BR2_PACKAGE_TINYALSA_TOOLS=y` are present in `out/buildroot-op3-egl/.config`. The staged target contains `usr/bin/tinymix` (`bd6b6dd37da7b969780846f8d09a33d4f386de54208c207cd5b7dd97a2da01c8`), `usr/bin/tinyplay` (`ce941c58cf43f0136b67a5397e40871b8afa2ee0ae3f722cc8c1f94909d666d5`), `usr/bin/tinycap` (`cd6b5c323680e668a73d33adcc82d275c01b1e81a0aa0328a165b40078f6bd24`), and `opt/op3-audio/route.sh` (`c1787698afc7d4b151df4089a5e4b1f38ba0e919a0a1d0cb61d8a590a20566a9`). This is rootfs assembly evidence only, not an ALSA or device PASS.

Initramfs generator self-test: PASS (agent-run, temporary path only). The first generator revision falsely rejected existing TinyALSA entries because `grep -q` closed a pipe under `pipefail`, causing `cpio` to report SIGPIPE. The corrected generator first materializes the archive listing, then checks it. It successfully appended `opt/op3-audio/route.sh` and `sbin/run_recovery.sh` to the firmware-provenance reference archive; no project artifact was created or overwritten during this self-test.

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: Collect /newroot/var/log/op3-audio-route.log, `cat /proc/asound/cards`, `cat /proc/asound/pcm`, and the matching `dmesg` ADSP/APR/SLIM/WCD9335 lines.

Conclusion: INCONCLUSIVE
Uncertainties: No device has yet run this configuration.  The helper intentionally checks the exported mixer controls before setting routes; the exact PCM device IDs are discovered from /proc/asound/pcm rather than assumed.  Speaker output and AMIC4 capture need hardware evidence, including post-suspend/resume repetition, before any acceptance claim.
Recommended next experiment: Build the minimal appended archive with `scripts/make-op3-audio-initrd.sh`, pack it with the committed audio Image.gz and own DTB, and boot it. It automatically runs `route.sh diagnose` from initramfs and persists the result at `/newroot/var/log/op3-audio-initramfs.log`; no sda15/rootfs change is made. Only if the card and MultiMedia3 PCMs exist, use owner-directed `route.sh speaker` plus 30-second tinyplay, then `route.sh mic` and 48 kHz/16-bit/mono tinycap.
```
