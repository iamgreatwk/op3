# OP3 Wi-Fi Buildroot integration handoff

```text
Task / GitHub Issue: owner-authorized Wi-Fi Buildroot integration request
Role: Implementation
Baseline commit: 60233fc
Working branch: agent/implementation/recovery-browser-001
Changed files: buildroot/op3-recovery.defconfig;
  buildroot/op3-recovery-post-build.sh; boot/wifi/opt/op3-wifi/wifi;
  scripts/prepare-op3-buildroot.sh; scripts/stage-op3-audio-rootfs.sh;
  manifests/op3-recovery-audio-full.env; boot/wifi/README.md;
  docs/rebuild-recovery.md; docs/handoff/latest.md
Commit SHA: pending

Layer: Buildroot recovery userspace and Wi-Fi payload packaging
Hypothesis tested: The default recovery Buildroot target can own the Wi-Fi
userspace and the exact 6.12.1 ath10k module dependency closure, while the
browser profile remains a separate opt-in Buildroot/bundle flow.
Only variable changed: Wi-Fi ownership moved from the standalone persistent
Wi-Fi bundle into the recovery Buildroot post-build target; kernel, initramfs
firmware, browser package selection, and device behavior were not changed.

Build run by project owner: NOT_RUN
Build result: NOT_RUN
Artifacts and SHA256: no generated project artifact was created or replaced.
The current owner-produced `artifacts/op3-wifi-modules-root` was read as the
module input; it is not committed.

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: local temporary checks only. A clean temporary
Buildroot checkout at commit `679b9ead7620bbf193620d1ebf56f53c1764d37a`
accepted the recovery preparation script. The post-build hook selected seven
modules (`rfkill`, `cfg80211`, `libarc4`, `mac80211`, `ath`, `ath10k_core`,
`ath10k_pci`) and installed the tracked Wi-Fi scripts. The combined audio
stager accepted a synthetic target containing those Wi-Fi files and the
TinyALSA tools. The browser profile preparation separately installed all
three tracked Cog patches and did not install the recovery hook.

Conclusion: INCONCLUSIVE
Uncertainties: the owner must compile the pinned Buildroot recovery profile;
the post-build hook depends on a single clean `OP3_WIFI_MODULES_ROOT` from
the matching kernel `modules_install` output. Device automatic Wi-Fi/DHCP,
IPv6-off policy, and recovery/audio behavior still need testing with the
combined payload.

Recommended next experiment: after the owner builds the final kernel modules,
run `scripts/prepare-op3-buildroot.sh source/buildroot recovery`, then:

  OP3_WIFI_MODULES_ROOT="$PWD/artifacts/op3-wifi-modules-root" \
    make -C source/buildroot O="$PWD/out/buildroot-op3-recovery" \
    op3_recovery_defconfig
  OP3_WIFI_MODULES_ROOT="$PWD/artifacts/op3-wifi-modules-root" \
    make -C source/buildroot O="$PWD/out/buildroot-op3-recovery" \
    BR2_JLEVEL=3

Stage `out/buildroot-op3-recovery/target` with
`scripts/stage-op3-audio-rootfs.sh`, then use the existing recovery initrd
and owner-authorized device test procedure. Build the browser profile only
when browser testing is explicitly requested.
```
