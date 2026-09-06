# pmOS 6.12 OnePlus 3 audio-card registration handoff

The previously generated q6asm-enumeration patch in this directory was
rejected and has been removed. Linux 6.12 exposes only the valid q6asm
frontend sessions, so adding child nodes for IDs 8 through 15 makes q6asm
DAI registration fail and leaves the phone with no ALSA card.

The final kernel integration branch is based on the already validated audio
branch and includes the physical-key and haptics commits:

```text
Final kernel integration worktree:
/home/kai/src/oneplus3-mainline/source/linux-pmos-msm8996-6.12-recovery-audio-full
Kernel branch:
agent/implementation/recovery-browser-audio-full-001
Kernel HEAD:
9491be0d6460
```

The audio branch already contains the required OP3 audio-card fixes,
including valid MM1--MM3 q6asm registration, disabling unavailable MM4--MM16
links, WCD9335 AMIC4 capture setup, and the previously device-validated
capture accounting changes. The prepared branch also contains the S1302
capacitive-key, volume/tri-state-key, and PM8994 haptics commits.

The owner should compile this final integration worktree and collect `/proc/asound/cards`,
`/proc/asound/pcm`, `/dev/snd`, and audio dmesg before testing recovery
`tinycap`/`tinyplay`.

The first owner build stopped in DTC because the physical-key merge was
missing the closing brace for the audio `&soc` node. Commits `9f81c0cd4289`
and `9491be0d6460` repair that nesting and indentation; compile the current
HEAD.
