# Latest handoff

```text
PROJECT MODE: PRODUCT BASELINE
TARGET KERNEL: pmOS MSM8996 Linux 6.12.1 LTS
TARGET COMMIT: 67b0bbc3cbf46bae712a2606a43361756fcbd829
SHELVED RESEARCH: Linux 7.x (physical UART required to resume)
LEGACY KERNEL: Linux 6.3.1 pmOS-derived
DO NOT BUILD LEGACY OR SHELVED 7.x UNLESS EXPLICITLY REQUESTED
HOST TARGET: Ubuntu 26.04 LTS x86_64
DEVICE: OnePlus 3 / MSM8996
```

## Historical 7.x investigation (2026-08-29) — shelved

**All 6.x pmOS kernels boot with the v74 DTB (compiled from 6.3.1-v74full);
7.2 does not boot even with the v74 DTB.**

Test matrix (2026-08-29):
- OP3-BOOT-035: pmOS **v6.16.12 stable** + strict v74 + own DTB → FAIL
- OP3-BOOT-036: pmOS v6.16.12 stable + strict v74 + **v74 DTB** → **PASS**
- OP3-BOOT-037: pmOS **v6.19.5** + strict v74 (UFS=y) + **v74 DTB** → **PASS**
- OP3-BOOT-038: Linux **7.2** + strict v74 + **v74 DTB** → FAIL
- OP3-BOOT-039: Linux 7.2 + strict v74 + v74 DTB + no **LSUI** → FAIL

**Root cause narrowed:**
- The device boots **any 6.x kernel when given the 6.3.1-compiled v74 DTB**.
  6.12/6.16/6.19 all fail with their *own* DTB but boot with v74 DTB.
  This explains all earlier "6.12 boots, 6.16/7.2 fail" confusion: 6.12's
  "boot" image had been packed with the v74 DTB.
- 6.19.5's earlier fastboot failure was additionally caused by
  `CONFIG_SCSI_UFS_QCOM=m`; v74 full config forces `=y`.
- **7.2 fails even with v74 DTB** → 7.2 has an independent kernel-code/config
  issue. Disabling the only 7.2-unique ARMv9 feature `CONFIG_ARM64_LSUI`
  (unsupported on A53) did NOT fix it.

## Next action (7.2, unresolved) — SUPERSEDED 2026-08-30: 7.x shelved, baseline is 6.12.1 (see below)

1. Get early-boot log from 7.2 (USB ACM gadget or earlycon) to locate where it
   hangs — 7.2 is the only kernel failing with the v74 DTB.
2. Or diff msm8996-mainline 6.19.5 (boots) vs torvalds mainline 7.2
   (fails) early-boot code (head.S / clk-msm8996 / smem / pinctrl).
3. Parallel line: diff v74 DTB vs 6.16/6.19 own DTB to find the node that
   breaks their own DTB boot (candidate: new `qcom,rpm-proc` structure).

## DRM RGB gate PASS (2026-08-30, layer 04 parallel diagnostic)

The DRM dumb-buffer RGB gate passes on the device with the pmOS 6.12 control
kernel plus the v74 DTB:

```text
image:  artifacts/boot-oneplus3-pmos612-v74dtb-drm-test-60s.img
        f9693a7baf9e1fae5ff8ce27517ac6c246782576b8eb739093a84c690b7a3670
result: connector=33 crtc=106 mode=1080x1920@60, solid colour active, exit 0
owner:  red → green → blue → red, displayed in that order and correct
scope:  layer 04 DRM RGB only. Not EGL, GBM, Wayland, Weston, Cog, WPE, or GPU
        runtime PM, and not a Linux 7.2 acceptance result.
```

Two test-program defects were found and fixed on the way: `d43821a` (second-pass
array pointers in the enumeration ioctls) and `e261c4d` (the
`connector_status_connected` constant). Details are in
`docs/handoff/pmos612-drm-dumb-buffer.md`, rows `OP3-DRM-001` …
`OP3-DRM-005` in `docs/test-matrix.md`. Promoting the result to an accepted
milestone is the Integration role’s decision.

This does not change the Linux 7.2 line below.

