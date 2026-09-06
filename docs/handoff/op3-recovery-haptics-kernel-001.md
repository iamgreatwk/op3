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

Build run by project owner: 2026-09-06
Build result: PASS (owner reports the combined formal 6.12 kernel build
completed)
Artifacts and SHA256: owner did not provide artifact hashes in the test
report

Device test run by project owner: 2026-09-06
Device result: PASS
Evidence links / log paths: `/proc/bus/input/devices`, `dmesg`, and
`/tmp/fb.log` from `root@172.16.42.1`. The kernel exposes
`spmi_haptics` on `/dev/input/event0` with `EV_FF`; dmesg registers the
device, and recovery reports `vibration: input FF -> /dev/input/event0
name=spmi_haptics effect=0`. The owner reports that the vibration motor
works physically.

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

Recommended next experiment: Integration should record this device PASS in
the aggregate recovery milestone after retaining the exact kernel artifact
hash and test log. No voltage-policy follow-up is required for the reported
hardware behavior. Keep the existing `qcom,vmax-mv` omission documented if a
different motor or board variant is tested.
```
