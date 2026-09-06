# OP3 Wi-Fi board fallback build handoff

Task / GitHub Issue: Current Wi-Fi recovery task; owner-authorized corrected-image build
Role: Implementation
Baseline commit: `312eb0b` top-level project; formal kernel tree `dd6476a68184e7293b05a7e962f0536f0d54048a`
Working branch: `agent/implementation/recovery-browser-001`; kernel worktree branch `agent/implementation/recovery-browser-audio-full-001`
Changed files: generated build outputs and test artifacts only; source fix commits are `cb0003e`, `26e934a`, `b17e838`, and `312eb0b`
Commit SHA: source fix commits listed above; this handoff records the owner build

Layer: Wi-Fi firmware packaging and recovery provisioning
Hypothesis tested: the QCA6174 device requires the legacy `board.bin` fallback because its PCI subsystem is reported as `0000:0000` and `board-2.bin` has no matching entry.
Only variable changed: build and package the already committed, hash-verified `board.bin` fallback through kernel firmware embedding, initramfs staging, and Buildroot post-build.

Build run by project owner: YES (explicit authorization in the current task)
Build result: PASS
Artifacts and SHA256:

- Kernel `Image.gz`: `835480696c9318e7c8d4895dcba15363ecd2b8b6463c870f750c0134cc8b8d3`
- Kernel DTB: `264f981678c1dd8d1d9a52f2db6e2130a0ebccbb9f4485ab8740784f73806db7`
- Buildroot initrd: `artifacts/initrd-op3-recovery-buildroot-board-fallback.cpio.gz`, `315cc923a43d5caded31612cfc043285d552e339adac60b6c40c6684c09eede1`
- Persistent rootfs tarball: `artifacts/op3-audio-rootfs-board-fallback.tar.gz`, `631ca41fa6ceb07c5dfa4f1e3cb182130cae985532efb12b78c90f9503e6f957`
- Boot image: `artifacts/boot-oneplus3-pmos612-recovery-buildroot-board-fallback.img`, `9ecef144150562b6eae47b34b54df5929a34af304303af11d07e391af851a2a0`

The Buildroot output is `out/buildroot-op3-recovery-board-fallback`. Its CPIO passed `gzip -t` and contains `recovery_mainline`, TinyALSA `tinycap`/`tinymix`/`tinyplay`, Wi-Fi scripts, the ath10k module closure, and all three QCA6174 files: `firmware-6.bin`, `board-2.bin`, and `board.bin`. The fallback in the target has SHA256 `1a8d225818b46986fc4f615594fbe448fa820618590d6902c8f844bb37cda667`.

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: Buildroot log `/tmp/buildroot-op3-recovery-board-fallback.log`; package contents verified from `out/buildroot-op3-recovery-board-fallback/images/rootfs.cpio.gz`.

Conclusion: INCONCLUSIVE
Uncertainties: The temporary ACM test with manually copied `board.bin` already made `wlan0` appear and associated successfully, but this newly built kernel/initrd has not yet been boot-tested. The canonical manifest still points to the previous tested artifact names until this corrected image passes the device test.
Recommended next experiment: boot `artifacts/boot-oneplus3-pmos612-recovery-buildroot-board-fallback.img` with `fastboot boot`, then verify ath10k probe, `wlan0`, `wifi reconnect`, DHCP, and IPv6-off state over ACM/SSH.
