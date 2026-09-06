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
| `baseline-pmos-6.12` | pmOS MSM8996 v6.12.1 LTS baseline reference | No unrelated device patch commits |
| `shelved-7x` | Archived Linux 7.x research | No default builds; resume only with UART-backed task |
| `bringup` | Current, single-layer device bring-up | One hypothesis per commit |
| `userspace-baseline` | Reproducible Buildroot/userspace work | Separate from kernel bring-up |
| `test/*` | One isolated A/B experiment | Cherry-pick only after a recorded result |
| `legacy/*` | Historical evidence only | Never use as a default build baseline |

## Current active two-repository lines

The project has two independent Git repositories. The top-level repository
handles recovery/userspace integration, while the nested formal kernel
repository handles Linux source changes. The current test integration lines
are:

```text
Top-level: /home/kai/src/oneplus3-mainline
  agent/implementation/recovery-browser-001

Kernel:    /home/kai/src/oneplus3-mainline/source/linux-pmos-msm8996-6.12-recovery-audio-full
  agent/implementation/recovery-browser-audio-full-001
```

The kernel baseline remains `msm8996-stable-6.12.y` at the commit pinned in
`BASELINE.env`; the GitHub default branch is not the kernel baseline. The
other `agent/implementation/*` branches are isolated or historical Issue
lines and must not be modified or selected without an explicit assignment.
The `legacy/*` and shelved 7.x lines are evidence/research only.

Before changing state, inspect both repositories:

```bash
git status --short --branch
git branch -vv
git worktree list
git -C source/linux-pmos-msm8996-6.12-recovery-audio-full status --short --branch
git -C source/linux-pmos-msm8996-6.12-recovery-audio-full worktree list
```

Keep each owner build in a distinct `out/` directory named for the kernel
line and experiment. Never delete a branch with an attached worktree; remove
the exact worktree first and then use `git branch -d`, never `-D`.

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
