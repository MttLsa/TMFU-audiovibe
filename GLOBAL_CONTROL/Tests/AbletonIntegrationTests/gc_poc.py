#!/usr/bin/env python3
"""
GLOBAL CONTROL - Proof-of-Concept client (Milestones 1-6).

Talks OSC/UDP on 127.0.0.1 to GLOBAL_CONTROL_BRIDGE.amxd running inside Ableton Live and runs the real
feasibility tests of the brief (section 17 / 22). Python 3.8+, standard library only.

    python3 gc_poc.py hello
    python3 gc_poc.py scan
    python3 gc_poc.py params  --device "Pro-C 2"                [--track Kick] [--json out.json]
    python3 gc_poc.py curve   --device "Pro-C 2" --param Threshold
    python3 gc_poc.py set     --device "Pro-C 2" --param Threshold --display -12      (absolute, all instances)
    python3 gc_poc.py set     --device "Pro-C 2" --param Threshold --raw 0.8
    python3 gc_poc.py relative --device "Pro-C 2" --param Threshold --delta -2
    python3 gc_poc.py watch   --device "Pro-C 2" --param Threshold     (move a knob in a plug-in GUI)
    python3 gc_poc.py phase-test --device "Pro-C 2" --param Threshold --expect 10 [--report report.md]

--device matches the device's class_display_name OR its (user-renamable) name, case-insensitive.
"""
import argparse, json, math, os, random, re, socket, struct, sys, threading, time, urllib.parse, queue

PROTO = 1
APP_PORT, BRIDGE_PORT = 47120, 47121

# --------------------------------------------------------------------------------------------- OSC codec
def _pad(b):
    return b + b"\0" * (4 - len(b) % 4)

def osc_encode(addr, args):
    tags, data = ",", b""
    for a in args:
        if isinstance(a, bool):
            a = int(a)
        if isinstance(a, int):
            tags += "i"; data += struct.pack(">i", a)
        elif isinstance(a, float):
            tags += "f"; data += struct.pack(">f", a)
        else:
            tags += "s"; data += _pad(str(a).encode("utf-8"))
    return _pad(addr.encode()) + _pad(tags.encode()) + data

def _rs(buf, off):
    end = buf.index(b"\0", off)
    return buf[off:end].decode("utf-8", "replace"), (end + 4) & ~3

def osc_decode(buf):
    addr, off = _rs(buf, 0)
    if off >= len(buf):
        return addr, []
    tags, off = _rs(buf, off)
    args = []
    for t in tags[1:]:
        if t == "i": args.append(struct.unpack_from(">i", buf, off)[0]); off += 4
        elif t == "f": args.append(struct.unpack_from(">f", buf, off)[0]); off += 4
        elif t == "d": args.append(struct.unpack_from(">d", buf, off)[0]); off += 8
        elif t == "h": args.append(struct.unpack_from(">q", buf, off)[0]); off += 8
        elif t == "s": s, off = _rs(buf, off); args.append(s)
        elif t == "T": args.append(1)
        elif t == "F": args.append(0)
        else: raise ValueError("unsupported OSC tag " + t)
    return addr, args

# ------------------------------------------------------------------------------------------------ client
class BridgeError(Exception):
    pass

