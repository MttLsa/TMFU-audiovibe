// =====================================================================================================
// GLOBAL CONTROL BRIDGE  -  Max for Live JavaScript ([js] object, ES5 / SpiderMonkey compatible)
//
// Runs inside GLOBAL_CONTROL_BRIDGE.amxd (Max MIDI Effect) on ONE dedicated MIDI track per Live Set.
// Talks to the Live Object Model (LiveAPI) and to GLOBAL CONTROL.app through OSC over UDP on 127.0.0.1.
//
//   inlet 0  : messages from [udpreceive 47121] (OSC address = message selector), bang from
//              [live.thisdevice], "projectid <id>" from the [pattr gc_project_id] restore.
//   outlet 0 : OSC messages to [udpsend 127.0.0.1 47120]
//   outlet 1 : status text for the device UI
//   outlet 2 : project id to store in [pattr gc_project_id] (saved inside the Live Set)
//
// Protocol: see GLOBAL_CONTROL/Protocol/OSCSpecification/GC_OSC_PROTOCOL.md (protocol version 1).
// Uses only the public Live Object Model. Never touches audio. Never modifies, replaces or wraps devices:
// it only reads/writes DeviceParameter.value of devices that already exist in the Set.
// =====================================================================================================

autowatch = 0;
inlets = 1;
outlets = 3;

var PROTO = 1;
var BRIDGE_VERSION = "0.1.0";
var CHUNK_CHARS = 1000;          // raw JSON chars per OSC packet (URI-encoded afterwards, stays well < 8 KB)
var HEARTBEAT_MS = 1000;
var APP_TIMEOUT_MS = 4000;
var VALUE_FLUSH_MS = 33;         // outgoing value notifications: max ~30 Hz per parameter (coalesced)
var SET_FLUSH_MS = 5;            // incoming set commands are coalesced per parameter, latest wins
var ECHO_WINDOW_MS = 1500;       // a value change matching a pending GC write inside this window is our own echo
var MAX_MSGS_PER_SEC = 3000;     // incoming flood guard per app session
var MAX_RACK_DEPTH = 6;
var STRUCTURE_DEBOUNCE_MS = 400;

// ------------------------------------------------------------------------------------------------- state
var S = {
    ready: false,
    bridgeSession: "",
    nonce: "",
    appSession: "",
    appLastSeen: 0,
    appConnected: false,
    seqOut: 0,
    recent: {},            // seq -> 1 (duplicate protection for the current app session)
    recentOrder: [],
    rateWindowStart: 0,
    rateCount: 0,
    rateWarned: false,
    projectId: "",
    thisDeviceId: "0",
    thisTrackId: "0",
    startTime: 0,
    params: {},            // pid -> ParamEntry
    dirty: {},             // pid -> {source, txn}
    dirtyCount: 0,
    pendingSets: {},       // pid -> {value, txn, reqSeq}
    pendingSetCount: 0,
    songPlaying: 0,
    scanBusy: false
};

var tHeartbeat = null, tValues = null, tSets = null, tStructure = null, tProjectId = null, tScan = null;
var songObserver = null;
var structureObservers = [];

// ------------------------------------------------------------------------------------------------ helpers
function isArr(x) { return Object.prototype.toString.call(x) === "[object Array]"; }

function now() { return new Date().getTime(); }

function randHex(n) {
    var s = "", h = "0123456789abcdef";
    for (var i = 0; i < n; i++) s += h.charAt(Math.floor(Math.random() * 16));
    return s;
}

function str(api, prop) {
    var r = api.get(prop);
    if (r === undefined || r === null) return "";
    if (isArr(r)) return r.join(" ");
    return String(r);
}

function num(api, prop) {
    var r = api.get(prop);
    if (isArr(r)) r = r[0];
    var v = Number(r);
    return isNaN(v) ? 0 : v;
}

function idList(api, prop) {          // ["id", 3, "id", 7] -> ["3","7"]
    var r = api.get(prop), out = [];
    if (!isArr(r)) return out;
    for (var i = 0; i + 1 < r.length; i += 2) if (r[i] === "id") out.push(String(r[i + 1]));
    return out;
}

function status(text) { outlet(1, "set", text); }

