# Agent entry contract

This file is mandatory reading for every human or AI agent before making a change. Read it together with `BASELINE.env` and `docs/handoff/latest.md`.

## Non-negotiable boundaries

1. The formal baseline is the pmOS MSM8996 Linux v6.12.1 LTS line, pinned in
   `BASELINE.env`. Linux 6.3.1 pmOS is legacy evidence only; do not build or
   patch it without an explicit `LEGACY-6.3.1` task. The Linux 7.x line is
   shelved research, not a default build or patch target; require an explicit
   `SHELVED-7X` task and a physical-UART evidence plan before resuming it.
2. The project owner alone runs kernel, Buildroot, Mesa, WebKit, WPE, and other large compilations. Agents may prepare source, configuration, scripts, and exact commands, but must not start those builds.
3. One task has one layer, one falsifiable hypothesis, and one changed variable. Do not mix kernel, DTS, DRM/GPU, and userspace changes.
4. Do not run device flashing, destructive device actions, or publish releases without explicit owner authorization.
5. Do not modify another agent's branch. Work only on `agent/<role>/<topic>` or the branch assigned in the GitHub Issue.
6. No agent may declare a fix accepted. Only the Integration role may promote an evidence-backed result after the owner reports the OnePlus 3 test.

## Branch and worktree management

These rules are mandatory because this project contains a top-level project
repository and a separate Git repository for the formal kernel source.

1. Treat the two repositories independently. The top-level repository at
   `/home/kai/src/oneplus3-mainline` contains recovery userspace, initramfs,
   browser/Wi-Fi/audio helpers, artifacts metadata, and handoff documents.
   The nested kernel repository contains only the formal Linux source and its
   kernel commits. A branch name in one repository does not select or modify a
   branch in the other repository.
2. At the current integration checkpoint, use these active branches:
   - top-level project: `agent/implementation/recovery-browser-001`
   - formal kernel worktree:
     `source/linux-pmos-msm8996-6.12-recovery-audio-full` on
     `agent/implementation/recovery-browser-audio-full-001`
   The latter is the combined audio, physical-key, haptics, and S1302 kernel
   line. Build the kernel only from that worktree unless the assigned Issue
   explicitly changes the target.
3. The formal kernel baseline is always the commit and branch pinned in
   `BASELINE.env` (`msm8996-stable-6.12.y` at the pinned 6.12.1 commit). The
   GitHub default branch is a project-repository convenience and must never be
   mistaken for the kernel baseline.
4. Classify branches before using them:
   - `agent/implementation/*`: one Issue or one isolated implementation line;
     do not mix layers or silently reuse another agent's branch.
   - `agent/integration/*`: integration evidence lines; use only when assigned
     or explicitly selected by the owner.
   - `main`/`bringup`: protected project baselines; agents do not commit there.
   - `legacy/*`: historical evidence only; never a default build target.
   - `upstream-7.2` and other 7.x lines: shelved research; require the explicit
     `SHELVED-7X` task and UART evidence plan.
5. Before switching or creating a branch, inspect both repository states:
   `git status --short --branch`, `git branch -vv`, and `git worktree list`.
   Never switch, commit, reset, or delete another agent's worktree or branch.
   The owner may compile only a clean, committed branch.
6. Keep build outputs separate from source branches. Use an output directory
   named for the target line and experiment, for example
   `out/pmos-msm8996-6.12-recovery-audio-full-s1302-retry`, and record the
   tested commit and artifact SHA256 in the handoff/test report. `out/`,
   `artifacts/`, and logs are not branches or source of truth.
7. Do not delete a branch that has a worktree. First inspect
   `git worktree list`; after explicit confirmation, remove only the exact old
   worktree and then use `git branch -d old-branch`. Never use `git branch -D`,
   broad recursive deletion, force-push, or destructive checkout/reset as a
   cleanup shortcut.
8. At a stable checkpoint, push only the assigned branch when the owner has
   authorized pushing. For a nested kernel branch, a top-level GitHub push
   does not publish the kernel commits; record the local kernel path/commit and
   any remote-credential limitation in the handoff.
9. For the current integrated recovery rebuild, use
   `manifests/op3-recovery-audio-full.env` as the only source/artifact lock.
   Run `scripts/verify-op3-recovery-manifest.sh --source` before the owner
   builds and `scripts/verify-op3-recovery-manifest.sh --artifacts` after
   packaging. If a source, branch, commit, or hash changes, update the
   manifest and handoff in the same assigned branch before building.

Recommended state check:

```bash
cd /home/kai/src/oneplus3-mainline
git fetch --prune origin
git remote set-head origin -a
git status --short --branch
git branch -vv
git worktree list

git -C source/linux-pmos-msm8996-6.12-recovery-audio-full \
  status --short --branch
git -C source/linux-pmos-msm8996-6.12-recovery-audio-full \
  branch -vv
git -C source/linux-pmos-msm8996-6.12-recovery-audio-full \
  worktree list
```

## Commit and checkpoint rules

1. Every source, configuration, or script change must be committed before the project owner builds it. The owner never builds uncommitted work.
2. Create one commit per independently explainable change. Do not commit merely because a repeated build was attempted without changing files.
3. A build failure followed by one minimal fix creates one new commit; the owner then builds that new commit. Repeat within the same task while the hypothesis remains the same.
4. Before a task pauses for an owner-run build, or reaches a conclusion, update its handoff and commit the documentation.
5. Agents commit only to their assigned `agent/*` or `test/*` branch. They do not commit directly to `main` or `bringup`.
6. Push the assigned branch at a stable checkpoint (for example, ready for owner build or task conclusion) when the owner has authorized push. Never force-push a shared branch.

## Required startup sequence

Run `./scripts/agent-start.sh`, then read the assigned GitHub Issue. Confirm the baseline commit, prior PASS milestone, active layer, sole hypothesis, sole variable, and PASS/FAIL condition. If one is missing, stop and request clarification rather than changing code.

## Required completion record

Use `docs/templates/agent-handoff.md` in the Issue or pull request. A code change also requires an updated test report or a statement that no device test occurred.

For every Codex subtask, start from `docs/templates/codex-subtask-prompt.md` and replace its bracketed fields. Detailed rules: `docs/collaboration-framework.md`.