## EGL gate PASS + ACM debug console (2026-08-30, layer 05)

**OP3-EGL-001 passes all three criteria** on the pmOS 6.12 control kernel plus
the v74 DTB:

```text
image:   artifacts/boot-oneplus3-pmos612-v74dtb-egl.img (284936e6…)
         bundle sda15:/opt/op3-egl (e7b1df23…), Mesa 26.0.1
result:  EGL 1.5 on /dev/dri/card0, GL renderer FD530 (freedreno hardware,
         not llvmpipe), OpenGL ES 3.1; kmscube 30 s windows, 1680 frames at
         59.8 fps, exit 0; owner confirmed the rotating cube repeatedly,
         including two image-only boots and three fastboot-boot runs
scope:   layer 05 EGL only. Not Wayland, Weston, Cog, WPE, and not a Linux 7.2
         acceptance result. Promotion is the Integration role's decision.
```

Fixed en route (details in `docs/handoff/pmos612-egl-gate.md`, row
`OP3-EGL-001` in `docs/test-matrix.md`):

- `run.sh` must not export `LD_LIBRARY_PATH` (broke busybox, SIGBUS in
  kmscube), and must feed kmscube stdin from a `sleep` pipe (kmscube treats
  any readable stdin as `user interrupted!` and exited after one frame).
- GPU runtime PM resume hard-resets the device (DTB has dummy GPU regulators);
  the launcher disables A530 runtime PM at boot as a workaround
  (`docs/known-issues.md`, DTB fix is a separate task).

**ACM debug console is now operational end to end**
(`boot-oneplus3-pmos612-v74dtb-egl-acm.img`, `65cac825…`):
the launcher fixes the stale `/dev/ttyGS0` placeholder node, relays
`/dev/kmsg` to it and spawns a debug shell; the host needs the udev rule
`scripts/99-op3-acm.rules` (group `dialout`) and `kai` in `dialout`. Verified:
live kernel log reaches `cat /dev/ttyACM0`, and commands typed on the ACM port
execute on the device (fallback channel when RNDIS/SSH is dead). Usage notes
and pitfalls in `docs/known-issues.md`. Root fix for a real `console=ttyGS0`
(panics, init output) needs a pmOS 6.12 kernel rebuild with
`CONFIG_U_SERIAL_CONSOLE=y` — recorded as a separate task.

This does not change the Linux 7.2 line below.

## Wayland gate PASS (2026-08-30, layer 06)

weston 14 composites on the panel with the GL renderer and an animated Wayland
client (`docs/handoff/pmos612-wayland-weston.md`, row `OP3-WAYLAND-001`):

```text
image:  artifacts/boot-oneplus3-pmos612-v74dtb-weston.img (ea6043cc…)
bundle: artifacts/op3-weston-bundle.tar.gz (0a8163cb…, whole Buildroot target)
result: weston DRM backend + GL renderer (FD530) on DSI-1 1080x1920@60;
        weston-simple-egl spinning triangle visible on the panel; owner
        confirmed; two runs + live session, zero assertions
scope:  layer 06 only. Not the browser, and not a Linux 7.2 acceptance result.
```

Six issues were fixed on the way (loader/module paths, eudev input_id rule,
/usr/libexec helper wrappers, stale weston twin holding DRM master, Mesa
built without the wayland platform) — details in the gate doc.

Next: layer 07 browser gate (Cog + WPE WebKit) — large Buildroot build.

## Browser gate PASS (2026-08-30, layer 07)

cog (WPE WebKit) renders a local HTML page fullscreen on the panel through
WPEBackend-fdo → weston → freedreno (`docs/handoff/pmos612-browser-cog.md`,
row `OP3-BROWSER-001`):

```text
image:  artifacts/boot-oneplus3-pmos612-v74dtb-browser.img (802d803c…)
bundle: artifacts/op3-browser-bundle.tar.gz (bec5ce8b…)
result: cog `wl` platform, fullscreen 1080x1920; page "Loaded successfully";
        CSS colour animation + JavaScript seconds counter advancing
        (owner-confirmed, two runs + deploy-session run)
scope:  layer 07 local-page rendering. Network browsing and multimedia are
        out of scope (gstreamer OFF in the build).
```

