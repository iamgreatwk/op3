import json, urllib.request, time, base64

BASE = "http://172.16.42.1:7000"
EID = "element-6066-11e4-a52e-4f735466cecf"


def wd(method, path, body=None, timeout=60):
    data = json.dumps(body).encode() if body is not None else None
    req = urllib.request.Request(BASE + path, data=data,
                                 headers={"Content-Type": "application/json"},
                                 method=method)
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return json.load(r)["value"]
    except urllib.error.HTTPError as e:
        print(f"  ! HTTP {e.code} on {method} {path}:", e.read().decode()[:200])
        raise


print("[1] status:", wd("GET", "/status"))
s = wd("POST", "/session", {"capabilities": {"alwaysMatch": {}}})
sid = s["sessionId"]
print("[2] session:", sid)

try:
    wd("POST", f"/session/{sid}/url", {"url": "https://www.baidu.com"}, timeout=60)
    time.sleep(5)
    print("[3] title:", wd("GET", f"/session/{sid}/title"))

    kw = wd("POST", f"/session/{sid}/element",
            {"using": "css selector", "value": "#kw"})
    kid = kw[EID]
    print("[4] search box found:", bool(kid))

    # No physical keyboard on the device: WebDriver key events have no focus
    # path, so drive the form through JS injection (equivalent for data tasks).
    wd("POST", f"/session/{sid}/execute/sync",
       {"script": "document.getElementById('kw').value='oneplus3 mainline linux';",
        "args": []}, timeout=30)
    wd("POST", f"/session/{sid}/execute/sync",
       {"script": "document.getElementById('su').click();", "args": []}, timeout=30)
    time.sleep(5)

    print("[5] title after click:", wd("GET", f"/session/{sid}/title"))
    try:
        txt = wd("POST", f"/session/{sid}/execute/sync", {
            "script": "return (document.getElementById('content_left')||document.body).innerText.slice(0,300);",
            "args": []}, timeout=30)
        print("[6] results data:", str(txt)[:300].replace("\n", " | "))
    except Exception as e:
        print("[6] execute/sync not supported:", e)

    shot = wd("GET", f"/session/{sid}/screenshot", timeout=30)
    open("/tmp/op3-webdriver-shot.png", "wb").write(base64.b64decode(shot))
    print("[7] screenshot saved: /tmp/op3-webdriver-shot.png")
finally:
    try:
        wd("DELETE", f"/session/{sid}")
        print("[8] session closed")
    except Exception as e:
        print("[8] close failed:", e)

import struct as _s
_d = open("/tmp/op3-webdriver-shot.png", "rb").read(24)
_w, _h = _s.unpack(">II", _d[16:24])
print("[9] PNG: %dx%d ->" % (_w, _h), "LANDSCAPE" if _w > _h else "PORTRAIT")
