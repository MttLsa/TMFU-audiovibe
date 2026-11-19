// A small in-memory Live set + Max js globals, to run the device scripts outside Max.
'use strict';
const fs = require('fs');
const path = require('path');
const vm = require('vm');

function makeSet() {
  const clip = (length, notes, name) => ({ length, notes: notes || [], name: name || '', loop_start: 0, loop_end: length, end_marker: length, is_midi_clip: 1 });
  const track = (name, dev, nScenes) => ({ name, devices: [{ name: dev }], slots: Array.from({ length: nScenes }, () => ({ clip: null })), arrangement: [] });
  const set = {
    tempo: 124, num: 4, den: 4, root_note: 9, scale_name: 'Minor',
    scenes: [{ name: 'Strofa' }, { name: 'Ritornello' }, { name: '' }],
    cue_points: [{ time: 0, name: 'Intro' }, { time: 32, name: 'Chorus' }],
    tracks: [], view: { detail: null, highlighted: null, scene: 0, focused: 'Session' }, song_time: 0, clip
  };
  for (const [n, d] of [['Chords', 'Bandmates Chords'], ['Drums', 'Bandmates Drums'], ['Bass', 'Bandmates Bass'], ['Keys', 'Bandmates Keys'], ['Beats', 'Bandmates Beats']]) set.tracks.push(track(n, d, 3));
  return set;
}

function resolve(set, p) {
  p = p.replace(/^id \d+$/, '');
  const t = p.split(' ');
  let node = null, kind = null, i = 0;
  if (p === 'live_app view') return { node: set.view, kind: 'appview', path: p };
  if (t[0] === 'live_set') { node = set; kind = 'song'; i = 1; } else return { node: null };
  while (i < t.length) {
    const k = t[i], n = parseInt(t[i + 1], 10);
    if (kind === 'song' && k === 'tracks') { node = node.tracks[n]; kind = 'track'; i += 2; }
    else if (kind === 'song' && k === 'scenes') { node = node.scenes[n]; kind = 'scene'; i += 2; }
    else if (kind === 'song' && k === 'cue_points') { node = node.cue_points[n]; kind = 'cue'; i += 2; }
    else if (kind === 'song' && k === 'view') {
      if (t[i + 1] === 'detail_clip') return set.view.detail ? resolve(set, set.view.detail) : { node: null };
      if (t[i + 1] === 'highlighted_clip_slot') return set.view.highlighted ? resolve(set, set.view.highlighted) : { node: null };
      if (t[i + 1] === 'selected_scene') return resolve(set, 'live_set scenes ' + set.view.scene);
      return { node: set.view, kind: 'view', path: 'live_set view' };
    }
    else if (kind === 'track' && k === 'devices') { node = node.devices[n]; kind = 'device'; i += 2; }
    else if (kind === 'track' && k === 'clip_slots') { node = node.slots[n]; kind = 'slot'; i += 2; }
    else if (kind === 'track' && k === 'arrangement_clips') { node = node.arrangement[n]; kind = 'clip'; i += 2; }
    else if (kind === 'slot' && k === 'clip') { node = node.clip; kind = 'clip'; i += 1; }
    else throw new Error('mock: cannot resolve ' + p);
    if (!node) return { node: null };
  }
  return { node, kind, path: p };
}

function makeLiveAPI(set, thisDevice) {
  return function LiveAPI(a, b) {
    let p = typeof a === 'function' ? b : a;
    if (p.indexOf('this_device') === 0) p = thisDevice + p.substring('this_device'.length);
    const r = resolve(set, p);
    this.id = r.node ? '7' : '0';
    this.unquotedpath = r.path || '';
    this.path = '"' + this.unquotedpath + '"';
    const node = r.node, kind = r.kind;
    this.get = (prop) => {
      if (kind === 'appview') return [prop === 'focused_document_view' ? set.view.focused : null];
      if (kind === 'song') {
        const m = { current_song_time: set.song_time, tempo: set.tempo, signature_numerator: set.num, signature_denominator: set.den, root_note: set.root_note, scale_name: set.scale_name };
        return [m[prop]];
      }
      if (kind === 'slot' && prop === 'has_clip') return [node.clip ? 1 : 0];
      if (kind === 'clip') {
        if (prop === 'start_time') return [node.start_time || 0];
        if (prop === 'end_time') return [(node.start_time || 0) + node.length];
        if (prop === 'start_marker') return [0];
        if (prop === 'looping') return [1];
      }
      return [node[prop]];
    };
    this.set = (prop, v) => { node[prop] = v; };
    this.getcount = (prop) => (kind === 'song' ? set[prop] : (prop === 'arrangement_clips' ? node.arrangement : node[prop])).length;
    this.call = (fn, ...args) => {
      if (fn === 'create_clip') { node.clip = set.clip(args[0]); return; }
      if (fn === 'create_midi_clip') { const c = set.clip(args[1]); c.start_time = args[0]; node.arrangement.push(c); return; }
      if (fn === 'get_notes_extended') {
        const [, , from, len] = args;
        return JSON.stringify({ notes: node.notes.filter((n) => n.start_time >= from && n.start_time < from + len) });
      }
      if (fn === 'remove_notes_extended') { node.notes = []; return; }
      if (fn === 'add_new_notes') { node.notes = node.notes.concat(args[0].notes); return; }
      throw new Error('mock: call ' + fn);
    };
  };
}

// load a device script into its own Max-like context
function loadDevice(set, file, trackIndex) {
  const out = [];
  const controls = {};
  const ctx = {
    post: () => {}, outlet: (...a) => out.push(a), arrayfromargs: (a) => Array.prototype.slice.call(a), messagename: '',
    Task: function (fn) { this.schedule = () => fn(); this.cancel = () => {}; },
    LiveAPI: makeLiveAPI(set, 'live_set tracks ' + trackIndex + ' devices 0'),
    patcher: { getnamed: (id) => ({ getvalueof: () => controls[id], message: (m, v) => { if (m === 'set') controls[id] = v; } }) }
  };
  vm.createContext(ctx);
  vm.runInContext(fs.readFileSync(path.join(__dirname, '..', 'devices', file), 'utf8'), ctx, { filename: file });
  ctx.patcher.controls = controls;
  ctx.__out = out;
  return ctx;
}

module.exports = { makeSet, loadDevice };