The graphics/user-space chain on the pmOS 6.12 control is validated end to
end: **DRM dumb buffer → GPU firmware → EGL → Wayland compositor → browser**.
Ten build/runtime fixes were required en route (all documented in the gate
doc): build parallelism, harfbuzz-icu, Mesa `-Dplatforms=wayland`, Mesa
`legacy-wayland=bind-wayland-display` (diagnosed with a cross-compiled
eglGetProcAddress probe), cog platform name `wl`, `--fullscreen` removal,
ln -sfn bridge gotcha, DejaVu fonts, wpe-webkit-2.0 helper wrappers,
shared-mime-info, and a cog fullscreen-size patch.

Next: the control-baseline bring-up is COMPLETE through layer 07. Remaining
project lines: Linux 7.2 early-boot (last), GPU regulator nodes in DTB,
`CONFIG_U_SERIAL_CONSOLE=y` kernel rebuild.

## Cross test: browser on the 6.3.1 (v100) kernel — PASS (2026-08-30)

The same browser bundle ran unchanged on `6.3.1-msm8996+ #31`
(`artifacts/boot-oneplus3-v100-browser.img`, v100 Image+DTB + browser
initramfs): weston GL renderer FD530, cog `wl` platform, page loaded,
desktop + rendered page owner-confirmed. Caveat: ~452 GPU SMMU context
faults (b40000.iommu, iova=0x0) on 6.3.1's a5xx path — non-blocking,
worth investigating in the parallel project.

**Conclusion: the 6.3.1 parallel project's browser failures were
user-space, not kernel.** The browser bundle + run.sh (fonts, MIME database,
loader discipline, helper wrapping, `wl` platform) transfers directly.
Details in `docs/handoff/pmos612-browser-cog.md` cross-test section.

## PROJECT BASELINE DECISION (2026-08-30): pmOS 6.12.1 LTS — 7.x line SHELVED

**The long-term project kernel is the pmOS 6.12.1 LTS line** (LTS EOL
2028-12-31; already validated end-to-end through the layer-07 browser gate on
this device). Non-LTS 6.19/7.0/7.1/7.2 carry no long-term maintenance and are
out of scope for the product path.

- **7.x line shelved** (owner decision): OP3's LK produces no early-boot
  output without a physical UART, so every 7.x boot is blind (fastboot
  return / no ACM / no pstore). Last data points: OP3-BOOT-040/041 — 7.0-rc1
  fails with BOTH the upstream OP3 DTB and the v74 DTB under the gemini-proven
  recipe; regression window is the 6.19.5 → 7.0-rc1 merge window. Full
  analysis and resume criteria in `docs/handoff/linux70rc1-minimal-ab.md`.
- **source/ reduced** to `linux-mainline-6.12.1`, `linux-pmos-msm8996-6.12`,
  `linux-pmos-msm8996-6.3.1` (+ `buildroot`). Unpushed local work from the
  removed trees (S6E3FA5 driver ports for 6.16/6.19.5, the 7.2 A/B patches)
  is archived in `patches/shelved-7x/` — the reusable asset if a newer LTS
  (e.g. 6.18) is ever picked up again.
- **Baseline retest PASS (OP3-BROWSER-003, 2026-08-30)**: the layer-07 browser
  gate re-runs clean on the pmOS 6.12.1 baseline after the source/out cleanup —
  weston desktop + cog fullscreen animated page owner-confirmed, and for the
  first time with a full ACM kernel-log capture from boot (554 lines,
  `artifacts/console-browser-retest-20260830.log`).
- Still-open project lines (unchanged): GPU regulator nodes in DTB,
  `CONFIG_U_SERIAL_CONSOLE=y` kernel rebuild for a real `console=ttyGS0`.

## Browser network gate PASS (OP3-BROWSER-005, 2026-08-31) — CJK font caveat

