// Bandmates Chords - the chord track: type or pick a progression, write it as a MIDI clip, or detect chords (Chord ID).
// The players read the chords from this track's clips (same scene / same arrangement position).
var DEV = { id: 'chords', personas: [], needs: {} };
var progText = 'C G/B | Am7 - | Fmaj7 | G7sus4 G7', pendingWrite = false;

function keyRoot(info) {
  var k = Math.round(uiValue('key'));
  return k >= 12 ? info.root : k;
}
function flats() { return !!uiValue('flats'); }
function showText(t) { progText = t; outlet(1, 'textedit', 'set', t); }

// textedit content arrives here (also after "write" asks the textedit to output)
function text() {
  var a = arrayfromargs(arguments);
  if (a.length && a[0] === 'text') a.shift();
  progText = a.join(' ');
  if (pendingWrite) { pendingWrite = false; doWrite(); }
}

function use_preset() {
  var info = songInfo(), id = BM.chords.PRESETS[Math.round(uiValue('progPreset'))].id;
  var prog = BM.chords.presetProgression(id, keyRoot(info), info.beatsPerBar);
  showText(BM.chords.toText(prog, info.beatsPerBar, flats()));
  status('Progressione: ' + BM.chords.PRESETS[Math.round(uiValue('progPreset'))].name + '. Premi "Scrivi nel clip".');
}

function write() {
  ensureReady();
  status('Scrivo...');
  pendingWrite = true;
  outlet(1, 'textedit', 'bang');
  // if the text box does not answer, write the last text it sent
  var t = new Task(function () { if (pendingWrite) { pendingWrite = false; doWrite(); } }, JSTHIS);
  t.schedule(150);
}

function doWrite() {
  try {
    var info = songInfo(), parsed = BM.chords.parseText(progText, info.beatsPerBar, keyRoot(info));
    if (!parsed.prog.length) { status('Scrivi una progressione, es: C G/B | Am7 - | F | G'); return; }
    var length = Math.ceil(parsed.length / info.beatsPerBar - 1e-6) * info.beatsPerBar;
    var target = selectedTarget(), clip;
    if (target.kind === 'session') {
      var cs = L(target.path);
      if (!getv(cs, 'has_clip')) cs.call('create_clip', length);
      clip = L(target.path + ' clip');
      clip.set('loop_start', 0);
      clip.set('loop_end', length);
      clip.set('end_marker', length);
    } else clip = L(target.path);
    writeNotes(clip, BM.chords.toNotes(parsed.prog, { ext: uiValue('ext') / 100 }), target.kind === 'session' ? 0 : getv(clip, 'start_marker'));
    var label = BM.chords.toText(parsed.prog, info.beatsPerBar, flats());
    clip.set('name', label.length > 60 ? label.substr(0, 57) + '...' : label);
    status(parsed.errors.length ? 'Non riconosciuti: ' + parsed.errors.join(' ') : 'Scritti ' + parsed.prog.length + ' accordi in ' + (length / info.beatsPerBar) + ' battute');
  } catch (e) { fail(e); }
}

// Chord ID: analyse the selected clip on any track
function read() {
  try {
    var dc = L('live_set view detail_clip');
    if (!valid(dc) || !getv(dc, 'is_midi_clip')) { status('Seleziona un clip MIDI da analizzare'); return; }
    showText(textFromClip(dc));
    status('Chord ID: ' + BM.spec.stripTag(String(getv(dc, 'name'))));
  } catch (e) { fail(e); }
}

function textFromClip(clip) {
  var info = songInfo(), lp = loopOf(clip), res = BM.spec.toEngine(paramSpec('idRes'), uiValue('idRes'));
  var prog = BM.chords.fromNotes(readNotes(clip, lp.start, lp.length), lp.length, { resolution: res });
  return BM.chords.toText(prog, info.beatsPerBar, flats());
}

DEV.onSelect = function (clip) {
  if (getv(clip, 'is_midi_clip')) showText(textFromClip(clip));
};

function transpose(n) {
  var info = songInfo(), parsed = BM.chords.parseText(progText, info.beatsPerBar, keyRoot(info));
  for (var i = 0; i < parsed.prog.length; i++) {
    var c = parsed.prog[i].chord;
    if (c.nc) continue;
    parsed.prog[i].chord = BM.theory.makeChord(c.root + n, c.q, c.bass === null ? null : c.bass + n);
  }
  showText(BM.chords.toText(parsed.prog, info.beatsPerBar, flats()));
}
