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
