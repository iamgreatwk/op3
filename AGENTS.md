# Agent entry contract

This file is mandatory reading for every human or AI agent before making a change. Read it together with `BASELINE.env` and `docs/handoff/latest.md`.

## Non-negotiable boundaries

1. The formal baseline is Linux v7.2 pristine upstream. Linux 6.3.1 pmOS is legacy evidence only; do not build or patch it without an explicit `LEGACY-6.3.1` task.
2. The project owner alone runs kernel, Buildroot, Mesa, WebKit, WPE, and other large compilations. Agents may prepare source, configuration, scripts, and exact commands, but must not start those builds.
3. One task has one layer, one falsifiable hypothesis, and one changed variable. Do not mix kernel, DTS, DRM/GPU, and userspace changes.
4. Do not run device flashing, destructive device actions, or publish releases without explicit owner authorization.
5. Do not modify another agent's branch. Work only on `agent/<role>/<topic>` or the branch assigned in the GitHub Issue.
6. No agent may declare a fix accepted. Only the Integration role may promote an evidence-backed result after the owner reports the OnePlus 3 test.

## Required startup sequence

Run `./scripts/agent-start.sh`, then read the assigned GitHub Issue. Confirm the baseline commit, prior PASS milestone, active layer, sole hypothesis, sole variable, and PASS/FAIL condition. If one is missing, stop and request clarification rather than changing code.

## Required completion record

Use `docs/templates/agent-handoff.md` in the Issue or pull request. A code change also requires an updated test report or a statement that no device test occurred.

Detailed rules: `docs/collaboration-framework.md`.
