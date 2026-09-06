# OP3 recovery kernel haptics handoff

```text
Task / GitHub Issue: pending owner issue for recovery kernel haptics
Role: Implementation
Baseline commit: 67b0bbc3cbf46bae712a2606a43361756fcbd829
Working branch: agent/implementation/recovery-browser-001
Changed files: patches/pmos612-op3-haptics/0001-*.patch,
  patches/pmos612-op3-haptics/0002-*.patch, patches/pmos612-op3-haptics/README.md
Commit SHA: d31f471

Layer: Linux 6.12 PM8994 haptics driver and OnePlus 3 DTS
Hypothesis tested: The existing Qualcomm SPMI haptics path can expose and
drive the OnePlus 3 ERM motor when the driver accepts ERM and the existing
pmi8994_haptics node is enabled.
Only variable changed: PM8994 haptics ERM acceptance and OP3 DTS enablement;
no recovery userspace, physical-key, audio, Wi-Fi, DRM, or browser changes.

Build run by project owner: NOT_RUN
Build result: NOT_RUN
Artifacts and SHA256: none

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: none

Conclusion: INCONCLUSIVE
Uncertainties:
  - Historical downstream data identifies an ERM motor and approximately
    2700 mV, but the current mainline driver does not implement the old
    qcom,vmax-mv property; this series intentionally leaves voltage policy
    unchanged.
  - The 5 ms qcom,wave-play-rate-us value is a first experiment and must be
    verified on hardware. Registration and electrical output are separate
    questions.

Static verification: Both patches pass git apply --check against the active
6.12 checkout and the formal baseline, including whitespace checks. The
top-level git diff --check also passes. No kernel build or device test was
run by this agent.

Recommended next experiment: In the assigned formal kernel worktree, apply
the two patches from patches/pmos612-op3-haptics/, reuse the validated OP3
configuration with CONFIG_INPUT_QCOM_SPMI_HAPTICS=y, and build/boot the
resulting image. Confirm dmesg exposes the spmi_haptics input device and its
haptics IRQs, then inspect /tmp/fb.log for
vibration: input FF -> ... and verify a short startup or touch vibration.
Test this series independently from the volume/tri-state DTS patch. If the
input device registers but the motor is silent, open a separate voltage or
actuator-policy experiment rather than changing recovery userspace here.
```
