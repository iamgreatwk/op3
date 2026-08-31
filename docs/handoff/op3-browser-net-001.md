# OP3-BROWSER-005 handoff — networked browser gate (cog over Wi-Fi)

```text
Task / GitHub Issue: Owner-authorized no-Issue task ("浏览器访问 baidu" test), OP3-BROWSER-005
Role: Implementation
Baseline commit: 31875200245e16643de1722ef12f823c910302cc (tip of agent/implementation/op3-wifi-port-001)
Working branch: agent/implementation/op3-browser-net-001
Changed files: boot/browser-test/opt/op3-browser/run.sh;
  boot/browser-net-test/{sbin/run_recovery.sh,README.md};
  buildroot/op3-browser.defconfig; docs (handoff, test-matrix)
Commit SHA: filled in per commit

Layer: 07 browser, network scope extension (one layer above the local-page browser gate)
Previous PASS milestone: OP3-BROWSER-004 (own-DTB browser, 2026-08-31) +
  OP3-WIFI-001 (WPA2-PSK association + DHCP, 2026-08-31)
Sole hypothesis: cog (WPE WebKit) can load and render a remote page
  (www.baidu.com) on the panel when the Wi-Fi link is brought up before the
  browser starts, using the existing sda15 browser bundle and the sda15 Wi-Fi
  CLI — no kernel/DTS change.
Only variable changed per test:
  1. Phase 1: the browser run.sh gains an OP3_BROWSER_URL/OP3_BROWSER_NET
     driven network bring-up (calls /newroot/opt/op3-wifi/wifi auto) and the
     launcher overlay passes http://www.baidu.com. No Buildroot rebuild.
  2. Phase 2 (separate owner build): BR2_PACKAGE_GLIB_NETWORKING=y +
     BR2_PACKAGE_CA_CERTIFICATES=y enable https. One build, one variable.

Build run by project owner: PASS — incremental Buildroot 2026.02.3 on
  out/buildroot-op3-egl: ca-certificates, p11-kit, libtasn1, gmp, nettle,
  gnutls (unused: buildroot's glib-networking defaults to the OpenSSL
  backend), openssl, glib-networking. Mesa/weston/wpewebkit reused.
Build result: PASS
Artifacts and SHA256:
  op3-browser-bundle-tls.tar.gz
    40346b442ea0e1ef56f1f06378476161c8a34f6cbafcc14d0bf3e9f46c423fa3
  initrd-op3-browser-net.cpio.gz
    ca7b10a00aecb4920bda4bacca98392ade49729b9994b626dbb3efbfa4880699
  boot-oneplus3-pmos612-own-dtb-browser-net.img
    d27dac50cc7218f76c14dbf1c996443a1385087f0914ecf80b4611d028e55f4f
  Image.gz 088d472f… and own DTB cb29ab65… identical to OP3-BOOT-044
  (out/pmos-msm8996-6.12-rpm-glink-own-dtb)

Device test run by project owner: PASS (owner-confirmed panel rendering,
  2026-08-31; final automated fastboot-boot flow plus a manual rerun)
Device result: PASS for the network gate with one recorded caveat: CJK
  text renders as tofu (bundle ships DejaVu only) — follow-up
  OP3-BROWSER-006, not a network-layer failure.
Evidence links / log paths: /newroot/var/log/op3-browser-net.log,
  /run/op3-weston/cog.log (`<https://www.baidu.com/> Load started /
  Loading… / Loaded successfully`, no TLS error), ACM console relay,
  owner panel confirmation

Conclusion: SUPPORTED for the network-gate scope (Wi-Fi association, DHCP,
  DNS, clock, HTTPS page load and rendering). Not an Integration acceptance.
Uncertainties:
  - CJK text renders as tofu (no CJK font in the bundle) — follow-up
    OP3-BROWSER-006.
  - Hard-reset attribution open: two early runs reset at association time;
    not reproduced after the GPU power-on sequencing change, but one stale
    run also survived. Placeholder DTB GPU regulators suspected.
  - The injected-bundle WARNING in WPEWebProcess is pre-existing and
    non-fatal (present since the layer-07 local-page gate).
Recommended next experiment: OP3-BROWSER-006 — stage a CJK font (WQY
  Microhei) into the bundle and confirm Chinese text renders; then decide
  whether a DTB GPU-regulator task is opened for the brownout question.
```

## Debug log

### Run 1 (2026-08-31): network PASS, hard reset at wlan0 association

