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

Build run by project owner: 2026-09-06
Build result: PASS (owner reports the combined formal 6.12 kernel build
completed)
Artifacts and SHA256: owner did not provide artifact hashes in the test
report

Device test run by project owner: 2026-09-06
Device result: PASS
Evidence links / log paths: `/proc/bus/input/devices`, `dmesg`, and
`/tmp/fb.log` from `root@172.16.42.1`. The current image exposes
`gpio-keys` on `/dev/input/event4`; recovery opens `tri=8` and `volume=9`.
The owner reports that volume up/down and all three tri-state positions work
physically. The recovery log reports the expected key events.

Conclusion: INCONCLUSIVE
Uncertainties:
  - The legacy tri-state driver source is absent from the archive. The
    standard gpio-keys implementation emits codes 600/601/602 and the owner
    has confirmed the physical positions.
  - The haptics path is intentionally excluded and has its own handoff.

Static verification: `git apply --check` passed against the formal baseline
commit and the active formal 6.12 checkout; `git diff --check` passed.

Recommended next experiment: Integration should record this device PASS in
the aggregate recovery milestone after retaining the exact kernel artifact
hash and test log. The physical-key scope itself requires no further code
change. Keep any follow-up focused on switch repeat-policy or key behavior,
not on the DTS registration already validated here.
```