function log(text) { post("[GLOBAL CONTROL] " + text + "\n"); }

// Cheap 32-bit FNV-1a hash (identity fingerprints, not security).
function fnv1a(s) {
    var h = 0x811c9dc5;
    for (var i = 0; i < s.length; i++) {
        h ^= s.charCodeAt(i);
        h = (h + ((h << 1) + (h << 4) + (h << 7) + (h << 8) + (h << 24))) >>> 0;
    }
    return ("00000000" + h.toString(16)).slice(-8);
}

// --------------------------------------------------------------------------------------------- OSC output
function send(addr, payload) {
    S.seqOut++;
    var msg = [addr, PROTO, S.bridgeSession, S.seqOut];
    for (var i = 0; i < payload.length; i++) msg.push(payload[i]);
    outlet(0, msg);
}

// Bulk JSON goes out in URI-encoded chunks so that Max never sees braces/commas/semicolons inside symbols.
function sendChunked(addr, head, obj) {
    var json = JSON.stringify(obj);
    var pieces = [], pos = 0;
    while (pos < json.length) {
        var end = Math.min(json.length, pos + CHUNK_CHARS);
        var c = json.charCodeAt(end - 1);
        if (end < json.length && c >= 0xD800 && c <= 0xDBFF) end--;   // never split a surrogate pair
        pieces.push(encodeURIComponent(json.substring(pos, end)));
        pos = end;
    }
    if (pieces.length === 0) pieces.push("");
    for (var p = 0; p < pieces.length; p++) send(addr, head.concat([p, pieces.length, pieces[p]]));
}

function sendError(reqSeq, code, message) {
    send("/global/error", [reqSeq, code, encodeURIComponent(message)]);
}