Branch `agent/implementation/op3-browser-net-001` (based on the OP3-WIFI-001
tip) combines the two PASS chains: the sda15 `run.sh` brings up Wi-Fi via
`/newroot/opt/op3-wifi/wifi auto`, steps the clock from the baidu HTTP Date
header (initramfs busybox has no ntpd), powers the GPU on after association,
and cog loads `https://www.baidu.com` with the TLS-enabled bundle
(glib-networking → OpenSSL GIO backend + ca-certificates; run.sh bridges
`/usr/lib/gio` and `/etc/ssl`). Device result: **PASS** — owner-confirmed
page rendering, `Loaded successfully` with no TLS error
(`d27dac50…` boot image). Two recorded caveats: (1) Chinese glyphs render as
tofu — bundle has DejaVu only; follow-up OP3-BROWSER-006 (stage a CJK font,
no rebuild needed). (2) Two early runs hard-reset at association time before
the GPU power-on sequencing fix; not reproduced since, attribution unproven
(placeholder DTB GPU regulators remain the suspected root cause). Details:
`docs/handoff/op3-browser-net-001.md`, row `OP3-BROWSER-005` in
`docs/test-matrix.md`.

Status update (2026-08-31 evening): OP3-BROWSER-006 PASS (owner confirmed
Chinese renders, WQY Microhei bundled + `TZ=CST-8`). A third hard reset
during browser display was traced to a **low battery** — all device tests
must now record battery/charging state. Architecture settled for the phone:
power-first on-demand sessions over the fbcon + agent-CLI home (see the
"Display model, power-first" section of the handoff); `kiosk-shell` is the
run.sh default and the dbus session bus is started for `cogctl`/WebDriver;
`BR2_PACKAGE_WPEWEBKIT_WEBDRIVER=y` is in the defconfig for the owner's
Playwright-style automation (incremental rebuild in progress).

Handover (2026-09-02): WebDriver transport verified end-to-end from the PC
(session/navigate/element/JS-inject/data/screenshot — see
`tests/browser/`); renderer intermittently crashes or hangs on heavy pages
→ the "mixed objects" theory was REJECTED (dirclean rebuild is
byte-identical to the reconfigure build) and POWER is the primary suspect;
the landscape output default (rotate-90, 1920x1080) is live. Full handover
steps + charging-check protocol: "Handover state (2026-09-02)" section of
`docs/handoff/op3-browser-net-001.md`. Private collection assets
(credentials) live in gitignored `local/jnu/` — never commit/push.

## Key artifacts

- **OP3-INITRD-FW-001 PASS (OP3-BOOT-044, 2026-08-31)**: Issue #3's six
  MSM8996 firmware files were reconstructed from SHA512-pinned inputs and
  verified against the historical SHA256 output manifest. With Image.gz, own
  DTB, boot profile, cmdline, non-firmware initramfs entries and sda15 fixed,
  the device reached recovery/RNDIS/SSH; early SLPI and ADSP loads succeeded.
  Artifact `boot-oneplus3-pmos612-own-dtb-firmware-provenance.img`
  (`38e59038…`); handoff `docs/handoff/op3-initrd-fw-001.md`.
- **Own-DTB browser closure (OP3-BROWSER-004, 2026-08-31)**: with the same
  OP3-BOOT-044 Image.gz and own DTB, the existing GPU-firmware/browser overlay
  and rootfs bundle ran Weston → Cog/WPE successfully. Weston used DSI-1 at
  1080x1920@60 with Mesa/freedreno FD530; the local page loaded and the owner
  visually confirmed it. This completes the own-DTB boot/initramfs/browser
  test line; no kernel source or configuration change is indicated.
- **Firmware placement follow-ups are deferred**: modem/MBA (MSS), Venus and
  ath10k are module-driver projects, not part of the boot-image gate. Open one
  branch/task per driver when that functionality is needed; first establish the
  exact modules and dependencies for `6.12.1-msm8996+`, then test firmware
  placement and loading after `/newroot`. Do not reopen OP3-BOOT-044 merely
  because these optional module paths remain untested.