Manual SSH run against the Wi-Fi-gate image (GPU firmware absent → GPU inert):
network bring-up fully PASSED — `wifi auto` found the already-associated
wlan0, IPv4 `192.168.1.6/24`, resolv.conf `223.5.5.5`, default route,
`ping 223.5.5.5 OK` — but weston died with `MESA: get_param failed -6
(ENODEV)` because that initramfs carries no A530 firmware. Expected; the
browser-net image does carry it.

The packed browser-net image run then hard-reset the device. Post-mortem from
the synced launcher log (`/newroot/var/log/op3-browser-net.log`):

```text
[ 8.7s] launcher: GPU runtime PM disabled: control=on, cur_freq=624000000
[11.3s] ath10k_pci probed, QCA6174 fw WLAN.RM.4.4.1-00309 loaded
[18.78s] wlan0: associated   (DHCP not yet run)
        < next 5 s periodic snapshot never happened: SoC hard reset >
```

This is the first configuration with the GPU forced on AND Wi-Fi active.
The wifi gate passed with the GPU inert (no firmware); the browser gates
passed with no Wi-Fi. Best-fit hypothesis: the ath10k TX power ramp at
association completion stacks on the always-on GPU behind the placeholder
DTB GPU regulators (docs/known-issues.md) → supply brownout → reset.

Countermeasure (one variable): move the GPU `control=on` from launcher time
to run.sh, after the Wi-Fi link is up and right before weston. The
browser-net launcher no longer touches GPU PM; run.sh does it idempotently
(the browser-test launcher duplicate is harmless).

### Baidu target adjustment (2026-08-31)

Owner confirmed on a PC that `http://www.baidu.com` 302-redirects to https.
The current bundle has no GLib TLS backend, so phase 1 cannot render the real
baidu homepage. Phase 1 target switched to `http://neverssl.com` (http-only),
which still validates the full chain: Wi-Fi association → DHCP/DNS → HTTP GET
→ cog rendering a live remote page. Baidu (`https://www.baidu.com`) moves to
phase 2 (glib-networking + ca-certificates rebuild). A baidu run on the
current bundle is still useful as on-device evidence: cog follows the 302 and
shows its TLS error page.

### Next: rerun with sequenced GPU power-on

```sh
# repack (launcher change), then redeploy run.sh and fastboot boot
OVERLAY_SOURCE="$PWD/boot/browser-net-test" scripts/make-drm-test-initrd.sh \
  artifacts/initrd-op3-firmware-provenance-v2.cpio.gz artifacts/op3-drm-dumb \
  artifacts/initrd-op3-browser-net.cpio.gz
scp -O boot/browser-test/opt/op3-browser/run.sh root@172.16.42.1:/newroot/opt/op3-browser/run.sh
```

If the reset still lands at association time, the next isolation step is
associating Wi-Fi from init_mainline (wifi_auto overlay) BEFORE any GPU
activity, i.e. the exact wifi-gate power state plus a later GPU power-on.

## Result (2026-08-31): PASS for the network gate

Final flow, all launcher-automated (fastboot boot, no manual env):

```text
launcher start (no GPU touch)
network: wifi auto → wlan0 associated (SSID 1106, 192.168.1.6/24)
network: resolv 223.5.5.5, default route via 192.168.1.1, ping 223.5.5.5 OK
network: clock pre-2001 → set from baidu HTTP Date header
         ('Mon, 31 Aug 2026 12:18:56 GMT' → UTC clock)
GPU runtime PM disabled AFTER network (cur_freq=133000000 → resumed)
weston ready after 1 s (wayland-1), cog platform wl
cog: <https://www.baidu.com/> Load started → Loading… → Loaded successfully
     (no redirect needed — direct https; no TLS error with the OpenSSL
     GIO backend + bridged /usr/lib/gio and /etc/ssl)
owner: page rendered on the panel
```

### Caveat recorded: CJK tofu — follow-up OP3-BROWSER-006

The rendered page shows Chinese text as boxes: the bundle ships DejaVu only
(Latin/Greek/Cyrillic). Fix direction: stage a CJK font (e.g. WQY Microhei,
~5 MB, GPL+font-exception) into the bundle's `/usr/share/fonts/` — fontconfig
bridging already exists in run.sh; no Buildroot rebuild required if the font
file is added at stage time. One task, one variable: font presence.

### Hard-reset history (open question)

- Run 1 (stale launcher, GPU on at launcher time): hard reset at wlan0
  association completion.
- Run 2 (same stale artifacts): survived, reached the TLS error page —
  reset NOT deterministic even in the old configuration.