// --------------------------------------------------------------------------------------------- lifecycle
function bang() {              // from [live.thisdevice]: the Live API is now usable
    if (S.ready) return;
    S.ready = true;
    S.startTime = now();
    S.bridgeSession = randHex(16);
    var me = new LiveAPI(null, "this_device");
    S.thisDeviceId = String(me.id);
    var path = String(me.unquotedpath || me.path).replace(/"/g, "");
    var m = path.match(/^(live_set (?:tracks|return_tracks) \d+|live_set master_track)/);
    if (m) { var t = new LiveAPI(null, m[1]); S.thisTrackId = String(t.id); }

    songObserver = new LiveAPI(function (a) { if (a[0] === "is_playing") S.songPlaying = Number(a[1]) || 0; }, "live_set");
    songObserver.property = "is_playing";

    tHeartbeat = new Task(heartbeatTick, this); tHeartbeat.interval = HEARTBEAT_MS; tHeartbeat.repeat();
    tValues = new Task(flushValues, this); tValues.interval = VALUE_FLUSH_MS; tValues.repeat();
    tSets = new Task(flushSets, this);
    tStructure = new Task(structureChanged, this);
    tProjectId = new Task(ensureProjectId, this); tProjectId.schedule(1500);   // after a possible pattr restore
    installStructureObservers();
    status("Waiting for GLOBAL CONTROL...");
    log("bridge " + BRIDGE_VERSION + " ready, session " + S.bridgeSession);
    send("/global/hello", [0, "", S.projectId, liveVersion(), BRIDGE_VERSION, S.thisDeviceId]);   // unsolicited announce
}

function projectid(v) {        // restored from [pattr gc_project_id] when the Set is loaded
    var s = (v === undefined || v === null) ? "" : String(v);
    if (s.length >= 8 && s !== "0") S.projectId = s;
}

function ensureProjectId() {
    if (S.projectId === "") {
        S.projectId = "gcp-" + randHex(20);
        outlet(2, S.projectId);           // stored inside the Live Set when the user saves it
        log("new project id " + S.projectId);
    }
}

function notifydeleted() {
    var ts = [tHeartbeat, tValues, tSets, tStructure, tProjectId, tScan];
    for (var i = 0; i < ts.length; i++) if (ts[i]) ts[i].cancel();
    clearObservers();
    clearStructureObservers();
    if (songObserver) songObserver.property = "";
}

function liveVersion() {
    try {
        var app = new LiveAPI(null, "live_app");
        return app.call("get_major_version") + "." + app.call("get_minor_version") + "." + app.call("get_bugfix_version");
    } catch (e) { return "unknown"; }
}

function heartbeatTick() {
    var t = now();
    var connected = S.appSession !== "" && (t - S.appLastSeen) < APP_TIMEOUT_MS;
    if (connected !== S.appConnected) {
        S.appConnected = connected;
        status(connected ? "Connected to GLOBAL CONTROL" : "Waiting for GLOBAL CONTROL...");
    }
    send("/global/heartbeat", [t - S.startTime, connected ? 1 : 0, S.projectId]);
}

// ---------------------------------------------------------------------------------------- OSC input
function anything() {
    if (!S.ready) return;
    var addr = messagename;
    var a = arrayfromargs(arguments);
    try { dispatch(addr, a); }
    catch (e) { log("error handling " + addr + ": " + e); sendError(a.length > 2 ? Number(a[2]) || 0 : 0, "E_INTERNAL", String(e)); }
}

// Envelope (app -> bridge): proto:int, appSession:string, seq:int, token:string, payload...
function dispatch(addr, a) {
    if (a.length < 4) { sendError(0, "E_PROTO", "short message " + addr); return; }
    var proto = Number(a[0]), session = String(a[1]), seq = Number(a[2]), token = String(a[3]);
    var p = a.slice(4);
    if (proto !== PROTO) { sendError(seq, "E_PROTO", "protocol " + proto + " not supported, bridge speaks " + PROTO); return; }
    if (!rateOk()) return;

    if (addr === "/global/hello") { onHello(session, seq, p); return; }

    // every other command needs the nonce handed out in the hello reply (only readable on 127.0.0.1)
    if (session !== S.appSession || token !== S.nonce || S.nonce === "") { sendError(seq, "E_AUTH", "send /global/hello first"); return; }
    if (isDuplicate(seq)) return;
    S.appLastSeen = now();

    switch (addr) {
        case "/global/heartbeat": return;
        case "/global/scan/request": return onScan(seq, p);
        case "/global/device/parameters": return onDeviceParameters(seq, p);
        case "/global/parameter/describe": return onDescribe(seq, p);
        case "/global/parameter/set": return onSet(seq, p);
        case "/global/parameter/get": return onGet(seq, p);
        case "/global/parameter/observe": return onObserve(seq, p);
        case "/global/project/setid": return onSetProjectId(seq, p);
        default: sendError(seq, "E_UNKNOWN", "unknown address " + addr);
    }
}

function rateOk() {
    var t = now();
    if (t - S.rateWindowStart >= 1000) { S.rateWindowStart = t; S.rateCount = 0; S.rateWarned = false; }
    if (++S.rateCount <= MAX_MSGS_PER_SEC) return true;
    if (!S.rateWarned) { S.rateWarned = true; sendError(0, "E_RATE", "more than " + MAX_MSGS_PER_SEC + " msg/s, dropping"); }
    return false;
}

function isDuplicate(seq) {
    if (S.recent[seq]) return true;
    S.recent[seq] = 1;
    S.recentOrder.push(seq);
    if (S.recentOrder.length > 512) delete S.recent[S.recentOrder.shift()];
    return false;
}

function onHello(session, seq, p) {
    if (session.length < 4) { sendError(seq, "E_PROTO", "invalid session id"); return; }
    if (session !== S.appSession) {             // new app instance (or restart): reset per-session state
        S.appSession = session;
        S.recent = {}; S.recentOrder = [];
        clearObservers();
    }
    S.nonce = randHex(16);
    S.appLastSeen = now();
    send("/global/hello", [seq, S.nonce, S.projectId, liveVersion(), BRIDGE_VERSION, S.thisDeviceId]);
    log("hello from " + (p.length ? p[0] : "?") + " session " + session);
}

function onSetProjectId(seq, p) {
    var id = String(p[0] || "");
    if (id.length < 8) { sendError(seq, "E_PROTO", "project id too short"); return; }
    S.projectId = id; outlet(2, id);
    send("/global/hello", [seq, S.nonce, S.projectId, liveVersion(), BRIDGE_VERSION, S.thisDeviceId]);
}

// -------------------------------------------------------------------------------- DEVICE DISCOVERY ENGINE
// The scan is split in Task ticks (one track per tick) so Live's UI thread is never blocked for long.
function onScan(seq, p) {
    if (S.scanBusy) { sendError(seq, "E_BUSY", "scan already running"); return; }
    var scanId = Number(p[0]) || seq;
    var root = new LiveAPI(null, "live_set");
    var jobs = [], i;
    var nt = root.getcount("tracks"), nr = root.getcount("return_tracks");
    for (i = 0; i < nt; i++) jobs.push({ path: "live_set tracks " + i, key: "t" + i, kind: "track", index: i });
    for (i = 0; i < nr; i++) jobs.push({ path: "live_set return_tracks " + i, key: "r" + i, kind: "return", index: i });
    jobs.push({ path: "live_set master_track", key: "m", kind: "master", index: 0 });

    var ctx = { seq: seq, scanId: scanId, jobs: jobs, next: 0, tracks: [], devices: [], started: now(), walker: new LiveAPI(null, "live_set") };
    S.scanBusy = true;
    tScan = new Task(function () { scanStep(ctx); }, this);
    tScan.interval = 1;
    tScan.repeat();
}

function scanStep(ctx) {
    if (ctx.next >= ctx.jobs.length) {
        tScan.cancel(); S.scanBusy = false;
        sendChunked("/global/device/list", [ctx.seq, ctx.scanId], { scanId: ctx.scanId, projectId: S.projectId, tracks: ctx.tracks, devices: ctx.devices });
        send("/global/scan/complete", [ctx.seq, ctx.scanId, ctx.tracks.length, ctx.devices.length, now() - ctx.started]);
        status("Scan: " + ctx.tracks.length + " tracks, " + ctx.devices.length + " devices");
        return;
    }
    var job = ctx.jobs[ctx.next++];
    try { scanTrack(ctx, job); }
    catch (e) { log("scan error on " + job.path + ": " + e); }
}

function scanTrack(ctx, job) {
    var w = ctx.walker;
    w.goto(job.path);
    var track = {
        id: String(w.id), key: job.key, kind: job.kind, index: job.index,
        name: str(w, "name"), color: num(w, "color"),
        isMidi: job.kind === "track" ? num(w, "has_midi_input") : 0,
        isGroup: job.kind === "track" ? num(w, "is_foldable") : 0,
        isBridgeTrack: String(w.id) === S.thisTrackId ? 1 : 0
    };
    ctx.tracks.push(track);
    scanChain(ctx, job.path, job.key, track, 0, "");
}

function scanChain(ctx, chainPath, key, track, depth, rackPath) {
    var w = ctx.walker;
    w.goto(chainPath);
    var n = w.getcount("devices");
    for (var i = 0; i < n; i++) {
        var dp = chainPath + " devices " + i, dkey = key + "/d" + i;
        w.goto(dp);
        var d = describeDevice(w, track, dkey, i, depth, rackPath);
        ctx.devices.push(d);
        if (d.canHaveChains && depth < MAX_RACK_DEPTH) {
            var nc = w.getcount("chains");
            for (var c = 0; c < nc; c++) scanChain(ctx, dp + " chains " + c, dkey + "/c" + c, track, depth + 1, rackPath + d.name + " > ");
            w.goto(dp);
            var nrc = w.getcount("return_chains");
            for (var r = 0; r < nrc; r++) scanChain(ctx, dp + " return_chains " + r, dkey + "/rc" + r, track, depth + 1, rackPath + d.name + " > ");
        }
        w.goto(chainPath);
    }
}

function describeDevice(w, track, key, position, depth, rackPath) {
    var cls = str(w, "class_name");
    var id = String(w.id);
    var pids = idList(w, "parameters");
    var nParams = pids.length;
    var names = [], pa = new LiveAPI(null, "live_set");
    for (var k = 0; k < nParams; k++) {
        pa.id = Number(pids[k]);
        names.push(str(pa, "original_name"));
    }
    var isPlugin = (cls === "PluginDevice" || cls === "AuPluginDevice");
    return {
        id: id, key: key, trackId: track.id, trackName: track.name, trackKey: track.key, trackKind: track.kind,
        position: position, depth: depth, rackPath: rackPath,
        name: str(w, "name"),                       // user-visible name (may be renamed by the user)
        className: cls,                             // PluginDevice (VST2/VST3), AuPluginDevice (AU), Compressor2, ...
        classDisplayName: str(w, "class_display_name"),
        type: num(w, "type"),                       // 1 audio fx, 2 instrument, 4 midi fx
        isPlugin: isPlugin ? 1 : 0,
        format: cls === "AuPluginDevice" ? "AU" : (cls === "PluginDevice" ? "VST" : "Live"),
        manufacturer: null,                         // not exposed by the Live Object Model (documented limitation)
        isActive: num(w, "is_active"),
        canHaveChains: num(w, "can_have_chains"),
        paramCount: nParams,
        paramNamesHash: fnv1a(names.join("\u001f")),
        isBridge: id === S.thisDeviceId ? 1 : 0
    };
}

// ------------------------------------------------------------------------------ PARAMETER DISCOVERY ENGINE
function onDeviceParameters(seq, p) {
    var deviceId = String(p[0]);
    var dev = new LiveAPI(null, "id " + deviceId);
    if (String(dev.id) === "0" || dev.type === undefined || String(dev.type).indexOf("Device") < 0) {
        sendError(seq, "E_NOT_FOUND", "device " + deviceId + " not found"); return;
    }
    var ids = idList(dev, "parameters"), out = [];
    for (var i = 0; i < ids.length; i++) out.push(describeParam(ids[i], i, deviceId));
    sendChunked("/global/device/parameters", [seq, deviceId], {
        deviceId: deviceId, name: str(dev, "name"), className: str(dev, "class_name"),
        classDisplayName: str(dev, "class_display_name"), parameters: out
    });
}

function describeParam(pid, index, deviceId) {
    var pa = new LiveAPI(null, "id " + pid);
    var q = num(pa, "is_quantized");
    var v = num(pa, "value");
    var d = {
        id: String(pid), index: index, deviceId: String(deviceId),
        name: str(pa, "name"), originalName: str(pa, "original_name"),
        value: v, min: num(pa, "min"), max: num(pa, "max"),
        isQuantized: q,
        valueItems: q ? valueItems(pa) : [],
        display: safeStrForValue(pa, v),
        state: num(pa, "state"),                // 0 active, 1 inactive, 2 disabled
        isEnabled: num(pa, "is_enabled"),       // 0 when e.g. controlled by a macro/modulation
        automationState: num(pa, "automation_state"),
        defaultValue: null
    };
    if (!q) { try { d.defaultValue = num(pa, "default_value"); } catch (e) { d.defaultValue = null; } }
    d.normalized = (d.max > d.min) ? (v - d.min) / (d.max - d.min) : 0;
    return d;
}

function valueItems(pa) {
    var r = pa.get("value_items");
    return isArr(r) ? r.map(function (x) { return String(x); }) : [];
}

function safeStrForValue(pa, v) {
    try {
        var r = pa.call("str_for_value", v);
        if (isArr(r)) r = r.join(" ");
        return r === undefined || r === null ? "" : String(r);
    } catch (e) { return ""; }
}

// Samples the parameter's own display function (str_for_value) across its range so the app can learn the
// real scale (linear / logarithmic / exponential / discrete) of a plug-in parameter. Nothing is invented:
// the curve is exactly what the plug-in reports for those values.
function onDescribe(seq, p) {
    var pid = String(p[0]);
    var n = Math.max(3, Math.min(257, Number(p[1]) || 33));
    var pa = new LiveAPI(null, "id " + pid);
    if (String(pa.id) === "0") { sendError(seq, "E_NOT_FOUND", "parameter " + pid + " not found"); return; }
    var mn = num(pa, "min"), mx = num(pa, "max"), q = num(pa, "is_quantized");
    var lo = mn, hi = mx;                        // optional sub-range (used to refine a display->value inversion)
    if (p.length >= 4 && !q) { lo = Math.max(mn, Number(p[2])); hi = Math.min(mx, Number(p[3])); if (!(hi > lo)) { lo = mn; hi = mx; } }
    var samples = [];
    if (q) {
        for (var v = Math.round(mn); v <= Math.round(mx); v++) samples.push([v, safeStrForValue(pa, v)]);
    } else {
        for (var i = 0; i < n; i++) { var x = lo + (hi - lo) * i / (n - 1); samples.push([x, safeStrForValue(pa, x)]); }
    }
    sendChunked("/global/parameter/curve", [seq, pid], { paramId: pid, min: mn, max: mx, lo: lo, hi: hi, isQuantized: q, samples: samples });
}

// --------------------------------------------------------------------------------------- PARAMETER ENGINE
// ParamEntry: cached LiveAPI per parameter, created lazily, validated on creation.
function entry(pid) {
    var e = S.params[pid];
    if (e && String(e.api.id) !== "0") return e;
    var api = new LiveAPI(null, "id " + pid);
    if (String(api.id) === "0" || String(api.type) !== "DeviceParameter") { delete S.params[pid]; return null; }
    e = { pid: pid, api: api, observer: null, min: num(api, "min"), max: num(api, "max"),
          quantized: num(api, "is_quantized"), pending: null, lastValue: num(api, "value") };
    S.params[pid] = e;
    return e;
}

// /global/parameter/set  token, txn:int, n:int, (pid:int, value:float) * n
function onSet(seq, p) {
    var txn = Number(p[0]) || seq, n = Number(p[1]) || 0;
    if (p.length < 2 + 2 * n) { sendError(seq, "E_PROTO", "set: expected " + n + " pairs"); return; }
    for (var i = 0; i < n; i++) {
        var pid = String(p[2 + 2 * i]), v = Number(p[3 + 2 * i]);
        if (isNaN(v)) { sendError(seq, "E_PROTO", "set: NaN for " + pid); continue; }
        if (!S.pendingSets[pid]) S.pendingSetCount++;
        S.pendingSets[pid] = { value: v, txn: txn, seq: seq };     // coalesced: latest value wins
    }
    // LOM changes may not be triggered from inside notifications: always apply from a scheduled Task.
    tSets.schedule(SET_FLUSH_MS);
}

function flushSets() {
    if (S.pendingSetCount === 0) return;
    var batch = S.pendingSets;
    S.pendingSets = {}; S.pendingSetCount = 0;
    for (var pid in batch) {
        var b = batch[pid], e = entry(pid);
        if (!e) { sendError(b.seq, "E_NOT_FOUND", "parameter " + pid + " no longer exists"); continue; }
        if (num(e.api, "is_enabled") === 0) { sendError(b.seq, "E_NOT_ENABLED", "parameter " + pid + " is not enabled (macro/modulation)"); continue; }
        var v = Math.max(e.min, Math.min(e.max, b.value));
        if (e.quantized) v = Math.round(v);
        e.pending = { value: v, t: now(), txn: b.txn };
        var unchanged = (v === e.lastValue);
        e.api.set("value", v);
        if (!e.observer || unchanged) {     // not observed (or no change -> no notification): confirm now                 // not observed: confirm the effective value right away
            markDirty(pid, "gc", b.txn);
            e.pending = null;
        }
    }
}

// /global/parameter/get  token, n, pid*n
function onGet(seq, p) {
    var n = Number(p[0]) || 0;
    for (var i = 0; i < n; i++) {
        var pid = String(p[1 + i]), e = entry(pid);
        if (!e) { sendError(seq, "E_NOT_FOUND", "parameter " + pid + " not found"); continue; }
        markDirty(pid, "query", seq);
    }
}

// /global/parameter/observe  token, on:int, n:int, pid*n
function onObserve(seq, p) {
    var on = Number(p[0]) === 1, n = Number(p[1]) || 0;
    for (var i = 0; i < n; i++) {
        var pid = String(p[2 + i]);
        var e = entry(pid);
        if (!e) { sendError(seq, "E_NOT_FOUND", "parameter " + pid + " not found"); continue; }
        if (on && !e.observer) {
            e.observer = new LiveAPI(makeObserverCallback(pid), "id " + pid);
            e.observer.property = "value";
        } else if (!on && e.observer) {
            e.observer.property = "";
            e.observer = null;
        }
        markDirty(pid, "query", seq);
    }
}

function makeObserverCallback(pid) {
    return function (args) {
        if (!args || args[0] !== "value") return;
        var e = S.params[pid];
        if (!e) return;
        var v = Number(args[1]);
        if (v === e.lastValue) return;           // also swallows the immediate callback when observation starts
        e.lastValue = v;
        markDirty(pid, classifySource(e, v), e.lastTxn || 0);
    };
}

// Who changed the value? The LOM does not report the origin of a change, so this is a best-effort
// classification (documented in Documentation/TECHNICAL_ANALYSIS.md):
//   gc         - matches a value written by GLOBAL CONTROL inside ECHO_WINDOW_MS (feedback-loop guard)
//   automation - the parameter's automation is playing and the transport is running
//   external   - anything else: plug-in GUI, Live device view, MIDI/remote-script controller, Push...
function classifySource(e, v) {
    var t = now();
    if (e.pending && (t - e.pending.t) < ECHO_WINDOW_MS) {
        var tol = Math.max(1e-6, (e.max - e.min) * 1e-4);
        if (Math.abs(v - e.pending.value) <= tol) { e.lastTxn = e.pending.txn; e.pending = null; return "gc"; }
    }
    e.lastTxn = 0;
    if (S.songPlaying && num(e.api, "automation_state") === 1) return "automation";
    return "external";
}

function markDirty(pid, source, txn) {
    var prev = S.dirty[pid];
    if (!prev) S.dirtyCount++;
    // within one flush frame the most recent origin wins, except that an external/automation change
    // arriving after our own write is reported as such (the user must see who moved it last)
    S.dirty[pid] = { source: source, txn: source === "gc" ? txn : 0 };
}

// /global/parameter/value  pid, value, display(uri), source, txn
function flushValues() {
    if (S.dirtyCount === 0) return;
    var d = S.dirty;
    S.dirty = {}; S.dirtyCount = 0;
    for (var pid in d) {
        var e = S.params[pid];
        if (!e || String(e.api.id) === "0") { send("/global/instance/status", ["param_removed", pid]); delete S.params[pid]; continue; }
        var v = num(e.api, "value");
        e.lastValue = v;
        send("/global/parameter/value", [Number(pid), v, encodeURIComponent(safeStrForValue(e.api, v)), d[pid].source, d[pid].txn || 0]);
    }
}

function clearObservers() {
    for (var pid in S.params) if (S.params[pid].observer) { S.params[pid].observer.property = ""; S.params[pid].observer = null; }
    S.params = {}; S.dirty = {}; S.dirtyCount = 0;
}

// --------------------------------------------------------------------------- project structure watching
// Track list and every track's device list are observed. Any change (track/device added, removed, moved,
// rack chains edited at top level) is debounced and reported; the app then re-scans and reconciles.
function installStructureObservers() {
    clearStructureObservers();
    S.installingObservers = true;          // observers fire once, synchronously, when installed: ignore that
    var cb = function (args) {
        if (!args || S.installingObservers) return;
        if (args[0] === "tracks" || args[0] === "return_tracks" || args[0] === "devices" || args[0] === "chains") {
            tStructure.schedule(STRUCTURE_DEBOUNCE_MS);
        }
    };
    var root = new LiveAPI(cb, "live_set"); root.property = "tracks"; structureObservers.push(root);
    var rr = new LiveAPI(cb, "live_set"); rr.property = "return_tracks"; structureObservers.push(rr);
    var probe = new LiveAPI(null, "live_set");
    var paths = [], i;
    for (i = 0; i < probe.getcount("tracks"); i++) paths.push("live_set tracks " + i);
    for (i = 0; i < probe.getcount("return_tracks"); i++) paths.push("live_set return_tracks " + i);
    paths.push("live_set master_track");
    for (i = 0; i < paths.length; i++) {
        var o = new LiveAPI(cb, paths[i]); o.property = "devices"; structureObservers.push(o);
    }
    S.installingObservers = false;
}

function clearStructureObservers() {
    for (var i = 0; i < structureObservers.length; i++) structureObservers[i].property = "";
    structureObservers = [];
}

function structureChanged() {
    installStructureObservers();          // new tracks need their own "devices" observer
    send("/global/instance/status", ["structure_changed", 0]);
    status("Project changed - rescan suggested");
}
