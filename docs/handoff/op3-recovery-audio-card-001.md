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

Build run by project owner: NOT_RUN
Build result: NOT_RUN
Artifacts and SHA256: No kernel artifact was built. Prepared kernel worktree
is clean and contains the validated audio branch plus the physical input and
haptics commits.

Device test run by project owner: NOT_RUN for the prepared integration branch
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
  - The final integration branch still requires the owner to rebuild and boot it.
  - Sound-card registration does not prove that the ADSP, WCD9335 codec,
    AMIC4 capture route, QUAT MI2S speaker route, or external amplifier are
    electrically functional.
  - The recovery userspace audio commit remains separate and is meaningful
    only after `/proc/asound/cards` and `/dev/snd/pcm*` appear.

Static verification: after correcting the first DTC syntax failure, the
final kernel integration worktree is clean; its OP3 DTS
contains only q6asm `dai@0`--`dai@2` and disables MM4--MM16 in the board DTS.
No kernel build or post-patch device test was run by this agent.

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
