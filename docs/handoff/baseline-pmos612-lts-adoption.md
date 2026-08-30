# pmOS MSM8996 6.12.1 LTS baseline adoption

```text
Task / GitHub Issue: Owner-authorized baseline update
Role: Integration / documentation
Baseline commit: 65a63b64e19c1504522e596ba9dd884463cd0427
Working branch: agent/implementation/s6e3fa5-linux72-port
Changed files: governance files listed in commit
Commit SHA: pending

Layer: Project governance
Hypothesis tested: The pmOS MSM8996 v6.12.1 LTS line is the supported product
baseline for OnePlus 3, with complete device evidence through browser output.
Only variable changed: Project baseline metadata and default script paths.

Build run by project owner: NOT_RUN (documentation/default-path update only)
Build result: NOT_RUN
Artifacts and SHA256: OP3-BROWSER-003 browser image
`a1fb30f6dd2824f999430621be3c550420eb7a79de9f19e0ea10c9f2568d70bb`

Device test run by project owner: previously completed OP3-BROWSER-003
Device result: PASS — Weston plus Cog/WPE animated browser page, with 554-line
ACM boot-log capture
Evidence links / log paths: docs/test-matrix.md OP3-BROWSER-003;
artifacts/console-browser-retest-20260830.log

Conclusion: SUPPORTED
Uncertainties: Linux 7.x remains an unresolved shelved research line.
Recommended next experiment: v6.12.1 DTB GPU-regulator task, followed by the
separate U_SERIAL_CONSOLE kernel configuration task.
```

## Adopted immutable source reference

```text
tree: source/linux-pmos-msm8996-6.12
remote: https://gitlab.com/msm8996-mainline/linux.git
branch: msm8996-stable-6.12.y
commit: 67b0bbc3cbf46bae712a2606a43361756fcbd829
release: Linux 6.12.1-msm8996+
device: OnePlus 3 / MSM8996 / Samsung S6E3FA5
```

Linux 6.3.1 remains legacy evidence only. Linux 7.x is explicitly shelved;
it must not be selected by default scripts or resumed without a physical UART
plan and an owner-authorized `SHELVED-7X` task.
