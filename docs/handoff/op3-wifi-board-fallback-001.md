# OP3 Wi-Fi board fallback handoff

Task / GitHub Issue: Current Wi-Fi recovery task; owner-requested board-data fix
Role: Implementation
Baseline commit: `129f73f` top-level project checkpoint; formal kernel baseline is pinned in `manifests/op3-recovery-audio-full.env`
Working branch: `agent/implementation/recovery-browser-001`
Changed files: `scripts/stage-op3-initramfs-firmware.sh`, `scripts/restore-op3-recovery-kernel.sh`, `buildroot/op3-recovery-post-build.sh`, `kernel/configs/oneplus3-recovery-audio-full.config`, `manifests/op3-recovery-audio-full.env`, Wi-Fi/rebuild documentation
Commit SHA: pending

Layer: Wi-Fi firmware packaging and recovery provisioning
Hypothesis tested: QCA6174 probe fails because the device reports PCI subsystem `0000:0000`; `board-2.bin` has no matching entry and the firmware loader needs the legacy `board.bin` fallback.
Only variable changed: add the hash-verified QCA6174 `board.bin` fallback to the external input and every firmware staging/embedding path.

Evidence before change:

- ACM diagnostics showed `ath10k_pci`, `ath10k_core`, `ath`, `mac80211`, `cfg80211`, and `rfkill` loaded.
- PCI function `0000:01:00.0` was bound to `ath10k_pci`, and `firmware-6.bin` plus `board-2.bin` were present.
- Kernel log reported `failed to fetch board data ... subsystem-vendor=0000,subsystem-device=0000`, then `failed to fetch board-2.bin or board.bin`, `failed to fetch board file: -2`, and `could not probe fw (-2)`.
- The historical reference initrd contains `board.bin`; the extracted 8124-byte file matches the host firmware package and has SHA256 `1a8d225818b46986fc4f615594fbe448fa820618590d6902c8f844bb37cda667`.

Build run by project owner: NOT_RUN
Build result: NOT_RUN
Artifacts and SHA256: Existing locked artifacts do not contain `board.bin`; a new kernel/Buildroot/image build is required.

Device test run by project owner: NOT_RUN
Device result: FAIL before change (`wlan0` did not appear); retest pending with the rebuilt image.
Evidence links / log paths: ACM diagnostic transcript; device `dmesg` contains the board-data `-2` failure described above.

Conclusion: INCONCLUSIVE
Uncertainties: The fallback board file is now sourced and hash-locked, but the rebuilt kernel/initramfs has not yet been boot-tested.
Recommended next experiment: restore/stage the new external input, rebuild the pinned kernel with `CONFIG_EXTRA_FIRMWARE` including `board.bin`, rebuild Buildroot/initramfs, package a new test image, and verify `wlan0` plus `wifi connect`.