class Client:
    def __init__(self, timeout=5.0):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4 << 20)
        self.sock.bind(("127.0.0.1", APP_PORT))           # localhost only: never reachable from the network
        self.session = "%016x" % random.getrandbits(64)
        self.seq = 0
        self.token = ""
        self.timeout = timeout
        self.inbox = queue.Queue()
        self.values = queue.Queue()
        self.bridge_session = None
        self.last_heartbeat = 0.0
        self._stop = False
        threading.Thread(target=self._rx, daemon=True).start()

    def _rx(self):
        while not self._stop:
            try:
                buf, peer = self.sock.recvfrom(65536)
            except OSError:
                return
            if peer[0] != "127.0.0.1":
                continue
            try:
                addr, args = osc_decode(buf)
            except Exception as e:
                print("! malformed packet:", e, file=sys.stderr); continue
            if len(args) < 3 or args[0] != PROTO:
                continue
            if addr == "/global/heartbeat":
                self.last_heartbeat = time.time(); continue
            if addr == "/global/parameter/value":
                self.values.put(args[3:])
            self.inbox.put((addr, args))

    def send(self, addr, *payload):
        self.seq += 1
        self.sock.sendto(osc_encode(addr, [PROTO, self.session, self.seq, self.token, *payload]), ("127.0.0.1", BRIDGE_PORT))
        return self.seq

    def wait(self, pred, timeout=None):
        end = time.time() + (timeout or self.timeout)
        while time.time() < end:
            try:
                addr, args = self.inbox.get(timeout=max(0.01, end - time.time()))
            except queue.Empty:
                break
            if addr == "/global/error":
                code, msg = args[4], urllib.parse.unquote(args[5])
                if args[3] in (0, self.seq) or code in ("E_AUTH", "E_PROTO"):
                    raise BridgeError("%s: %s" % (code, msg))
                print("! bridge error %s: %s" % (code, msg), file=sys.stderr)
                continue
            if pred(addr, args):
                return addr, args
        raise TimeoutError("no answer from GLOBAL_CONTROL_BRIDGE (is it on a track of the open Live Set?)")

    def chunked(self, addr, req, key):
        parts, total = {}, None
        while total is None or len(parts) < total:
            _, a = self.wait(lambda ad, ar: ad == addr and ar[3] == req and str(ar[4]) == str(key))
            parts[a[5]] = urllib.parse.unquote(a[7]); total = a[6]
        return json.loads("".join(parts[i] for i in range(total)))

    def hello(self, retries=3):
        for attempt in range(retries):
            req = self.send("/global/hello", "gc_poc.py", "0.1")
            try:
                _, a = self.wait(lambda ad, ar: ad == "/global/hello" and ar[3] == req, timeout=2.0)
                self.bridge_session, self.token = a[1], a[4]
                return {"bridgeSession": a[1], "projectId": a[5], "liveVersion": a[6], "bridgeVersion": a[7], "bridgeDeviceId": a[8]}
            except TimeoutError:
                if attempt == retries - 1:
                    raise

    def scan(self):
        req = self.send("/global/scan/request", 1000 + self.seq)
        doc = self.chunked("/global/device/list", req, 1000 + req - 1)
        _, done = self.wait(lambda ad, ar: ad == "/global/scan/complete" and ar[3] == req)
        doc["durationMs"] = done[7]
        return doc

    def parameters(self, device_id):
        req = self.send("/global/device/parameters", int(device_id))
        return self.chunked("/global/device/parameters", req, device_id)

    def curve(self, pid, n=65, lo=None, hi=None):
        args = [int(pid), n] + ([float(lo), float(hi)] if lo is not None else [])
        req = self.send("/global/parameter/describe", *args)
        return self.chunked("/global/parameter/curve", req, pid)

    def set_many(self, pairs, txn=None):
        flat = []
        for pid, v in pairs:
            flat += [int(pid), float(v)]
        txn = txn or (self.seq + 1)
        self.send("/global/parameter/set", txn, len(pairs), *flat)
        return txn

    def observe(self, pids, on=True):
        self.send("/global/parameter/observe", 1 if on else 0, len(pids), *[int(p) for p in pids])

    def get(self, pids):
        """Reads the effective values back from Live: {pid: (value, display, source)}."""
        while not self.values.empty():
            self.values.get_nowait()
        self.send("/global/parameter/get", len(pids), *[int(p) for p in pids])
        return self.collect(pids)

    def collect(self, pids, timeout=3.0, source=None, txn=None):
        want, got, end = set(str(p) for p in pids), {}, time.time() + timeout
        while want - set(got) and time.time() < end:
            try:
                v = self.values.get(timeout=0.05)
            except queue.Empty:
                continue
            pid, val, disp, src, t = str(v[0]), v[1], urllib.parse.unquote(v[2]), v[3], v[4]
            if pid in want and (source is None or src == source) and (txn is None or t == txn):
                got[pid] = (val, disp, src)
        return got

    def close(self):
        self._stop = True
        self.sock.close()

# --------------------------------------------------------------------------------------- display scales
NUM = re.compile(r"[-+]?(?:\d+(?:[.,]\d*)?|[.,]\d+)(?:[eE][-+]?\d+)?")

