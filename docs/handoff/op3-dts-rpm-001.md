# OP3-DTS-RPM-001 — pmOS 6.12 own-DTB RPM topology A/B

```text
Task / GitHub Issue: https://github.com/iamgreatwk/op3/issues/1
Role: Implementation
Baseline commit: pmOS MSM8996 6.12.1, 67b0bbc3cbf46bae712a2606a43361756fcbd829
Working branch: agent/implementation/op3-dts-rpm-001
Changed files: arch/arm64/boot/dts/qcom/msm8996.dtsi
Commit SHA: 91df7ccd284e5c62c5aed13c2738192b96c1f8dd

Layer: DTS / early boot
Hypothesis tested: The 6.12 RPM remoteproc/glink-edge wrapping, rather than
the direct rpm-glink topology, causes the OnePlus 3 own-DTB early-boot failure.
Only variable changed: The MSM8996 RPM node is restored to direct
rpm-glink/rpm-requests topology; all unrelated 6.12 DTS properties remain.

Build run by project owner: NOT_RUN
Build result: NOT_RUN
Artifacts and SHA256: NOT_RUN. The owner must record the 6.12 Image.gz,
6.12-built msm8996-oneplus3.dtb, fixed reference initramfs, and packed boot.img.

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: NOT_RUN

Conclusion: INCONCLUSIVE
Uncertainties: The preceding S6E3FA5 binding commit 88826709b54b is included
in this branch and has not independently received a device build/test. The
historical v74 DTB remains evidence only and must not be packed for this test.
Recommended next experiment: Owner builds this exact source commit, packs its
own DTB with the fixed initramfs/profile, and tests fastboot boot plus ACM/RNDIS,
UFS/root filesystem, and panel state.
```

## Static review

No kernel build or device action was run by the agent. `git diff --check` is
clean. The change removes only `qcom,msm8996-rpm-proc` / `qcom,rpm-proc`, the
intermediate `glink-edge` node, and `qcom,glink-smd-rpm`; it retains the RPM
request, RPM clock-controller, and RPM power-domain nodes and their labels.
