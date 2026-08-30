# pmOS 6.12 OnePlus 3 own-DTB patch series

This directory is the project-owned mirror of the source-tree changes used by
OP3-BOOT-042. It exists because the pmOS upstream GitLab remote is read-only
for this project: do not attempt to push these commits there.

## Base and order

Apply this two-patch series to the pmOS MSM8996 v6.12.1 source commit
`67b0bbc3cbf46bae712a2606a43361756fcbd829`, in lexical/numeric order:

```bash
git am --3way patches/pmos612-op3-own-dtb/0001-*.patch \
  patches/pmos612-op3-own-dtb/0002-*.patch
```

| Patch | Original source commit | SHA256 | Purpose |
| --- | --- | --- | --- |
| `0001-arm64-dts-qcom-msm8996-oneplus3-select-S6E3FA5-panel.patch` | `88826709b54bf6ba55b23cbb147d49e8ad9bd008` | `1b602b779abd1f31a62845092e80c753942fef2ff0d5d5ff491487c091aceefa` | Select the installed Samsung S6E3FA5 panel. |
| `0002-arm64-dts-qcom-msm8996-restore-direct-RPM-GLINK-topo.patch` | `91df7ccd284e5c62c5aed13c2738192b96c1f8dd` | `b9779554f4632df473b09887390129cf636fa0e68e00f10cb469a6f68f80b54d` | Restore direct `rpm-glink → rpm-requests` topology. |

The second patch alone is not a complete OP3-BOOT-042 reproduction: it relies
on the panel binding introduced by the first patch. These are source inputs,
not proof of device acceptance; device evidence is in
`docs/handoff/op3-dts-rpm-001.md` and row OP3-BOOT-042 in the test matrix.
