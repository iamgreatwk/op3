# Git workflow

## Source of truth

The configured GitHub remote is the durable project record. Local `out/`,
`artifacts/`, `cache/`, and `diag_archive/` are not source of truth and must
not be committed. Every device result records the tested commit and artifact
SHA256 in `docs/test-matrix.md`.

## Branches

| Branch | Purpose | Merge rule |
| --- | --- | --- |
| `main` | States accepted after device validation | Merge request only; protect remotely |
| `upstream-7.2` | Unmodified Linux v7.2 source reference | No device patch commits |
| `bringup` | Current, single-layer device bring-up | One hypothesis per commit |
| `userspace-baseline` | Reproducible Buildroot/userspace work | Separate from kernel bring-up |
| `test/*` | One isolated A/B experiment | Cherry-pick only after a recorded result |
| `legacy/*` | Historical evidence only | Never use as a default build baseline |

## Change rules

1. Start every change by recording its layer, hypothesis, one variable, and
   PASS/FAIL condition in the commit message or an associated test report.
2. A commit changes one logical thing. Never mix DTS, GPU, userspace, and
   diagnostics in one commit.
3. Do not commit generated kernels, DTBs, rootfs images, Buildroot output, or
   logs. Commit their SHA256 and the test report instead.
4. Do not build or patch Linux 6.3.1 unless the task is explicitly legacy.
5. Do not push directly to protected `main`; use a GitLab merge request after
   review and device evidence.

## Required test-report fields

```text
Commit:
Branch:
Layer:
Hypothesis:
Only variable changed:
Build command executed by:
Artifact SHA256:
Device command:
PASS/FAIL:
Relevant logs:
Next action:
```

## Initial remote setup

After the first local governance commit, push all branches to the configured
GitHub repository. In GitHub, protect `main` and require pull requests. Do not
create an arbitrary remote or invent project ownership.
