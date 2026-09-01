# Browser automation tests (OP3-BROWSER-005 WebDriver extension)

PC-side Playwright-style automation against the OnePlus 3 WPE browser.

**Before writing or running ANY command/script on the device, read
`docs/device-shell-compat.md`** — the BusyBox/kernel/Buildroot trap list.
Every rule there was learned from a real incident.

## Components

- `op3-automation-session.sh` — device-side script (push to the device, run
  with `sh`): starts the kiosk compositor, `cog --automation` and
  `WPEWebDriver` with everything they need (loopback up, input_id udev rule,
  helper-process wrappers through the bundled loader, dbus session bus from
  the bundle's `session.conf`, GPU forced on per docs/known-issues.md).
  Requires on sda15: the WebDriver bundle (`op3-browser-bundle-webdriver.tar.gz`)
  and network (Wi-Fi already associated by the boot flow).
- `wd-baidu-test.py` — PC-side WebDriver client (stdlib only, no selenium
  dependency): status → new session → navigate baidu → find `#kw` → type →
  click `#su` → read result text via `execute/sync` → screenshot → close.

## Transport facts (verified 2026-09-01)

- WPEWebDriver listens on `0.0.0.0:7000` (`--host=all`); the automation
  channel to the browser is the WebKit remote inspector server on
  `127.0.0.1:9222` (`WEBKIT_INSPECTOR_SERVER` + `cog --automation`), wired
  via `-t 127.0.0.1:9222`.
- `ip link set lo up` is REQUIRED — the initramfs does not enable loopback
  and the inspector bind fails with EADDRNOTAVAIL otherwise.
- dbus-daemon must be started with `--config-file=<bundle>/usr/share/dbus-1/session.conf`
  and WITHOUT `--session` (they conflict); the initramfs root has no
  dbus-1 configuration.
- The WPE helper processes (`WPEWebProcess`/`WPENetworkProcess`/`WPEGPUProcess`)
  are exec'd by absolute path and must be wrapped through the bundled loader
  (the script recreates the run.sh-style wrappers because a bundle
  re-extract removes them).
- `--replace-on-new-session` avoids "Maximum number of active sessions"
  when a previous client died without DELETE-ing its session.

## Status

Blocked on the clean `wpewebkit-dirclean` rebuild: the reconfigure-based
rebuild produced a renderer that crashes on heavy pages (see
`docs/handoff/op3-browser-net-001.md`). Session/navigate steps verified
working; element/click/data steps pending the rebuild.

## Deployment discipline (mandatory after the 2026-08-31/09-01 mess)

Use `deploy-bundle.sh` for every bundle deploy — never a hand-rolled
`tar -x` over the live directory:

```sh
tests/browser/deploy-bundle.sh artifacts/op3-browser-bundle-webdriver.tar.gz \
  [root@<device>] [tag]
```

It extracts into a versioned directory (`/newroot/opt/op3-browser.<tag>`),
re-applies every runtime asset that an extraction destroys (run.sh,
test-page, CJK font; the udev rule/helper wrappers/locale/dbus are applied
by `op3-automation-session.sh`), verifies a full sha256 manifest against
the host target, then atomically switches the `op3-browser` symlink.
Rollback = point the symlink at the previous version directory.

Known-asset checklist per deploy (all automated now): loopback up,
input_id udev rule, helper wrappers via bundled loader, dbus session.conf
from the bundle, CJK font, `TZ=CST-8`, GPU `control=on`.
