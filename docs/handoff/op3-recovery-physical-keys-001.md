# OP3 recovery physical-key handoff

```text
Task / GitHub Issue: pending owner issue for recovery physical keys
Role: Implementation
Baseline commit: 67b0bbc3cbf46bae712a2606a43361756fcbd829
Working branch: agent/implementation/recovery-browser-001
Changed files: patches/pmos612-op3-physical-keys/0001-*.patch,
patches/pmos612-op3-physical-keys/README.md
Commit SHA: 6f58544

Layer: Linux 6.12 OnePlus 3 DTS input exposure
Hypothesis tested: The existing formal gpio-keys driver can expose the
OnePlus 3 volume buttons and three-position switch as the EV_KEY codes that
recovery already consumes.
Only variable changed: OP3 DTS registration of PMIC GPIO input keys; no
recovery userspace, haptics, audio, Wi-Fi, DRM, or browser changes.

Build run by project owner: NOT_RUN
Build result: NOT_RUN
Artifacts and SHA256: not generated

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: historical OP3 DTS and archived mainline
diagnostics under _mainline_test/; no test of this patch on the current
formal 6.12 image.

Conclusion: INCONCLUSIVE
Uncertainties:
  - The formal 6.12 kernel currently has CONFIG_KEYBOARD_GPIO=y but its OP3
    DTS does not register GPIO2/3/4/5/6 for these controls.
  - The legacy tri-state driver source is absent from the archive. This patch
    uses the documented low-active GPIO behavior and emits codes 600/601/602
    through gpio-keys; physical testing must confirm the top/middle/bottom
    correspondence.
  - The haptics path is intentionally excluded and has its own handoff.

Static verification: `git apply --check` passed against the formal baseline
commit and the active formal 6.12 checkout; `git diff --check` passed.

Recommended next experiment: Apply the patch in the assigned formal kernel
worktree, reuse the validated OP3 6.12 config containing
CONFIG_KEYBOARD_GPIO=y and the S1302 capkey option, then let the owner build
and boot the resulting kernel/DTB. Verify `/proc/bus/input/devices` contains
`gpio-keys`, and `/tmp/fb.log` reports `volume` and `tri-state` recovery fds.
Press and release both volume keys and move the switch through all three
positions; record the resulting 115/114 and 600/601/602 events before
testing the separate haptics patch.
```
