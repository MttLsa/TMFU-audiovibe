// Bandmates - Max for Live runtime shared by all devices (Live API: clips, chords, sections, controls). ES5.
// The device script defines DEV = { id, personas, build: function (params, ctx) -> notes, needs: {...} }.
inlets = 1;
outlets = 2; // 0: status text, 1: UI commands (routed in the patcher: xy, followmenu, textedit)
autowatch = 0;

// the js object itself (top-level `this`): plain function calls must not rely on `this`
var JSTHIS = this;
var ready = false, seed = 1 + Math.floor(Math.random() * 99999), autoTask = null, clipWatcher = null;
var followTrackName = '';

function log(s) { post('[Bandmates] ' + s + '\n'); }
function fail(e) { status('Errore: ' + e); log('ERROR ' + e + (e && e.stack ? '\n' + e.stack : '')); }
function status(s) { outlet(0, 'set', s); }

// ---------------------------------------------------------------- Live helpers
function L(path) { return new LiveAPI(path); }
function first(v) { return (v && typeof v === 'object' && v.length !== undefined) ? v[0] : v; }
function getv(api, prop) { return first(api.get(prop)); }
function valid(api) { return api && parseInt(api.id, 10) !== 0; }
function upath(api) { return String(api.unquotedpath || api.path || '').replace(/"/g, ''); }

function songInfo() {
  var song = L('live_set'), num = getv(song, 'signature_numerator') || 4, den = getv(song, 'signature_denominator') || 4;
  var info = { tempo: getv(song, 'tempo'), num: num, den: den, beatsPerBar: num * 4 / den, compound: den === 8 && num % 3 === 0,
               root: 0, scale: 'Major' };
  try { info.root = getv(song, 'root_note') || 0; info.scale = String(getv(song, 'scale_name') || 'Major'); } catch (e) { }
  return info;
}

function myTrack() {
  var m = /tracks (\d+)/.exec(upath(L('this_device')));
  return m ? parseInt(m[1], 10) : -1;
}
function trackCount() { return L('live_set').getcount('tracks'); }
function trackName(i) { return String(getv(L('live_set tracks ' + i), 'name')); }

// first track (not this one) that holds a device whose name starts with one of prefixes, else whose name matches re
function findTrack(prefixes, re) {
  var n = trackCount(), me = myTrack(), i, j;
  for (i = 0; i < n; i++) {
    if (i === me) continue;
    var t = L('live_set tracks ' + i), nd = t.getcount('devices');
    for (j = 0; j < nd; j++) {
      var dn = String(getv(L('live_set tracks ' + i + ' devices ' + j), 'name')).toLowerCase();
      for (var k = 0; k < prefixes.length; k++) if (dn.indexOf(prefixes[k].toLowerCase()) === 0) return i;
    }
  }
  if (re) for (i = 0; i < n; i++) if (i !== me && re.test(trackName(i))) return i;
  return -1;
}
function findTrackByName(name) {
  var n = trackCount();
  for (var i = 0; i < n; i++) if (trackName(i) === name) return i;
  return -1;
}

// ---------------------------------------------------------------- clips
function readNotes(clip, from, len) {
  var out = [], d = null, i;
  try {
    var r = clip.call('get_notes_extended', 0, 128, from, len);
    if (typeof r !== 'string') r = r.join(' ');
    d = JSON.parse(r);
  } catch (e) { d = null; }
  if (d && d.notes) {
    for (i = 0; i < d.notes.length; i++) {
      var n = d.notes[i];
      out.push({ pitch: n.pitch, start: n.start_time - from, dur: n.duration, vel: n.velocity, mute: n.mute });
    }
    return out;
  }
  // Live 10 style: "notes N note p t d v m note ... done"
  var flat = clip.call('get_notes', from, 0, len, 128);
  for (i = 0; i < flat.length; i++) {
    if (flat[i] === 'note') {
      out.push({ pitch: +flat[i + 1], start: +flat[i + 2] - from, dur: +flat[i + 3], vel: +flat[i + 4], mute: +flat[i + 5] });
      i += 5;
    }
  }
  return out;
}

function loopOf(clip) {
  var ls = getv(clip, 'loop_start'), le = getv(clip, 'loop_end');
  return { start: ls, length: Math.max(0.25, le - ls) };
}

// notes of a clip slot, looped/trimmed to `length`
function slotNotes(track, slot, length) {
  if (track < 0) return null;
  var cs = L('live_set tracks ' + track + ' clip_slots ' + slot);
  if (!valid(cs) || !getv(cs, 'has_clip')) return null;
  var clip = L('live_set tracks ' + track + ' clip_slots ' + slot + ' clip');
  if (!getv(clip, 'is_midi_clip')) return null;
  var lp = loopOf(clip), base = readNotes(clip, lp.start, lp.length);
  return { notes: tile(base, lp.length, length), length: lp.length };
}

function tile(notes, period, length) {
  if (!length) return notes;
  var out = [];
  for (var off = 0; off < length - 1e-6; off += period) {
    for (var i = 0; i < notes.length; i++) {
      var s = notes[i].start + off;
      if (s >= 0 && s < length - 1e-6) out.push({ pitch: notes[i].pitch, start: s, dur: Math.min(notes[i].dur, length - s), vel: notes[i].vel, mute: notes[i].mute });
    }
  }
  return out;
}

// notes of all arrangement clips of a track in [start, start+length), relative to start
function arrangementNotes(track, start, length) {
  if (track < 0) return null;
  var t = L('live_set tracks ' + track), n = t.getcount('arrangement_clips'), out = [], found = false;
  for (var i = 0; i < n; i++) {
    var c = L('live_set tracks ' + track + ' arrangement_clips ' + i);
    if (!getv(c, 'is_midi_clip')) continue;
    var st = getv(c, 'start_time'), en = getv(c, 'end_time');
    if (en <= start || st >= start + length) continue;
    found = true;
    var looping = getv(c, 'looping'), sm = getv(c, 'start_marker'), lp = loopOf(c);
    var src = looping ? readNotes(c, lp.start, lp.length) : readNotes(c, sm, en - st);
    var period = looping ? lp.length : (en - st), first0 = looping ? (lp.start - sm) : 0;
    for (var off = first0; st + off < en; off += period) {
      for (var k = 0; k < src.length; k++) {
        var abs = st + off + src[k].start;
        if (abs < st || abs >= en || abs < start || abs >= start + length) continue;
        out.push({ pitch: src[k].pitch, start: abs - start, dur: Math.min(src[k].dur, en - abs), vel: src[k].vel, mute: src[k].mute });
      }
      if (!looping) break;
    }
  }
  return found ? { notes: out, length: length } : null;
}

function writeNotes(clip, notes, offset) {
  var lp = loopOf(clip), end = Math.max(getv(clip, 'end_marker') || 0, lp.start + lp.length) + 64, i;
  try { clip.call('remove_notes_extended', 0, 128, -16, end + 16); }
  catch (e) { clip.call('remove_notes', -16, 0, end + 16, 128); }
  if (!notes.length) return;
  var arr = [];
  for (i = 0; i < notes.length; i++) {
    arr.push({ pitch: notes[i].pitch, start_time: notes[i].start + (offset || 0), duration: notes[i].dur,
               velocity: notes[i].vel, mute: notes[i].mute ? 1 : 0 });
  }
  var ok = false;
  try { clip.call('add_new_notes', { notes: arr }); ok = true; } catch (e) { log('add_new_notes failed, using set_notes: ' + e); }
  if (ok) return;
  // classic API, available in every Live version with Max for Live
  clip.call('set_notes');
  clip.call('notes', arr.length);
  for (i = 0; i < arr.length; i++) {
    clip.call('note', arr[i].pitch, arr[i].start_time.toFixed(6), Math.max(0.01, arr[i].duration).toFixed(6), arr[i].velocity, arr[i].mute);
  }
  clip.call('done');
}

// ---------------------------------------------------------------- targets (what "Genera" writes into)
function sessionTarget(slot) {
  return { kind: 'session', track: myTrack(), slot: slot, path: 'live_set tracks ' + myTrack() + ' clip_slots ' + slot };
}

// Which clip "Genera" writes into. In Arrangement view the Session selection is ignored (and vice versa).
function selectedTarget() {
  var me = myTrack(), m, arr = arrangementFocused();
  var dc = L('live_set view detail_clip');
  if (valid(dc)) {
    var p = upath(dc);
    m = /tracks (\d+) arrangement_clips (\d+)/.exec(p);
    if (m && +m[1] === me) return { kind: 'arr', track: me, path: p, start: getv(dc, 'start_time'), length: getv(dc, 'end_time') - getv(dc, 'start_time') };
    m = /tracks (\d+) clip_slots (\d+) clip/.exec(p);
    if (m && +m[1] === me && !arr) return sessionTarget(+m[2]);
  }
  if (arr) return arrangementTarget(me);
  var hs = L('live_set view highlighted_clip_slot');
  if (valid(hs)) {
    m = /tracks (\d+) clip_slots (\d+)/.exec(upath(hs));
    if (m && +m[1] === me) return sessionTarget(+m[2]);
  }
  m = /scenes (\d+)/.exec(upath(L('live_set view selected_scene')));
  return sessionTarget(m ? +m[1] : 0);
}

// ---------------------------------------------------------------- never overwrite the user's own clips
function owns(clip) {
  var name = String(getv(clip, 'name') || '');
  return DEV.owns ? DEV.owns(name) : !!BM.spec.decodeTag(DEV.id, name);
}
function hasNotes(clip) { return readNotes(clip, -16, 100000).length > 0; }
function writable(clip) { return owns(clip) || !hasNotes(clip); }
function protectedError(clip) {
  return new Error('il clip "' + BM.spec.stripTag(String(getv(clip, 'name') || '')) + '" non e di Bandmates, non lo sovrascrivo: usa uno slot vuoto o un clip vuoto');
}

function arrangementFocused() {
  try { return String(getv(L('live_app view'), 'focused_document_view')) === 'Arranger'; } catch (e) { return false; }
}

// Arrangement view with no clip selected: write at the playhead's bar (or into the chord region found there)
function arrangementTarget(me) {
  var info = songInfo(), song = L('live_set'), bpb = info.beatsPerBar;
  var pos = Math.floor((getv(song, 'current_song_time') || 0) / bpb + 1e-6) * bpb;
  var start = pos, length = 8 * bpb, ct = chordTrack(), i;
  if (ct >= 0) {
    var cn = L('live_set tracks ' + ct).getcount('arrangement_clips');
    for (i = 0; i < cn; i++) {
      var c = L('live_set tracks ' + ct + ' arrangement_clips ' + i), st = getv(c, 'start_time'), en = getv(c, 'end_time');
      if (pos >= st - 1e-6 && pos < en - 1e-6) { start = st; length = en - st; break; }
    }
  }
  var idx = arrangementClipOverlapping(me, start, length);
  if (idx >= 0) {
    var ex = L('live_set tracks ' + me + ' arrangement_clips ' + idx);
    if (!writable(ex)) throw protectedError(ex);
  } else {
    L('live_set tracks ' + me).call('create_midi_clip', start, length);
    idx = arrangementClipAt(me, start);
    if (idx < 0) throw new Error('serve Live 12 per creare clip in Arrangement: crea un clip vuoto e selezionalo');
  }
  var p = 'live_set tracks ' + me + ' arrangement_clips ' + idx, cl = L(p);
  return { kind: 'arr', track: me, path: p, start: getv(cl, 'start_time'), length: getv(cl, 'end_time') - getv(cl, 'start_time') };
}

function sceneName(slot) { return String(getv(L('live_set scenes ' + slot), 'name') || ''); }

function locatorAt(time) {
  var song = L('live_set'), n = song.getcount('cue_points'), best = '', bt = -1;
  for (var i = 0; i < n; i++) {
    var c = L('live_set cue_points ' + i), t = getv(c, 'time');
    if (t <= time + 1e-6 && t > bt) { bt = t; best = String(getv(c, 'name')); }
  }
  return best;
}

function chordTrack() { return findTrack(['Bandmates Chords'], /chord|accord/i); }
function drumTrack() { return findTrack(['Bandmates Drums', 'Bandmates Beats'], /drum|batter|beat/i); }

// ctx: length, beatsPerBar, sectionName, prog, kick (onsets), follow (onsets), info
function targetContext(target, info) {
  var ctx = { info: info, beatsPerBar: info.beatsPerBar, compound: info.compound, prog: null, kick: null, follow: null };
  var ct = chordTrack(), chordData = null;
  if (target.kind === 'session') {
    var cs = L(target.path);
    if (getv(cs, 'has_clip')) ctx.length = loopOf(L(target.path + ' clip')).length;
    if (ct >= 0) chordData = slotNotes(ct, target.slot, null);
    if (!ctx.length) ctx.length = chordData ? chordData.length : 8 * info.beatsPerBar;
    ctx.sectionName = sceneName(target.slot);
    if (ct >= 0) chordData = slotNotes(ct, target.slot, ctx.length);
  } else {
    ctx.length = target.length;
    ctx.sectionName = locatorAt(target.start);
    if (ct >= 0) chordData = arrangementNotes(ct, target.start, target.length);
  }
  if (chordData && chordData.notes.length) ctx.prog = BM.chords.fromNotes(chordData.notes, ctx.length, { resolution: 1 });
  ctx.chordSource = ctx.prog ? 'traccia accordi' : 'tonalita del brano';
  if (!ctx.prog) {
    var minor = /minor|dorian|phrygian|aeolian|locrian/i.test(info.scale) && !/major/i.test(info.scale);
    ctx.prog = [{ start: 0, dur: ctx.length, chord: BM.theory.makeChord(info.root, minor ? 'min' : 'maj', null) }];
  }
  if (DEV.needs && DEV.needs.kick) {
    var dt = drumTrack(), kd = null;
    if (dt >= 0) kd = target.kind === 'session' ? slotNotes(dt, target.slot, ctx.length) : arrangementNotes(dt, target.start, ctx.length);
    if (kd) ctx.kick = onsets(kd.notes, 36);
  }
  if (DEV.needs && DEV.needs.follow && uiValue('followOn')) {
    var ft = findTrackByName(followTrackName), fd = null;
    if (ft >= 0) fd = target.kind === 'session' ? slotNotes(ft, target.slot, ctx.length) : arrangementNotes(ft, target.start, ctx.length);
    if (fd) ctx.follow = onsets(fd.notes, null);
  }
  return ctx;
}

function onsets(notes, pitch) {
  var seen = {}, out = [];
  for (var i = 0; i < notes.length; i++) {
    if (pitch !== null && notes[i].pitch !== pitch) continue;
    if (notes[i].mute || notes[i].vel < 30) continue;
    var q = Math.round(notes[i].start * 4) / 4;
    if (!seen[q]) { seen[q] = 1; out.push(q); }
  }
  out.sort(function (a, b) { return a - b; });
  return out;
}

// ---------------------------------------------------------------- controls
function spec() { return BM.spec.DEVICES[DEV.id]; }
function paramSpec(id) {
  var ps = spec().params;
  for (var i = 0; i < ps.length; i++) if (ps[i].id === id) return ps[i];
  return null;
}
function ctl(id) { return JSTHIS.patcher.getnamed(id); }
function uiValue(id) {
  var o = ctl(id), p = paramSpec(id);
  if (!o) return p ? p.def : 0;
  var v = first(o.getvalueof());
  return (v === undefined || v === null) ? (p ? p.def : 0) : v;
}
function setUi(id, v) {
  var o = ctl(id);
  if (o && v !== null && v !== undefined) o.message('set', v);
}
function syncPad() { outlet(1, 'xy', uiValue('complexity'), uiValue('loudness')); }

function readParams() {
  var ps = spec().params, o = {};
  for (var i = 0; i < ps.length; i++) {
    if (ps[i].kind === 'button') continue;
    o[ps[i].id] = BM.spec.toEngine(ps[i], uiValue(ps[i].id));
  }
  o.intensity = o.loudness;
  o.seed = seed;
  return o;
}

function personaIndex() { return Math.round(uiValue('persona')) || 0; }

function applyPreset(personaIdx, presetIdx) {
  var persona = DEV.personas[Math.max(0, Math.min(DEV.personas.length - 1, personaIdx))];
  var pr = BM.styles.presetsFor(persona)[presetIdx] || BM.styles.presetsFor(persona)[0];
  for (var k in pr.params) {
    if (!pr.params.hasOwnProperty(k)) continue;
    var id = k === 'intensity' ? 'loudness' : k, p = paramSpec(id);
    if (!p || p.kind === 'button') continue;
    setUi(id, BM.spec.fromEngine(p, pr.params[k]));
  }
  syncPad();
}

// ---------------------------------------------------------------- generation
function clipFor(target, length) {
  var clip;
  if (target.kind === 'arr') clip = L(target.path);
  else {
    var cs = L(target.path);
    if (!getv(cs, 'has_clip')) cs.call('create_clip', length);
    clip = L(target.path + ' clip');
  }
  if (!writable(clip)) throw protectedError(clip);
  return clip;
}

function clipLabel(target, ctx) {
  var persona = DEV.personas[personaIndex()];
  var sec = ctx.sectionName ? ' · ' + ctx.sectionName : '';
  var tag = BM.spec.encodeTag(DEV.id, currentUiValues(), personaIndex(), seed);
  return persona.name + sec + ' ' + tag;
}

function currentUiValues() {
  var ps = BM.spec.storable(DEV.id), o = {};
  for (var i = 0; i < ps.length; i++) o[ps[i].id] = Math.round(uiValue(ps[i].id));
  return o;
}

function generateInto(target) {
  var info = songInfo(), ctx = targetContext(target, info), params = readParams();
  params.length = ctx.length;
  params.beatsPerBar = ctx.beatsPerBar;
  params.compound = ctx.compound;
  params.sectionName = ctx.sectionName;
  params.prog = ctx.prog;
  params.kick = ctx.kick;
  params.follow = ctx.follow;
  params.persona = DEV.personas[personaIndex()].id;
  params.fallbackRoot = info.root;
  var notes = DEV.build(params, ctx);
  var clip = clipFor(target, ctx.length);
  writeNotes(clip, notes, target.kind === 'session' ? loopOf(clip).start : getv(clip, 'start_marker'));
  clip.set('name', clipLabel(target, ctx));
  return { notes: notes.length, bars: ctx.length / ctx.beatsPerBar, section: ctx.sectionName, source: ctx.chordSource };
}

function report(r) {
  status(DEV.personas[personaIndex()].name + ': ' + r.notes + ' note, ' + (Math.round(r.bars * 10) / 10) + ' batt.' +
         (r.section ? ' · ' + r.section : '') + (DEV.needs && DEV.needs.chords ? ' (' + r.source + ')' : ''));
}

// buttons must work even if the load-time init was missed
function ensureReady() {
  if (!clipWatcher) { try { init(); } catch (e) { log(e); } }
  ready = true;
}

function generate() {
  ensureReady();
  status('Genero...');
  try { report(generateInto(selectedTarget())); } catch (e) { fail(e); }
}

function newvariation() { seed = 1 + Math.floor(Math.random() * 1679615); generate(); }

function generate_scenes() {
  ensureReady();
  try {
    var n = L('live_set').getcount('scenes'), ct = chordTrack(), me = myTrack(), done = 0, skipped = 0;
    for (var s = 0; s < n; s++) {
      var want = false;
      if (ct >= 0) want = !!getv(L('live_set tracks ' + ct + ' clip_slots ' + s), 'has_clip');
      else want = !!getv(L('live_set tracks ' + me + ' clip_slots ' + s), 'has_clip') || sceneName(s).length > 0;
      if (!want) continue;
      var own = L('live_set tracks ' + me + ' clip_slots ' + s);
      if (getv(own, 'has_clip') && !writable(L('live_set tracks ' + me + ' clip_slots ' + s + ' clip'))) { skipped++; continue; }
      generateInto(sessionTarget(s));
      done++;
    }
    status((done ? 'Generati ' + done + ' clip (una per scena)' : 'Nessuna scena con accordi o nome') + (skipped ? ', ' + skipped + ' clip tuoi lasciati intatti' : ''));
  } catch (e) { fail(e); }
}

function generate_arrangement() {
  ensureReady();
  try {
    var me = myTrack(), ct = chordTrack(), t = L('live_set tracks ' + me), regions = [], i;
    if (ct >= 0) {
      var cn = L('live_set tracks ' + ct).getcount('arrangement_clips');
      for (i = 0; i < cn; i++) {
        var c = L('live_set tracks ' + ct + ' arrangement_clips ' + i);
        regions.push({ start: getv(c, 'start_time'), end: getv(c, 'end_time') });
      }
    }
    if (!regions.length) { status('Arrangement: nessun clip sulla traccia accordi'); return; }
    var done = 0, skipped = 0;
    for (i = 0; i < regions.length; i++) {
      var ov = arrangementClipOverlapping(me, regions[i].start, regions[i].end - regions[i].start);
      if (ov >= 0 && !writable(L('live_set tracks ' + me + ' arrangement_clips ' + ov))) { skipped++; continue; }
      var idx = ov >= 0 ? ov : arrangementClipAt(me, regions[i].start);
      if (idx < 0) {
        t.call('create_midi_clip', regions[i].start, regions[i].end - regions[i].start);
        idx = arrangementClipAt(me, regions[i].start);
      }
      if (idx < 0) { status('Serve Live 12 per creare clip in Arrangement'); return; }
      var p = 'live_set tracks ' + me + ' arrangement_clips ' + idx, c2 = L(p);
      generateInto({ kind: 'arr', track: me, path: p, start: getv(c2, 'start_time'), length: getv(c2, 'end_time') - getv(c2, 'start_time') });
      done++;
    }
    status('Arrangement: generati ' + done + ' clip' + (skipped ? ', ' + skipped + ' clip tuoi lasciati intatti' : ''));
  } catch (e) { fail(e); }
}

function arrangementClipOverlapping(track, start, length) {
  var n = L('live_set tracks ' + track).getcount('arrangement_clips');
  for (var i = 0; i < n; i++) {
    var c = L('live_set tracks ' + track + ' arrangement_clips ' + i);
    if (getv(c, 'start_time') < start + length - 1e-6 && getv(c, 'end_time') > start + 1e-6) return i;
  }
  return -1;
}

function arrangementClipAt(track, start) {
  var n = L('live_set tracks ' + track).getcount('arrangement_clips');
  for (var i = 0; i < n; i++) if (Math.abs(getv(L('live_set tracks ' + track + ' arrangement_clips ' + i), 'start_time') - start) < 1e-3) return i;
  return -1;
}

// ---------------------------------------------------------------- messages from the patcher
function init() {
  ready = false;
  clipWatcher = new LiveAPI(onSelection, 'live_set view');
  clipWatcher.property = 'detail_clip';
  refreshTracks();
  syncPad();
  status('Avvio...');
  var t = new Task(function () { ready = true; status('Pronto. Seleziona uno slot o un clip su questa traccia e premi Genera.'); }, JSTHIS);
  t.schedule(600);
}

// a control changed: param <id> <value>
function param(id, v) {
  if (!ready) return;
  if (id === 'persona') { setUi('preset', 0); applyPreset(Math.round(v), 0); }
  else if (id === 'preset') applyPreset(personaIndex(), Math.round(v));
  if (id === 'auto' || id === 'followSections') return;
  if (uiValue('auto')) scheduleRegenerate();
}

function scheduleRegenerate() {
  if (autoTask) autoTask.cancel();
  autoTask = new Task(function () {
    var target = selectedTarget();
    // like editing a region: only regenerate a clip this device made
    if (target.kind === 'session' && !getv(L(target.path), 'has_clip')) return;
    var clip = target.kind === 'session' ? L(target.path + ' clip') : L(target.path);
    if (!BM.spec.decodeTag(DEV.id, String(getv(clip, 'name')))) return;
    try { report(generateInto(target)); } catch (e) { fail(e); }
  }, JSTHIS);
  autoTask.schedule(250);
}

// selecting a clip made by this device restores its settings (each clip keeps its own performance)
function onSelection(args) {
  if (!ready) return;
  var dc = L('live_set view detail_clip');
  if (!valid(dc)) return;
  var m = /tracks (\d+) /.exec(upath(dc));
  if (!m || +m[1] !== myTrack()) return;
  if (DEV.onSelect) { DEV.onSelect(dc); return; }
  var tag = BM.spec.decodeTag(DEV.id, String(getv(dc, 'name')));
  if (!tag) return;
  ready = false;
  setUi('persona', tag.persona);
  for (var k in tag.values) if (tag.values.hasOwnProperty(k)) setUi(k, tag.values[k]);
  seed = tag.seed || seed;
  syncPad();
  ready = true;
  status('Impostazioni del clip: ' + BM.spec.stripTag(String(getv(dc, 'name'))));
}

function refreshTracks() {
  if (!(DEV.needs && DEV.needs.follow)) return;
  outlet(1, 'followmenu', 'clear');
  var n = trackCount(), me = myTrack();
  for (var i = 0; i < n; i++) if (i !== me) outlet(1, 'followmenu', 'append', trackName(i));
}
function followtrack() {
  var a = arrayfromargs(arguments);
  followTrackName = a.join(' ');
  if (ready && uiValue('auto')) scheduleRegenerate();
}

function anything() { log('unknown message ' + messagename); }