- pmOS 6.12 own-DTB + reproducible-initramfs boot PASS (OP3-BOOT-042/043):
  `artifacts/boot-oneplus3-pmos612-own-dtb-repro-initrd.img`
  (`0b2e85ee…`); source `91df7ccd…`, tree-built DTB `cb29ab65…`, generated
  initramfs `61cf6338…`. The source uses the direct `rpm-glink` topology; it
  replaces the v74 DTB only for the pmOS 6.12 product path. The generated
  initramfs is a reproducible/auditable reserialization of the still-pinned
  historical archive; MSM8996 firmware provenance is now separately validated
  by OP3-BOOT-044. Details: `docs/handoff/op3-dts-rpm-001.md` and
  `docs/handoff/op3-initrd-001.md`.
- Project-owned pmOS source patch archive (GitLab upstream remains read-only):
  `patches/pmos612-op3-own-dtb/` contains the ordered S6E3FA5 and direct
  RPM-GLINK patches needed to reproduce OP3-BOOT-042 on the pinned 6.12.1
  source commit.
- 6.16.12 bootable (v74 DTB): `artifacts/boot-oneplus3-pmos616-v74strict-v74dtb.img`
- 6.19.5 bootable (v74 DTB): `artifacts/boot-oneplus3-pmos6195-v74strict-v74dtb.img`
- 6.16.12 worktree: `source/linux-pmos-msm8996-6.16` (tag `v6.16.12-msm8996`)
- v74 DTB: `out/pmos-msm8996-6.3.1-v74full/arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb` (73383 bytes, `463b2c72...`)
- Panel driver source: `source/linux-mainline-6.4/drivers/gpu/drm/panel/panel-samsung-s6e3fa5.c`
## DEVICE full flow PASS: feishu delivered from the phone (2026-09-03)

The complete JNU flow ran on the OnePlus 3 (browser-net image, rotate-270,
same collector): slider (human) -> fill -> login -> mouse-mode override ->
tab -> semester + both categories via filter+first-item-click -> building
name -> query (report iframe took ~40 s on-device; execute/sync BLOCKS
while the report renders - tolerate long timeouts) -> table extraction
(1.05 MB) -> parse (29 classrooms, 本科703/研究生189/借用19) -> JSON
-> feishu DELIVERED.

