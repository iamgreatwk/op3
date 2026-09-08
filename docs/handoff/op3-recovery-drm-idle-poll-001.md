# Agent handoff: recovery DRM idle wakeups

Task / GitHub Issue: DRM recovery idle wakeup reduction (follow-up)
Role: Implementation
Baseline commit: `44b3e2749fadae3b40859ad2814b9ee0127b5df8` top-level project
Working branch: `agent/implementation/recovery-browser-001`
Changed files: `recovery/recovery_mainline.c`
Commit SHA: `e11a9dc`

Layer: Recovery userspace main loop
Hypothesis tested: A 20 ms empty `poll()` timeout causes unnecessary recovery
wakeups while the screen is on and no event is pending; increasing it lowers
idle CPU activity without delaying input because actual input still wakes
`poll()` immediately.
Only variable changed: Screen-on idle `poll()` timeout, 20 ms -> 100 ms.

Build run by project owner: YES
Build result: PASS
Artifacts and SHA256:

- Kernel `Image.gz`: `5c89259d9340071c9c8684d361042482ca25c8f76b2de4d33cdacf2105f78861`
- Kernel DTB: `264f981678c1dd8d1d9a52f2db6e2130a0ebccbb9f4485ab8740784f73806db7`
- Buildroot initramfs: `27736d1d662158bedd5170c033f9b73d807d559a564d3fd6c9090438b6d0f968`
- Boot image: `fcd5e6bd476467e09abfbcbea3dcafd206b0ae98e60a17165a697fd87f4239c5`

Device test run by project owner: NOT_RUN for this commit; deferred while the
phone is charging
Device result: PENDING
Evidence links / log paths: Prior device sample recorded about 0.9% recovery
CPU over 30 s and about 1,535 voluntary context switches over 30 s; GPU was
runtime-suspended during screen-on and screen-off idle samples. This does not
yet isolate active DRM refresh cost while the UI is changing.

Conclusion: INCONCLUSIVE
Uncertainties: The measured idle thermal state was on a charging phone and did
not include a sustained full-screen redraw. The DRM backend remains direct CPU
rendering to a KMS dumb buffer; no Mesa/EGL/GLES path was added.
Recommended next experiment: Owner rebuild the Buildroot initramfs and boot
image, then compare recovery `voluntary_ctxt_switches`, process CPU time,
thermal zones, and DRM/MDSS interrupt deltas with the previous image. Verify
touch, physical keys, and PTY input remain responsive.
