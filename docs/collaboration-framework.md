# Multi-agent collaboration framework

## Purpose

GitHub is the project record for source, decisions, Issues, pull requests, and evidence references. A conversational claim is never a project fact until it is tied to a commit and, where applicable, a reproducible artifact and OnePlus 3 result.

## Roles

| Role | May do | Must not do |
| --- | --- | --- |
| Project owner / operator | Run large builds, flash/test hardware, approve remote settings | Delegate hardware evidence to an unsupported claim |
| Implementation agent | Make one scoped source/config/script change on its assigned branch | Compile large targets, change unrelated subsystems, accept its own result |
| Review agent | Review diff and test design; submit review/report | Modify integration branches or replace device evidence |
| Research agent | Produce cited upstream/history reports | Modify the current device baseline or treat a source reference as proof |
| Integration agent | Verify gate, choose allowed merge/cherry-pick, update status/milestones | Merge unreviewed or untested multi-variable work |

## Branch ownership

```text
main                 accepted, device-validated states only
bringup              current approved integration line
upstream-7.2         unchanged upstream reference metadata
userspace-baseline   isolated reproducible userspace work
agent/<role>/<topic> one Issue, one hypothesis, one owner
test/<topic>         one explicit A/B experiment
legacy/*             read-only historical reference
```

Never force-push shared branches. Agents merge or cherry-pick only explicit commit SHA values. `main` must be protected in GitHub and updated through a pull request.

## Stage gate

The Integration role maintains exactly one state per layer: `NOT_STARTED`, `IN_PROGRESS`, `PASS`, `FAIL`, or `BLOCKED`.

No later layer may enter `bringup` before its prerequisite is `PASS`. Research may proceed in parallel, but research commits cannot alter the integration baseline.

```text
Debug infrastructure → boot → UFS/USB/shell → upstream DTS verification
→ DRM RGB → A530 EGL → runtime PM A/B if needed → no-preempt A/B if needed
→ Weston SHM → Weston EGL → Cog → WPE RGB → CSS/text → browser
```

## Evidence priority

1. Repeatable OnePlus 3 A/B result
2. Device logs and hardware state
3. Commit plus artifact SHA256
4. Upstream source
5. Upstream commit, issue, or mailing-list evidence
6. Static review
7. Agent hypothesis

Lower-priority evidence never overrides higher-priority evidence.

## Task lifecycle

```text
GitHub Issue → assigned agent branch → scoped commit(s) → owner-run build and SHA256
→ independent review → owner-run OnePlus 3 test → PASS/FAIL report
→ Integration decision → merge/cherry-pick or retain failed evidence
```

Every Issue must state baseline commit, layer, known-good milestone, hypothesis, only variable, expected PASS/FAIL, assigned roles, and required evidence.

## Commit and build loop

One task owns one hypothesis, not one build attempt. An owner-run build is
always against a committed state:

```text
agent makes one minimal change → agent commits → owner builds that commit
→ PASS or FAIL → agent analyzes result → next minimal commit only if needed
```

Do not create a commit for a repeated build with no file change. Do create a
new commit for each independently explainable fix. Keep the same task through
repeated build/fix cycles while the hypothesis and layer remain unchanged.
Start a new task only when the layer or sole hypothesis changes, or when the
current task has reached a stable PASS/FAIL/INCONCLUSIVE conclusion.

At a stable checkpoint, the agent updates its handoff and commits it. An agent
may push only its assigned `agent/*` or `test/*` branch and only with owner
authorization. `main` and `bringup` remain Integration-controlled.

## Prohibited actions

- Combining DTS, GPU, and userspace changes in one experiment.
- Quietly changing a test condition to make it pass.
- Deleting failure evidence or overwriting an artifact without a new SHA256.
- Treating a pmOS patch or another agent's confidence as proof.
- Starting a large build or flashing a device as an agent.
- Merging WPE work before the preceding graphics gates pass.

## Required records

- `docs/handoff/latest.md`: current project state only.
- `docs/test-matrix.md`: one row per owner-run device test.
- `docs/decisions.md`: accepted process and baseline decisions.
- `docs/handoff/archive/`: completed handoffs.
- GitHub Issue and pull request: task intent, review, and links to evidence.
