# OP3 recovery ALSA sound-card registration handoff

```text
Task / GitHub Issue: pending owner issue for recovery kernel audio card
Role: Implementation
Baseline commit: 67b0bbc3cbf46bae712a2606a43361756fcbd829
Working branch: agent/implementation/recovery-browser-001
Changed files: patches/pmos612-op3-audio-card/0001-*.patch,
  patches/pmos612-op3-audio-card/README.md
Commit SHA: 228b319

Layer: Linux 6.12 OnePlus 3 audio DTS DAI enumeration
Hypothesis tested: The current OP3 sound card fails because its DT declares
MultiMedia1..16 links while q6asmdai enables only IDs 0..2. Enabling the
remaining referenced q6asm frontend IDs should allow the sound card to
register and create ALSA PCM devices.
Only variable changed: q6asmdai child-DT enumeration; no recovery userspace,
codec controls, audio routing, Wi-Fi, DRM, input, or haptics changes.

Build run by project owner: NOT_RUN
Build result: NOT_RUN
Artifacts and SHA256: No kernel artifact was built. The patch passes
git apply --check against both the active 6.12 checkout and the formal
baseline. The owner must build the resulting kernel/DTB.

Device test run by project owner: NOT_RUN for commit 228b319
Device result: NOT_RUN for the patch
Evidence links / log paths: Pre-patch device evidence from
root@172.16.42.1 shows `/dev/snd` contains only `timer`,
`/proc/asound/cards` reports `--- no soundcards ---`, and dmesg reports:
`msm-snd-apq8096 sound: error -EINVAL: MultiMedia4: error getting cpu dai
name` followed by `probe ... failed with error -22`. `tinycap` therefore
reports that card 0/device 0 does not exist and no `/tmp/voice.wav` is made.

Conclusion: INCONCLUSIVE
Uncertainties:
  - This patch tests sound-card registration only; it does not prove that
    the ADSP, WCD9335 codec, AMIC4 capture route, QUAT MI2S speaker route,
    or external amplifier are electrically functional.
  - The recovery userspace audio commit remains separate and will only be
    meaningful after `/proc/asound/cards` and `/dev/snd/pcm*` appear.

Static verification: The patch passes `git diff --check` and
`git apply --check` in the active checkout and a clean archive of the formal
6.12.1 baseline. No kernel build or post-patch device test was run by this
agent.

Recommended next experiment: The owner should apply this one patch to the
private formal kernel worktree, reuse the validated OP3 configuration, build
and boot the resulting image, then collect `/proc/asound/cards`,
`/proc/asound/pcm`, `/dev/snd`, and the audio dmesg. If an ALSA card appears,
test `tinycap` first and then `tinyplay`; only after those pass should the
recovery microphone-key workflow be evaluated.
```