- Runs 3+ (sequenced GPU power-on): no reset in three runs.
Attribution of the fix to the sequencing is therefore unproven; a
marginal-supply brownout hypothesis (GPU + ath10k TX behind placeholder DTB
regulators) remains the best fit. Keep watching for resets under network
load; the DTB GPU-regulator fix (existing project line) is the real cure.

## Deferred feature list (2026-08-31, owner decision)

The browser's role in this project is automated testing, so the following
gaps against a "modern browser" are recorded but intentionally NOT
implemented for now:

- P0: user-data persistence (WebKit profile lives on initramfs tmpfs; point
  HOME/XDG_* at sda15 in run.sh); on-device navigation (cog has no URL bar;
  D-Bus remote control is the interim path — the session bus is now started
  by run.sh).
- P1: on-screen keyboard (weston-keyboard already bundled; needs the
  text-input/IME path), readability scaling (`cog --device-scale=2`),
  media playback (gstreamer chain unbuilt; blocked on the audio task
  bringing up the audio path).
- P2: WebGL verification, download management, clipboard.
- Browser stack direction (2026-08-31): automation is the primary purpose;
  `BR2_PACKAGE_WPEWEBKIT_WEBDRIVER=y` is now enabled in the defconfig for
  the Playwright-style owner workflow. Cage (wlroots kiosk compositor) is a
  possible future simplification of the display stack — requires a Buildroot
  rebuild and would replace the validated weston chain, so it stays deferred
  until the recovery-integration work actually needs it.

## OP3-BROWSER-006 (2026-08-31): CJK font PASS

`wqy-microhei.ttc` (5 MB, GPL+font-exception, fetched per the recipe inside
`stage-browser-rootfs.sh`) staged into the bundle; `run.sh` exports
`TZ=CST-8`. `fc-list` indexes 2 WenQuanYi faces; owner confirmed Chinese
renders correctly on the baidu homepage.

## OP3-BROWSER-007 (recorded): pinch zoom / viewport pan

Owner feedback: no pinch zoom; viewport-fitted pages cannot be panned.
Interim mitigations: `cog --scale` / `--device-scale` (static zoom — a
`--device-scale` > 1 renders viewport-fitted pages larger and scrollable).
Investigation task: whether the WPE WebKit gesture controller in this build
can deliver pinch zoom through cog's touch path.

## Display switching with the recovery terminal (design note, 2026-08-31)

The browser's role is automated testing; the owner's daily driver will be the
ported 6.3.1 recovery terminal, with more programs joining later. Verified
facts:

- cog/WPE always needs a Wayland compositor, but NOT the desktop shell:
  `--shell=kiosk-shell.so` (already in the bundle) was verified on-device —
  1 s to ready, cog fullscreen, pages load. `run.sh` now defaults to it
  (`WESTON_SHELL` env overrides). `fullscreen-shell.so` is NOT xdg_shell
  compatible with cog; do not use. Cage (wlroots kiosk) is recorded as a
  possible future simplification — see the deferred list.
- Only one DRM master at a time, so switching between fullscreen-exclusive
  programs (recovery terminal ↔ browser ↔ future programs) is a
  serialization problem: a supervisor script stops the current display owner,
  runs the next, then restores. The automated-testing flow maps directly:
  WebDriver/`cogctl quit` → compositor exits → recovery program restarts and
  re-initialises the display.
- "Embedding the browser inside the recovery UI" is only viable if the
  recovery UI itself becomes Wayland-native (then cog can live on an IVI
  layer — the bundle already ships `ivi-shell.so` and the ivi HMI helper —
  or a subsurface). Otherwise keep the serialization model.

## Deferred feature list (2026-08-31, owner decision)

- The Wi-Fi CLI (`/newroot/opt/op3-wifi/wifi auto`) already performs module
  loading (absolute insmod order), wpa association against the saved default
  profile, `udhcpc` and resolv.conf setup. Calling it from the sda15
  `run.sh` means the browser-net test needs **no initramfs wifi overlay and
  no wifi-specific repack** — only the browser-net `run_recovery.sh` overlay
  member on top of the OP3-BOOT-044-provenance base archive.
- The URL is resolved in order: `OP3_BROWSER_URL` (launcher env) >
  `/newroot/opt/op3-browser/op3-url` (one-line file on sda15, editable over
  SSH) > built-in local test page. The default behavior of the existing
  browser gate is unchanged when neither is set.
- Network bring-up failure is deterministic: `FATAL-NET` lines in the
  launcher log and exit 1 before weston starts, so a FAIL is diagnosable
  from the log alone.
