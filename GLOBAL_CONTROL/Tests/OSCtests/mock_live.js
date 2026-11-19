// =====================================================================================================
// TEST HARNESS ONLY - a fake Live Object Model + fake Max [js] runtime used to unit-test gc_bridge.js
// on machines without Ableton Live (CI, Linux). It is NOT part of the product and is never shipped.
//
// The "Pro-C 2" fixture below is HYPOTHETICAL: parameter names, ranges and display strings are
// placeholders shaped like a typical VST3 compressor (normalized 0..1 values, text from the plug-in).
// The real list of parameters exposed by Live for FabFilter Pro-C 2 must be captured on a Mac with
// Tests/AbletonIntegrationTests/gc_poc.py (command "params") - see Documentation/TEST_PLAN.md.
// =====================================================================================================
'use strict';
const fs = require('fs');
const vm = require('vm');
const path = require('path');

const BRIDGE_JS = path.join(__dirname, '..', '..', 'AbletonBridge', 'LiveAPI', 'gc_bridge.js');

// ------------------------------------------------------------------------------------------ fake LOM
class FakeLive {
  constructor(idOffset = 0) {
    this.nextId = 1 + idOffset;    // a reopened Live Set gets different runtime ids
    this.objs = new Map();     // id -> {id, type, props, lists, fns, parent, path}
    this.observers = [];       // {api, id, prop}
    this.song = this.make('Song', { is_playing: 0 }, 'live_set');
    this.song.lists = { tracks: [], return_tracks: [] };
    this.app = this.make('Application', {}, 'live_app');
    this.app.fns = { get_major_version: () => 12, get_minor_version: () => 1, get_bugfix_version: () => 5 };
    this.master = this.addTrack('Main', { master: true });
    this.thisDevice = null;
  }

  make(type, props, p) {
    const o = { id: this.nextId++, type, props: Object.assign({}, props), lists: {}, fns: {}, path: p, alive: true };
    this.objs.set(o.id, o);
    return o;
  }

  addTrack(name, opt = {}) {
    let t;
    if (opt.master) {
      t = this.make('Track', { name, color: 0, has_midi_input: 0, is_foldable: 0 }, 'live_set master_track');
    } else {
      const list = opt.ret ? 'return_tracks' : 'tracks';
      t = this.make('Track', { name, color: 0x3366ff, has_midi_input: opt.midi ? 1 : 0, is_foldable: 0 }, '');
      this.song.lists[list].push(t.id);
      this.notify(this.song, list);
    }
    t.lists.devices = [];
    this.repath();
    return t;
  }

  addDevice(host, spec, index) {
    const d = this.make(spec.type || 'Device', {
      name: spec.name, class_name: spec.className, class_display_name: spec.classDisplayName || spec.name,
      type: spec.devType || 1, is_active: 1, can_have_chains: spec.chains ? 1 : 0
    }, '');
    d.lists.parameters = [];
    if (spec.chains) d.lists.chains = []; if (spec.chains) d.lists.return_chains = [];
    for (const ps of spec.params || []) {
      const p = this.make('DeviceParameter', {
        name: ps.name, original_name: ps.name, value: ps.value, min: ps.min ?? 0, max: ps.max ?? 1,
        is_quantized: ps.items ? 1 : 0, value_items: ps.items || [], state: 0, is_enabled: 1,
        automation_state: 0, default_value: ps.value
      }, '');
      p.fns.str_for_value = ps.display;
      d.lists.parameters.push(p.id);
    }
    const arr = host.lists.devices;
    arr.splice(index === undefined ? arr.length : index, 0, d.id);
    this.repath();
    this.notify(host, 'devices');
    return d;
  }

  addChain(rack, name) {
    const c = this.make('Chain', { name }, '');
    c.lists.devices = [];
    rack.lists.chains.push(c.id);
    this.repath();
    return c;
  }

  removeDevice(host, d) {
    host.lists.devices = host.lists.devices.filter(x => x !== d.id);
    const kill = (o) => { o.alive = false; this.objs.delete(o.id); for (const l of Object.values(o.lists)) for (const id of l) { const c = this.objs.get(id); if (c) kill(c); } };
    kill(d);
    this.repath();
    this.notify(host, 'devices');
  }

  repath() {
    const walk = (o, p) => {
      o.path = p;
      for (const [k, l] of Object.entries(o.lists)) l.forEach((id, i) => { const c = this.objs.get(id); if (c) walk(c, `${p} ${k} ${i}`); });
    };
    this.song.lists.tracks.forEach((id, i) => walk(this.objs.get(id), `live_set tracks ${i}`));
    this.song.lists.return_tracks.forEach((id, i) => walk(this.objs.get(id), `live_set return_tracks ${i}`));
    if (this.master) walk(this.master, 'live_set master_track');
  }