Device-specific findings vs PC rig:
- After slider verification the login FORM fields (#un/#pd) render LATE
  (~10 s on device vs instant on PC): the collector must poll for
  #un existence before filling (patched; also login deadline 300->480 s).
  The first fill+click hit a not-yet-rendered form and the yidun token
  expired before the manual retry - the owner re-clicked and login
  succeeded; collector deadline expired seconds before success, so the
  remaining steps were driven ad-hoc against the live session. The
  patched collector automates exactly that sequence for future runs.
- Same engine-version parity as the PC rig held: no flow changes needed
  for the device other than timing tolerance.

## Chrome/Playwright migration line - day 1 results + WAF wall (2026-09-03 evening)

Decision: the automation product line moves to Chromium/Playwright (the
owner's original, proven stack); the WPE line stays as the lightweight
browser bring-up achievement. Rationale: every WPE automation blocker
(jqx touch-mode mismatch, cookie-jar losing login tickets, WebDriver
injection dropping cross-domain cookies, no native headless) does not
exist in Chromium, and the owner's original Playwright code ports as-is.

PC validation day 1 (chrome_test.py, in cogwebauto private repo):
- Env: playwright already present (owner's original automation),
  chromium-1234 browsers installed; user-level install on python3.14.
- VISIBLE login PASS: persistent context + auto-filled credentials +
  human slider + login click. CRITICAL detection lesson: after login the
  SPA renders 排课管理 AT the CAS URL (URL stays icas.jnu.edu.cn/cas/...)
  - success detection MUST use the page TITLE, never the URL.
- Login cookies are SESSION cookies (no expiry) -> Chromium by design
  never writes them to the profile disk -> restarting a persistent
  context cannot restore the login. This is browser security design,
  not a bug.
- Correct restore mechanism = Playwright storage_state export/import
  (session.json, includes session cookies): implemented in chrome_test.py
  (login exports after success; verify imports + headless). UNTESTED:
  blocked by the auth4 WAF (see below).
- Headless wall: auth4.jnu.edu.cn 云防护 WAF returns 网关错误 for
  HeadlessChrome UA; with a normal-Chrome UA override the icas step
  passed but the auth4 login navigation got blocked again (fingerprint
  inconsistency and/or rate limiting after ~dozens of automation hits
  today). The WAF likely flagged this IP/fingerprint for the day.
- gid_ URL finding: the jw app URL embeds an auth ticket (gid_=MkpJ...)
  but the owner confirmed it only works combined with the session
  cookies; an old recorded URL (t_s=2022) was rejected.

Tomorrow: resume storage_state import validation (fresh login ->
export -> immediate headless restore), then headless full flow, then the
device rootfs decision (pmOS vs Ubuntu debootstrap vs deb-extraction
into the buildroot rootfs). Avoid UA inconsistency between visible and
headless runs; consider warming up slowly to avoid the WAF.

## PC rig: FULL JNU flow PASS + feishu delivered (2026-09-03)

End-to-end on the PC comparison rig (same WPE/cog pins as device):
login (slider+fill+click) -> jqx mouse-mode override -> tab -> semester via
filter+first-item-click -> both category selections (same interaction,
values 6+7 verified in hidden input) -> building name fill -> query ->
iframe table extraction -> parse (29 classrooms, 本科703/研究生189/借用19)
-> JSON saved -> feishu text+file DELIVERED.

Fixed en route (all pushed to cogwebauto): collector helper ordering
(UnboundLocalError), iframe selector must iterate ALL iframes (first one is
an empty placeholder; the report lives in the frReport2/show.do iframe).
jqx touch-mode root cause and the mouse-mode override are documented above;
the override is applied by the collector right after login.

## PC rig: JNU login PASS; collection dropdown findings (2026-09-03)

PC comparison rig (buildroot x86_64, same WPE 2.50.5/cog 0.18.5 pins) is
OPERATIONAL after fixing a chain of rig-only issues, and the JNU login
flow PASSED end-to-end on it:

- slider (human) -> yidun-gone detection -> human pause -> key-event fill ->
  pause -> programmatic login click -> CAS redirect -> 排课管理 marker:
  **PASS on PC**. The redirect-load-error seen on the device did NOT
  reproduce on the PC -> that failure is device/network-specific, not a
  flow bug. Device attempt can proceed once the collection step is fixed.
- Rig fixes landed in `local/jnu/op3-pc-rig.sh` (pushed to cogwebauto):
  `COG_MODULEDIR` (cog looks for platform modules at compiled-in
  /usr/lib/cog/modules), `GIO_MODULE_DIR` (glib TLS module; without it
  every https page shows the device-era "TLS not supported" error),
  XDG_RUNTIME_DIR must be the DESKTOP SESSION's /run/user/<uid> (cog's
  check_supported() does wl_display_connect(NULL); a rig-private
  XDG_RUNTIME_DIR makes that fail -> wl platform reports "not supported"),
  libWPEBackend-default.so symlink to the fdo backend.
- Rebuild chain this session (all committed): MESA3D_LLVM explicit
  (kconfig silently dropped llvmpipe->EGL/GLES->wpewebkit otherwise),
  CAIRO for cog's wayland platform, libdrm re-dirclean (same incremental
  trap), LLVM AMDGPU backend needed for radeonsi, host-llvm dirclean
  (llvm-dirclean does NOT rebuild the host variant; mesa consults the
  STALE host llvm-config via sysroot). Verified: probe shows
  `bind-wayland-display: YES` (WPEBackend-fdo requirement).
- Collection step findings (page DOM, live session): the 学期 select is
  jqxDropDownList (already correct value, can be skipped); 教室分类
  (JSFLDM) is emap multi-select2 wrapping jqxDropDownList. Position-based
  xpaths from the old Playwright flow do NOT match this DOM. JS-synthetic
  el.click() and jqxDropDownList('selectItem') do NOT register item
  selection; emap harvests the selection on the jqx 'close' event
  (handlers on wrapper: close x2, open, keydown/focus/blur). Next step:
  open via jqxDropDownList('open') (works), select items by their native
  mousedown path (WebDriver native click on .jqx-item needs position
  verification), then trigger 'close' and verify the hidden input value,
  then fill 教室名称 (番禺教学大楼2) and click 查询 -> iframe table ->
  parse -> feishu.

## Privacy-scraping content lives in a PRIVATE repo (2026-09-02)

All JNU/credential-bearing web-scraping assets are versioned in the private
repo `https://github.com/iamgreatwk/cogwebauto` (branch `main`): collector
(`jnu_collect.py` incl. yidun-aware humanized login flow), parser
(`parse_table_to_json_v2.py`), credentials (`config.json`), deploy tooling
(`op3-deploy.sh`) and the xlsx templates. In THIS repo they stay under
`local/` which is gitignored - NEVER commit them to any public branch; the
nested git repo at `local/jnu/.git` is their version control. Excluded from
the private repo: `site-packages/`, `wheels/`, `__pycache__/`, `out/`.

## Device test result + PC comparison rig (2026-09-02, second session)

Full-flow device test (charged, fastboot boot browser-net img, rotate-270
confirmed in weston log): yidun slider drag PASSED (owner saw the green
verify-success mark), script filled un/pd and clicked login; during the
post-login redirect the browser showed an error page (owner description:
a "no path / route"-style load error). After that the CAS form was back
empty. All device processes were then stopped (collector + browser stack,
0 residual, GPU control=auto).

Owner directive: STOP debugging on the phone. Build a PC comparison rig
with the SAME engine versions and validate the whole flow there first:

- `buildroot/op3-browser-x86-pc.defconfig` (committed): x86_64 buildroot
  with the SAME pins as the device (WPE WebKit 2.50.5, cog 0.18.5,
  WPEWebDriver), llvmpipe software GL for a self-contained stack. Output:
  `out/buildroot-x86-pc`. Build running (nohup, /tmp/buildroot-x86-pc.log).
- `local/jnu/op3-pc-rig.sh` (private): up/down/collect/log. Prefers the
  desktop's native GNOME Wayland session for cog (no weston install
  needed); falls back to nested distro weston. System python3 already has
  requests+bs4 for the collector.
