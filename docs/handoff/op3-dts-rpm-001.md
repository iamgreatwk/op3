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

Build run by project owner: 2026-08-30
Build result: PASS
Artifacts and SHA256:
- config: 2ebb875b6ed1694e91f51d078f6cb8d17e7c8a23850bc0252d74526de01e45b3
- Image.gz: 088d472f90f90388ee90426f190806f721e3e483777c3dfdc9992a4b1321a7ad
- 6.12-built DTB: cb29ab658135cd0cfcde3b47c1e115b763f5dbd37b724554590b7a61afcbf32f
- fixed initramfs: c3358a1cadb747996ddaa492e636827f2d72974040e8fd40d81f8a213e676366
- boot image: 58bd7b38949d923643f3533cb1e52385609608818e7a096f36ce76a153d340c9
  (`artifacts/boot-oneplus3-pmos612-rpm-glink-own-dtb.img`)

Device test run by project owner: 2026-08-30, `fastboot boot`
Device result: PASS for the issue boot/DTS gate. Recovery program, RNDIS, and
SSH were available. Remote read-only inspection confirmed Linux
`6.12.1-msm8996+`, runtime DTB `rpm-glink` compatible `qcom,glink-rpm`, no
`/proc/device-tree/rpm` remoteproc node, DSI-1 `connected`, and UFS `sda`
with `sda15` mounted read-write at `/newroot`.
Evidence links / log paths: Owner report plus read-only SSH evidence captured
in the Codex task on 2026-08-30; relevant dmesg includes UFS SCSI host 0 and
MSM DRM/DSI binding.

Conclusion: SUPPORTED. The pmOS 6.12 tree-generated DTB with only the RPM
topology change boots the OnePlus 3; the historical v74 DTB was not packed.
Uncertainties: The preceding S6E3FA5 binding commit 88826709b54b is included
in this branch and has not independently received a device build/test. The
historical v74 DTB remains evidence only and must not be packed for this test.
Recommended next experiment: Create a separate initramfs-provenance task. It
must replace the historical reference archive with a reproducibly generated
archive while holding this now-validated Image.gz and DTB fixed.
```

## Static review

No kernel build or device action was run by the agent. `git diff --check` is
clean. The change removes only `qcom,msm8996-rpm-proc` / `qcom,rpm-proc`, the
intermediate `glink-edge` node, and `qcom,glink-smd-rpm`; it retains the RPM
request, RPM clock-controller, and RPM power-domain nodes and their labels.
