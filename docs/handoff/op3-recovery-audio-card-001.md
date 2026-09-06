# OP3 recovery ALSA sound-card registration handoff

```text
Task / GitHub Issue: recovery kernel audio-card restore
Role: Implementation
Baseline commit: 67b0bbc3cbf46bae712a2606a43361756fcbd829
Working branch: agent/implementation/recovery-browser-audio-full-001 (final kernel integration)
Changed files: final kernel integration worktree, based on agent/implementation/op3-audio-mic-001;
  physical-key and haptics commits are included on top
Commit SHA: 9491be0d6460 (kernel); recovery preparation docs: be89bec

Layer: Linux 6.12 OnePlus 3 audio DTS DAI enumeration
Hypothesis tested: the previously observed no-card state is fixed by using
the already validated audio branch, which keeps valid q6asm sessions and
disables unavailable OP3 MM4--MM16 links; no new q6asm child IDs are added.
Only variable changed: kernel branch composition; recovery userspace,
Wi-Fi, DRM, and browser contents are unchanged.

Build run by project owner: YES
Build result: PASS
Artifacts and SHA256: output directory
`out/pmos-msm8996-6.12-recovery-audio-full-retry`; `.config`
`c3da142eb257c5b0b501f24b91b74c0dba58a16ac6af252f054a4f14540baeb3`,
`Image.gz` `504ca5bc1623dd038cb3a9c9d36b7e77d45a3c28e34f7c86e1cff1a05f3347bd`,
and `msm8996-oneplus3.dtb`
`264f981678c1dd8d1d9a52f2db6e2130a0ebccbb9f4485ab8740784f73806db7`.
The final test initrd, with the current recovery audio/browser bundle
overlaid, is `artifacts/initrd-op3-recovery-browser-audio.cpio.gz` with SHA256
`c0f0a2b45a5b9b1e6d34f25c5a20d19521203794c779b9460b9d0e5ec5a0055b`; the
packed temporary-boot image is
`artifacts/boot-oneplus3-pmos612-recovery-audio-full-v2.img` with SHA256
`2a979d0fd78fdf492141fb6213a4206d72d26de381e3e3ae04f96c8568f41028`.

Device test run by project owner: NOT_RUN for the final integration branch
Device result: NOT_RUN
Evidence links / log paths: Pre-patch device evidence from
root@172.16.42.1 shows `/dev/snd` contains only `timer`,
`/proc/asound/cards` reports `--- no soundcards ---`, and dmesg reports:
`msm-snd-apq8096 sound: error -EINVAL: MultiMedia4: error getting cpu dai
name` followed by `probe ... failed with error -22`. `tinycap` therefore
reports that card 0/device 0 does not exist and no `/tmp/voice.wav` is made.

The rejected experiment was top-level commit `228b319`, which added q6asm
child IDs 3--15. The owner booted it and dmesg showed repeated `valid dai id
not found:0`, `Failed to register DAIs: -12`, and no ALSA card. Its revert-only
worktree `6e5e5e728512` was not selected for the final test because it still
lacked the later validated audio-branch DTS fixes.

Conclusion: INCONCLUSIVE
Uncertainties:
  - The final integration branch still requires the owner to boot the new image.
  - Sound-card registration does not prove that the ADSP, WCD9335 codec,
    AMIC4 capture route, QUAT MI2S speaker route, or external amplifier are
    electrically functional.
  - The recovery userspace audio commit remains separate and is meaningful
    only after `/proc/asound/cards` and `/dev/snd/pcm*` appear.

Static verification: after correcting the first DTC syntax failure, the
final kernel integration worktree is clean; its OP3 DTS
contains only q6asm `dai@0`--`dai@2` and disables MM4--MM16 in the board DTS.
The owner completed the kernel build successfully; the final boot image is
ready for the post-build device test.

The owner build first failed at DTC line 91 of
`msm8996-oneplus-common.dtsi` because the conflict resolution omitted the
closing brace for `&soc`. Commit `9f81c0cd4289` adds the missing brace and
`9491be0d6460` restores the affected indentation. The current worktree is
clean and ready for a new owner build. A subsequent build also exposed that
the copied output configuration embeds ath10k firmware from the ignored
`extfw/` directory. The two required files are now staged locally in the
final worktree: `firmware-6.bin` (706360 bytes) and `board-2.bin` (740076
bytes). They are external firmware inputs and intentionally are not committed.

Recommended next experiment: the owner should compile the final integration worktree,
pack and boot the resulting image, then collect `/proc/asound/cards`,
`/proc/asound/pcm`, `/dev/snd`, and audio dmesg. If an ALSA card appears,
test `tinycap` first and then `tinyplay`; only after those pass should the
recovery microphone-key workflow be evaluated.
```