- PC-rig rule: the JNU flow (slider login incl. the redirect-failure step)
  must PASS on the rig before any device attempt; one device session per
  validated change only, then `op3-deploy.sh stop`.

## Device heat protocol (owner directive, 2026-09-02)

The browser stack (weston + cog + forced-on GPU) was left running during
host-side code/debug cycles and cooked the phone. RULES:

- After EVERY debug/test session on the device, kill the browser stack:
  `tests/browser/op3-automation-stop.sh` (on device; kills weston/cog/
  WPEWebDriver/webkit helpers/dbus/udevd, restores GPU runtime PM to auto,
  fbcon takes the display back). Host shortcut: `local/jnu/op3-deploy.sh stop`.
- Never leave the device-side session up "just in case" between host-side
  code edits; restart it on demand (`op3-deploy.sh restart-session`).
- Overheated / low-battery device: stop all device operations until it has
  cooled down / recharged (reset-class failures must record battery state).

## Screen orientation fix + JNU flow prepared (2026-09-02)

- Owner confirmed the landscape rotation is 180 deg off: default
  `WESTON_TRANSFORM` changed `rotate-90` -> `rotate-270` in BOTH
  `boot/browser-test/opt/op3-browser/run.sh` and
  `tests/browser/op3-automation-session.sh` (not yet deployed - device
  cooling/recharging; deploy with `local/jnu/op3-deploy.sh deploy`).
- JNU CAS login PASS recorded (slider gate was the only blocker; see
  op3-browser-net-001.md). Anti-detection pacing added to
  `local/jnu/jnu_collect.py`: yidun-disappear detection -> human pause ->
  key-event fill -> pause -> programmatic login click -> human-paced
  collection steps (random-jitter pauses via human_pause()); fixed
  undefined `res`/`BUILDING_NAME` in main()/collect_rest().
- New tooling: `tests/browser/op3-automation-stop.sh` (device stop script)
  and `local/jnu/op3-deploy.sh` (host: deploy / restart-session / collect /
  log / stop). Full-flow retest pending device cool-down + charge: deploy ->
  restart-session -> collect -> owner slides slider on the (now correctly
  oriented) touchscreen -> script fills/clicks -> collection -> feishu.

