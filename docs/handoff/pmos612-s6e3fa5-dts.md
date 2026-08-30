# pmOS MSM8996 6.12.1 OnePlus 3 S6E3FA5 DTS selection

```text
Task / GitHub Issue: Owner-authorized OnePlus 3 panel-compatible correction
Role: Implementation
Formal product baseline: pmOS MSM8996 Linux v6.12.1 LTS
Baseline source commit: 67b0bbc3cbf46bae712a2606a43361756fcbd829
Working source branch: agent/implementation/pmos612-s6e3fa5-dts
Changed source file: arch/arm64/boot/dts/qcom/msm8996-oneplus3.dts
Source commit SHA: 88826709b54bf6ba55b23cbb147d49e8ad9bd008

Layer: DTS / panel binding
Hypothesis tested: The OnePlus 3 DTS must identify the installed Samsung
S6E3FA5 panel rather than S6E3FA3.
Only variable changed: panel node compatible string, plus its explanatory
comment.

Build run by project owner: NOT_RUN
Build result: NOT_RUN
Artifacts and SHA256: none; a fresh Image.gz and DTB must be recorded by the
owner after the source commit is built.

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: prior device identification confirms FA5; the
existing browser PASS used the v74 reference DTB, so it does not validate this
tree-built DTS.

Conclusion: INCONCLUSIVE pending owner build and OnePlus 3 test
Uncertainties: The pmOS 6.12 source must contain/build the matching S6E3FA5
panel driver; this commit only corrects the device binding.
Recommended next experiment: Owner builds this exact source commit, packages
its own msm8996-oneplus3.dtb with the fixed control inputs, and verifies boot,
panel probe, and the browser regression gate.
```

## Delta

```diff
&panel {
-       compatible = "samsung,s6e3fa3";
+       compatible = "samsung,s6e3fa5";
};
```

No regulator, clock, GPU, DRM/MSM, runtime-PM, cmdline, boot image, or
userspace change is included.
