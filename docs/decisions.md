# Decisions

## 2026-08-26 — Adopt Linux 7.2 clean-rebuild baseline

The default target is Linux v7.2 pristine upstream. The pmOS MSM8996 6.3.1
tree is retained solely as legacy evidence and may not be built or patched
unless the task explicitly permits legacy work.

## 2026-08-26 — Compilation boundary

The project owner runs every kernel and large userspace compilation. Agent work
may prepare and review source, configuration, scripts, commands, and reports,
but may not start those builds.