  resolve(p) {
    p = String(p).replace(/"/g, '').trim();
    if (p === 'this_device') return this.thisDevice;
    if (p === 'live_app') return this.app;
    let m = p.match(/^id (\d+)$/);
    if (m) { const o = this.objs.get(Number(m[1])); return o && o.alive ? o : null; }
    const tok = p.split(/\s+/);
    if (tok[0] !== 'live_set') return null;
    let o = this.song;
    for (let i = 1; i < tok.length; i++) {
      if (tok[i] === 'master_track') { o = this.master; continue; }
      const list = o.lists[tok[i]]; const idx = Number(tok[i + 1]); i++;
      if (!list || !(idx in list)) return null;
      o = this.objs.get(list[idx]);
    }
    return o;
  }

  notify(o, prop) {
    for (const ob of this.observers) if (ob.id === o.id && ob.prop === prop) ob.api._fire();
  }

  // a change made "by the user" in the plug-in GUI / by automation / by a controller
  externalSet(paramId, v) { const p = this.objs.get(paramId); p.props.value = v; this.notify(p, 'value'); }
}

function makeLiveAPI(live) {
  return class LiveAPI {
    constructor(cb, p) { this._cb = cb; this._prop = ''; this._o = live.resolve(p); }
    goto(p) { this._o = live.resolve(p); }
    get id() { return this._o && this._o.alive ? String(this._o.id) : '0'; }
    set id(n) { const o = live.objs.get(Number(n)); this._o = o || null; }
    get type() { return this._o ? this._o.type : undefined; }
    get path() { return this._o ? `"${this._o.path}"` : ''; }
    get unquotedpath() { return this._o ? this._o.path : ''; }
    get(prop) {
      const o = this._o; if (!o || !o.alive) return [];
      if (o.lists[prop]) return o.lists[prop].flatMap(id => ['id', id]);
      if (prop in o.props) { const v = o.props[prop]; return Array.isArray(v) ? v.slice() : [v]; }
      throw new Error(`get: no property ${prop} on ${o.type}`);
    }
    getcount(prop) { return this._o && this._o.lists[prop] ? this._o.lists[prop].length : 0; }
    set(prop, v) {
      const o = this._o; if (!o) return;
      if (live.inNotification) throw new Error('Changes cannot be triggered by notifications');
      o.props[prop] = v; live.writes.push({ id: o.id, prop, v });
      live.notify(o, prop);
    }
    call(fn, ...args) { const f = this._o && this._o.fns[fn]; if (!f) throw new Error('no function ' + fn); return f(...args); }
    get property() { return this._prop; }
    set property(p) {
      live.observers = live.observers.filter(o => o.api !== this);
      this._prop = p;
      if (p && this._o) { live.observers.push({ api: this, id: this._o.id, prop: p }); this._fire(); }
    }
    _fire() {
      if (!this._cb || !this._o || !this._o.alive) return;
      const o = this._o; const v = o.lists[this._prop] ? o.lists[this._prop].flatMap(id => ['id', id]) : [o.props[this._prop]];
      live.inNotification = true;
      try { this._cb([this._prop, ...v]); } finally { live.inNotification = false; }
    }
  };
}

// --------------------------------------------------------------------------------- fake Max js runtime
function loadBridge(live, onOutlet) {
  live.writes = [];
  const tasks = [];
  class Task {
    constructor(fn, ctx) { this.fn = fn; this.ctx = ctx; this.interval = 0; this.h = null; tasks.push(this); }
    repeat() { this.cancel(); this.h = setInterval(() => this.fn.call(this.ctx), Math.max(1, this.interval)); }
    schedule(ms) { this.cancel(); this.h = setTimeout(() => { this.h = null; this.fn.call(this.ctx); }, ms || 0); }
    cancel() { if (this.h) { clearInterval(this.h); clearTimeout(this.h); this.h = null; } }
  }
  const ctx = {
    LiveAPI: makeLiveAPI(live), Task,
    outlet: (n, ...a) => onOutlet(n, a.length === 1 && Array.isArray(a[0]) ? a[0] : a),
    post: (s) => { if (process.env.GC_VERBOSE) process.stdout.write(s); },
    arrayfromargs: (args) => Array.prototype.slice.call(args),
    messagename: '', encodeURIComponent, decodeURIComponent, JSON, Math, Date, Number, String, isNaN
  };
  vm.createContext(ctx);
  vm.runInContext(fs.readFileSync(BRIDGE_JS, 'utf8'), ctx, { filename: 'gc_bridge.js' });
  return {
    ctx,
    stop() { ctx.notifydeleted(); for (const t of tasks) t.cancel(); },
    message(addr, args) { ctx.messagename = addr; ctx.anything(...args); },
    bang() { ctx.bang(); },
    projectid(v) { ctx.projectid(v); }
  };
}

// ------------------------------------------------------------------------------------- test fixture
const fmt = (v, d) => v.toFixed(d);
function proC2Params(thresholdNorm) {
  // HYPOTHETICAL parameter set (see header). Values normalized 0..1 as Live reports for VST3/AU plug-ins.
  return [
    { name: 'Style', value: 0, min: 0, max: 7, items: ['Clean', 'Classic', 'Opto', 'Vocal', 'Mastering', 'Bus', 'Punch', 'Pumping'],
      display: v => ['Clean', 'Classic', 'Opto', 'Vocal', 'Mastering', 'Bus', 'Punch', 'Pumping'][Math.round(v)] },
    { name: 'Threshold', value: thresholdNorm, display: v => fmt(-60 + 60 * v, 2) + ' dB' },                       // linear dB
    { name: 'Ratio', value: 0.3, display: v => fmt(Math.pow(20, v), 2) + ':1' },                                  // exponential
    { name: 'Attack', value: 0.5, display: v => fmt(0.005 * Math.pow(250 / 0.005, v), 3) + ' ms' },                // logarithmic
    { name: 'Release', value: 0.4, display: v => fmt(10 * Math.pow(250, v), 1) + ' ms' },
    { name: 'Knee', value: 0.2, display: v => fmt(72 * v, 1) + ' dB' },
    { name: 'Output Level', value: 0.5, display: v => (36 * v - 18 >= 0 ? '+' : '') + fmt(36 * v - 18, 2) + ' dB' },
    { name: 'Bypass', value: 0, min: 0, max: 1, items: ['Off', 'On'], display: v => (Math.round(v) ? 'On' : 'Off') }
  ];
}

// threshold in dB -> normalized for the fixture above
const thrNorm = (db) => (db + 60) / 60;

function buildProject(opt = {}) {
  const live = new FakeLive(opt.idOffset || 0);
  const proC2 = (thrDb) => ({ type: 'PluginDevice', name: 'Pro-C 2', className: 'PluginDevice', classDisplayName: 'Pro-C 2', params: proC2Params(thrNorm(thrDb)) });
  const eq = { type: 'Device', name: 'EQ Eight', className: 'Eq8', params: [{ name: 'Device On', value: 1, items: ['Off', 'On'], max: 1, display: v => (v ? 'On' : 'Off') }] };
  const tKick = live.addTrack('Kick'); const tSnare = live.addTrack('Snare'); const tBass = live.addTrack('Bass');
  const tVox = live.addTrack('Vocal'); const tBus = live.addTrack('Drum Bus'); const tSyn = live.addTrack('Synth', { midi: true });
  const tGC = live.addTrack('GLOBAL CONTROL', { midi: true });
  const rBus = live.addTrack('Reverb', { ret: true });
  live.thisDevice = live.addDevice(tGC, { type: 'MaxDevice', name: 'GLOBAL_CONTROL_BRIDGE', className: 'MxDeviceMidiEffect', devType: 4, params: [] });
  const inst = [];
  inst.push(live.addDevice(tKick, proC2(-12)));
  live.addDevice(tKick, eq);
  inst.push(live.addDevice(tKick, proC2(-6)));
  inst.push(live.addDevice(tSnare, proC2(-8)));
  inst.push(live.addDevice(tBass, proC2(-15)));
  inst.push(live.addDevice(tVox, proC2(-10)));
  inst.push(live.addDevice(tVox, proC2(-20)));
  const rack = live.addDevice(tBus, { type: 'RackDevice', name: 'Audio Effect Rack', className: 'AudioEffectGroupDevice', chains: true, params: [] });
  const ch0 = live.addChain(rack, 'Parallel'); const ch1 = live.addChain(rack, 'Dry');
  inst.push(live.addDevice(ch0, proC2(-30)));
  inst.push(live.addDevice(ch1, proC2(-3)));
  inst.push(live.addDevice(tSyn, proC2(-18)));
  inst.push(live.addDevice(rBus, proC2(-24)));
  return { live, instances: inst, tracks: { tKick, tSnare, tBass, tVox, tBus, tSyn, tGC, rBus } };
}

module.exports = { FakeLive, loadBridge, buildProject, thrNorm, BRIDGE_JS };
