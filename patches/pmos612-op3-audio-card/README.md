# pmOS 6.12 OnePlus 3 audio-card registration handoff

The previously generated q6asm-enumeration patch in this directory was
rejected and has been removed. Linux 6.12 exposes only the valid q6asm
frontend sessions, so adding child nodes for IDs 8 through 15 makes q6asm
DAI registration fail and leaves the phone with no ALSA card.

Use the already validated audio branch as the kernel base, then add the
physical-key and haptics commits in the separate prepared worktree:

```text
Kernel worktree:
/home/kai/src/oneplus3-mainline/source/linux-pmos-msm8996-6.12-recovery-audio-full
Kernel branch:
agent/implementation/recovery-browser-audio-full-001
Kernel HEAD:
aa2eafa8c662
```

The audio branch already contains the required OP3 audio-card fixes,
including valid MM1--MM3 q6asm registration, disabling unavailable MM4--MM16
links, WCD9335 AMIC4 capture setup, and the previously device-validated
capture accounting changes. The prepared branch also contains the S1302
capacitive-key, volume/tri-state-key, and PM8994 haptics commits.

The owner should compile this worktree and collect `/proc/asound/cards`,
`/proc/asound/pcm`, `/dev/snd`, and audio dmesg before testing recovery
`tinycap`/`tinyplay`.