def parse_display(s):
    """'-12.0 dB' -> (-12.0, 'dB'); '1.2 kHz' -> (1200.0, 'Hz'); 'Off'/'-inf' -> (None, text)."""
    s = (s or "").strip()
    low = s.lower()
    if "inf" in low or "∞" in s:
        return (-math.inf if s.startswith("-") else math.inf), re.sub(r"[-+]?(inf|∞)", "", s, flags=re.I).strip()
    m = NUM.search(s)
    if not m:
        return None, s
    v = float(m.group(0).replace(",", "."))
    unit = s[m.end():].strip()
    if unit.startswith("kHz"): v, unit = v * 1000.0, "Hz"
    elif unit in ("s", "sec"): v, unit = v * 1000.0, "ms"
    return v, unit

class Scale:
    """Display <-> raw mapping learned from the parameter's own str_for_value samples."""
    def __init__(self, curve):
        self.min, self.max, self.quantized = curve["min"], curve["max"], curve["isQuantized"]
        self.pts = []
        for x, text in curve["samples"]:
            d, unit = parse_display(text)
            if d is not None and math.isfinite(d):
                self.pts.append((x, d))
        self.unit = parse_display(curve["samples"][len(curve["samples"]) // 2][1])[1] if curve["samples"] else ""
        ds = [d for _, d in self.pts]
        self.monotonic = len(ds) >= 2 and (all(b >= a for a, b in zip(ds, ds[1:])) or all(b <= a for a, b in zip(ds, ds[1:])))
        self.kind = self._classify()

    def _classify(self):
        if self.quantized:
            return "discrete"
        if len(self.pts) < 3 or not self.monotonic:
            return "unknown"
        xs = [x for x, _ in self.pts]; ds = [d for _, d in self.pts]
        def lin_err(ys):
            n = len(xs); mx, my = sum(xs) / n, sum(ys) / n
            sxx = sum((x - mx) ** 2 for x in xs) or 1e-12
            k = sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / sxx
            span = (max(ys) - min(ys)) or 1e-12
            return max(abs(y - (my + k * (x - mx))) for x, y in zip(xs, ys)) / span
        if lin_err(ds) < 0.01:
            return "linear"
        if all(d > 0 for d in ds) and lin_err([math.log(d) for d in ds]) < 0.02:
            return "logarithmic"
        return "curved"

    def to_display(self, x):
        p = self.pts
        if not p: return None
        if x <= p[0][0]: return p[0][1]
        for (x0, d0), (x1, d1) in zip(p, p[1:]):
            if x0 <= x <= x1:
                t = (x - x0) / ((x1 - x0) or 1e-12)
                if self.kind == "logarithmic":
                    return math.exp(math.log(d0) + t * (math.log(d1) - math.log(d0)))
                return d0 + t * (d1 - d0)
        return p[-1][1]

    def to_raw(self, d):
        """Inverse by piecewise interpolation (in log domain for logarithmic scales). Clamped."""
        if not self.monotonic:
            raise ValueError("display is not monotonic: absolute/relative in display units unsupported")
        p = self.pts if self.pts[0][1] <= self.pts[-1][1] else list(reversed(self.pts))
        if d <= p[0][1]: return p[0][0]
        if d >= p[-1][1]: return p[-1][0]
        for (x0, d0), (x1, d1) in zip(p, p[1:]):
            if d0 <= d <= d1:
                if self.kind == "logarithmic":
                    t = (math.log(d) - math.log(d0)) / ((math.log(d1) - math.log(d0)) or 1e-12)
                else:
                    t = (d - d0) / ((d1 - d0) or 1e-12)
                return x0 + t * (x1 - x0)
        return p[-1][0]

def refine_raw(c, pid, scale, target, raw, iters=2):
    """Refines an inversion by re-sampling the plug-in's own display function around the estimate."""
    span = (scale.max - scale.min) / max(1, len(scale.pts) - 1)
    for _ in range(iters):
        lo, hi = max(scale.min, raw - span), min(scale.max, raw + span)
        local = Scale(dict(c.curve(pid, 17, lo, hi), min=scale.min, max=scale.max))
        local.kind = scale.kind
        if not local.monotonic or len(local.pts) < 2:
            break
        raw = local.to_raw(target); span /= 8
    return raw

# --------------------------------------------------------------------------------------------- commands
def match_devices(doc, name, track=None):
    n = name.lower()
    out = [d for d in doc["devices"] if not d.get("isBridge") and (d["classDisplayName"].lower() == n or d["name"].lower() == n)]
    if track:
        out = [d for d in out if d["trackName"].lower() == track.lower()]
    return out

def param_of(c, dev, pname):
    pdoc = c.parameters(dev["id"])
    for p in pdoc["parameters"]:
        if p["name"].lower() == pname.lower() or p["originalName"].lower() == pname.lower():
            return p
    return None

def instances_with_param(c, doc, a):
    devs = match_devices(doc, a.device, getattr(a, "track", None))
    rows = []
    for d in devs:
        p = param_of(c, d, a.param)
        rows.append((d, p))
    return rows

def where(d):
    return "%-14s %s#%d%s" % (d["trackName"][:14], d["key"], d["position"], "  [rack: " + d["rackPath"].strip(" >") + "]" if d["rackPath"] else "")

def cmd_hello(c, a):
    print(json.dumps(c.hello(), indent=2))

def cmd_scan(c, a):
    info = c.hello(); doc = c.scan()
    print("Project id %s  Live %s  bridge %s  -  %d tracks, %d devices, scan %d ms\n" % (
        info["projectId"] or "(new)", info["liveVersion"], info["bridgeVersion"], len(doc["tracks"]), len(doc["devices"]), doc["durationMs"]))
    groups = {}
    for d in doc["devices"]:
        groups.setdefault((d["format"], d["classDisplayName"]), []).append(d)
    for (fmt, name), ds in sorted(groups.items(), key=lambda kv: (-len(kv[1]), kv[0])):
        print("%-4s %-34s %3d instance(s)  params=%s" % (fmt, name, len(ds), sorted(set(x["paramCount"] for x in ds))))
        for d in ds:
            print("        id %-6s %s  name=%r%s" % (d["id"], where(d), d["name"], "  (this bridge)" if d["isBridge"] else ""))

def cmd_params(c, a):
    c.hello(); doc = c.scan()
    devs = match_devices(doc, a.device, a.track)
    if not devs:
        sys.exit("no device matching %r" % a.device)
    d = devs[0]; pdoc = c.parameters(d["id"])
    print("%s on %s - %d parameters exposed by Live (class %s)\n" % (d["classDisplayName"], d["trackName"], len(pdoc["parameters"]), d["className"]))
    for p in pdoc["parameters"]:
        flags = ("Q " if p["isQuantized"] else "  ") + ("" if p["isEnabled"] else "DISABLED ") + ["", "inactive ", "disabled "][p["state"]]
        print("%3d  id %-7s %-30s %-18s raw %.6f [%g..%g] %s" % (p["index"], p["id"], p["name"][:30], p["display"][:18], p["value"], p["min"], p["max"], flags))
    if a.json:
        json.dump({"device": d, "parameters": pdoc["parameters"]}, open(a.json, "w"), indent=2)
        print("\nwritten", a.json)

def cmd_curve(c, a):
    c.hello(); doc = c.scan()
    d, p = instances_with_param(c, doc, a)[0]
    cv = c.curve(p["id"], 33); s = Scale(cv)
    print("%s / %s : scale=%s unit=%r monotonic=%s" % (d["classDisplayName"], p["name"], s.kind, s.unit, s.monotonic))
    for x, t in cv["samples"]:
        print("   raw %.4f -> %s" % (x, t))

def _rows_or_die(c, a):
    c.hello(); doc = c.scan()
    rows = instances_with_param(c, doc, a)
    if not rows:
        sys.exit("no device matching %r" % a.device)
    missing = [d for d, p in rows if p is None]
    for d in missing:
        print("! %s: parameter %r NOT exposed by Live for this instance - skipped" % (where(d), a.param))
    return [(d, p) for d, p in rows if p is not None]

def cmd_set(c, a):
    rows = _rows_or_die(c, a)
    scale = Scale(c.curve(rows[0][1]["id"], 65))
    pairs = []
    for d, p in rows:
        raw = a.raw if a.raw is not None else refine_raw(c, p["id"], scale, a.display, scale.to_raw(a.display))
        pairs.append((p["id"], raw))
    txn = c.set_many(pairs)
    got = c.collect([p for p, _ in pairs], source="gc", txn=txn)
    for (d, p), (_, raw) in zip(rows, pairs):
        v = got.get(str(p["id"]))
        print("%s  %s -> %s" % (where(d), p["display"], v[1] if v else "NO CONFIRMATION"))
    print("%d/%d instances confirmed by Live" % (len(got), len(pairs)))

def relative_targets(scale, rows, delta):
    out = []
    for d, p in rows:
        cur = parse_display(p["display"])[0]
        if scale.kind == "discrete":
            raw = max(scale.min, min(scale.max, round(p["value"] + delta)))
        elif cur is None or not math.isfinite(cur):
            raw = None
        else:
            raw = scale.to_raw(cur + delta)
        out.append(raw)
    return out

def cmd_relative(c, a):
    rows = _rows_or_die(c, a)
    scale = Scale(c.curve(rows[0][1]["id"], 65))
    print("scale %s - applying %+g %s to every instance\n" % (scale.kind, a.delta, "step(s)" if scale.kind == "discrete" else scale.unit))
    targets = relative_targets(scale, rows, a.delta)
    pairs = []
    for (d, p), raw in zip(rows, targets):
        if raw is None:
            print("! %s current display %r is not numeric - skipped" % (where(d), p["display"])); continue
        cur = parse_display(p["display"])[0]
        pairs.append((p["id"], refine_raw(c, p["id"], scale, cur + a.delta, raw) if scale.kind != "discrete" else raw))
    txn = c.set_many(pairs)
    got = c.collect([p for p, _ in pairs], source="gc", txn=txn)
    for d, p in rows:
        v = got.get(str(p["id"]))
        print("%s  %-12s -> %s" % (where(d), p["display"], v[1] if v else "-"))

def cmd_watch(c, a):
    rows = _rows_or_die(c, a)
    pids = [p["id"] for _, p in rows]
    names = {str(p["id"]): where(d) for d, p in rows}
    c.observe(pids)
    print("observing %d instances of %s / %s - move the parameter in a plug-in window (Ctrl-C to stop)" % (len(pids), a.device, a.param))
    try:
        while True:
            try:
                v = c.values.get(timeout=1.0)
            except queue.Empty:
                if time.time() - c.last_heartbeat > 4:
                    print("! no heartbeat from bridge")
                continue
            print("%s  %-24s  %-10s raw %.5f  [%s]" % (time.strftime("%H:%M:%S"), urllib.parse.unquote(v[2]), names.get(str(v[0]), v[0]), v[1], v[3]))
    except KeyboardInterrupt:
        c.observe(pids, on=False)

def cmd_phase_test(c, a):
    """Phases 2-7 of the brief, automatically, with restore of the original values at the end."""
    rep, ok = [], True
    def step(name, passed, detail=""):
        nonlocal ok
        ok = ok and passed
        rep.append("| %s | %s | %s |" % (name, "PASS" if passed else "FAIL", detail.replace("|", "/")))
        print(("PASS " if passed else "FAIL ") + name + ("  - " + detail if detail else ""))
    info = c.hello()
    step("Bridge handshake", True, "Live %s, bridge %s, project %s" % (info["liveVersion"], info["bridgeVersion"], info["projectId"]))
    doc = c.scan()
    rows = instances_with_param(c, doc, a)
    tracks = set(d["trackId"] for d, _ in rows)
    step("Phase 2 - instances found", len(rows) >= a.expect, "%d instances of %s on %d tracks (scan %d ms)" % (len(rows), a.device, len(tracks), doc["durationMs"]))
    readable = [(d, p) for d, p in rows if p is not None]
    step("Phase 3 - %s readable" % a.param, len(readable) == len(rows) and len(rows) > 0,
         ", ".join("%s=%s" % (d["trackName"], p["display"]) for d, p in readable))
    if not readable:
        return finish(a, rep, ok)
    scale = Scale(c.curve(readable[0][1]["id"], 65))
    step("Scale analysis", scale.monotonic, "kind=%s unit=%r, %d numeric samples" % (scale.kind, scale.unit, len(scale.pts)))
    orig = [(p["id"], p["value"]) for _, p in readable]
    try:
        # Phase 5/6: one command, every instance, absolute value
        target = a.absolute if a.absolute is not None else scale.to_display((scale.min + scale.max) / 2)
        raw = refine_raw(c, readable[0][1]["id"], scale, target, scale.to_raw(target))
        t0 = time.time(); txn = c.set_many([(pid, raw) for pid, _ in orig])
        got = c.collect([pid for pid, _ in orig], source="gc", txn=txn)
        lat = (time.time() - t0) * 1000
        vals = [parse_display(g[1])[0] for g in got.values()]
        tol = max(0.05, abs(target) * 0.01)
        step("Phase 5/6 - absolute %g %s on all" % (target, scale.unit), len(got) == len(orig) and all(v is not None and abs(v - target) <= tol for v in vals),
             "%d/%d confirmed in %.0f ms: %s" % (len(got), len(orig), lat, sorted(set(g[1] for g in got.values()))))
        # restore, then Phase 7: relative
        c.set_many(orig); c.collect([pid for pid, _ in orig], source="gc")
        fresh = c.get([pid for pid, _ in orig])
        cur_rows = [(d, dict(p, value=fresh[str(p["id"])][0], display=fresh[str(p["id"])][1])) for d, p in readable if str(p["id"]) in fresh]
        before = {str(p["id"]): parse_display(p["display"])[0] for _, p in cur_rows}
        targets = relative_targets(scale, cur_rows, a.delta)
        pairs = []
        for (d, p), r in zip(cur_rows, targets):
            if r is not None:
                pairs.append((p["id"], refine_raw(c, p["id"], scale, before[str(p["id"])] + a.delta, r)))
        txn = c.set_many(pairs)
        got = c.collect([pid for pid, _ in pairs], source="gc", txn=txn)
        diffs, detail = [], []
        for pid, _ in pairs:
            g = got.get(str(pid))
            if not g: continue
            after = parse_display(g[1])[0]
            exp = before[str(pid)] + a.delta
            lo, hi = sorted([scale.to_display(scale.min), scale.to_display(scale.max)])
            exp = max(lo, min(hi, exp))                     # per-instance clamping at the parameter limits
            diffs.append(abs(after - exp)); detail.append("%g->%g" % (before[str(pid)], after))
        step("Phase 7 - relative %+g %s preserves differences" % (a.delta, scale.unit),
             len(diffs) == len(pairs) and all(x <= max(0.05, abs(a.delta) * 0.05) for x in diffs), ", ".join(detail))
    finally:
        c.set_many(orig)
        got = c.collect([pid for pid, _ in orig], source="gc")
        step("Restore original values", len(got) == len(orig), "%d/%d restored" % (len(got), len(orig)))
    return finish(a, rep, ok)

def finish(a, rep, ok):
    md = "# GLOBAL CONTROL - PoC phase test\n\n%s, device `%s`, parameter `%s`\n\n| Step | Result | Detail |\n|---|---|---|\n%s\n\n**%s**\n" % (
        time.strftime("%Y-%m-%d %H:%M"), a.device, a.param, "\n".join(rep), "ALL PASSED" if ok else "SOME STEPS FAILED")
    if a.report:
        os.makedirs(os.path.dirname(os.path.abspath(a.report)), exist_ok=True)
        open(a.report, "w").write(md); print("\nreport written to", a.report)
    return 0 if ok else 1

def main():
    global APP_PORT, BRIDGE_PORT
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--app-port", type=int, default=APP_PORT)
    ap.add_argument("--bridge-port", type=int, default=BRIDGE_PORT)
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("hello"); sub.add_parser("scan")
    def dev(p, param=True):
        p.add_argument("--device", required=True); p.add_argument("--track")
        if param: p.add_argument("--param", required=True)
        return p
    p = dev(sub.add_parser("params"), False); p.add_argument("--json")
    dev(sub.add_parser("curve"))
    p = dev(sub.add_parser("set")); g = p.add_mutually_exclusive_group(required=True)
    g.add_argument("--display", type=float); g.add_argument("--raw", type=float)
    p = dev(sub.add_parser("relative")); p.add_argument("--delta", type=float, required=True)
    dev(sub.add_parser("watch"))
    p = dev(sub.add_parser("phase-test")); p.add_argument("--expect", type=int, default=10)
    p.add_argument("--absolute", type=float); p.add_argument("--delta", type=float, default=-2.0); p.add_argument("--report")
    a = ap.parse_args()
    APP_PORT, BRIDGE_PORT = a.app_port, a.bridge_port
    c = Client()
    try:
        r = {"hello": cmd_hello, "scan": cmd_scan, "params": cmd_params, "curve": cmd_curve, "set": cmd_set,
             "relative": cmd_relative, "watch": cmd_watch, "phase-test": cmd_phase_test}[a.cmd](c, a)
    except (BridgeError, TimeoutError) as e:
        sys.exit("error: %s" % e)
    finally:
        c.close()
    sys.exit(r or 0)

if __name__ == "__main__":
    main()
