// Bandmates engine - chord track: progressions as timed events, text parsing, presets, Chord ID
(function (BM) {
  'use strict';
  var T = BM.theory;

  // Progression = array of {start, dur, chord} in beats, sorted, non-overlapping.

  // Generic, public-domain harmonic formulas (roman numerals relative to the key).
  // `bars` = bars per chord when a single number, or list per chord.
  var PRESETS = [
    { id: 'pop1', name: 'Pop I-V-vi-IV', roman: 'I V vi IV', bars: 1 },
    { id: 'pop2', name: 'Pop vi-IV-I-V', roman: 'vi IV I V', bars: 1 },
    { id: 'fifties', name: 'Anni 50 I-vi-IV-V', roman: 'I vi IV V', bars: 1 },
    { id: 'axis', name: 'Ballata I-iii-IV-V', roman: 'I iii IV V', bars: 1 },
    { id: 'royal', name: 'J-Pop IVmaj7-V7-iii7-vi', roman: 'IVmaj7 V7 iii7 vi', bars: 1 },
    { id: 'canon', name: 'Canone (barocco)', roman: 'I V vi iii IV I IV V', bars: 1 },
    { id: 'iivi', name: 'Jazz ii-V-I', roman: 'ii7 V7 Imaj7 Imaj7', bars: 1 },
    { id: 'turn', name: 'Turnaround I-vi-ii-V', roman: 'Imaj7 vi7 ii7 V7', bars: 1 },
    { id: 'blues', name: 'Blues 12 battute', roman: 'I7 IV7 I7 I7 IV7 IV7 I7 I7 V7 IV7 I7 V7', bars: 1 },
    { id: 'minor1', name: 'Minore i-bVI-bIII-bVII', roman: 'i bVI bIII bVII', bars: 1 },
    { id: 'andalusian', name: 'Andalusa i-bVII-bVI-V', roman: 'i bVII bVI V', bars: 1 },
    { id: 'minor2', name: 'Minore i-iv-v-i', roman: 'i iv v i', bars: 1 },
    { id: 'soul', name: 'Soul ii9-V9 (vamp)', roman: 'ii9 V9', bars: 2 },
    { id: 'gospel', name: 'Gospel IV-iii-ii-I', roman: 'IVmaj7 iii7 ii7 Imaj7', bars: 1 },
    { id: 'rock', name: 'Rock I-bVII-IV-I', roman: 'I bVII IV I', bars: 1 },
    { id: 'mixo', name: 'Modale I-bVII (vamp)', roman: 'I bVII', bars: 2 },
    { id: 'dorian', name: 'Dorico i7-IV7 (vamp)', roman: 'i7 IV7', bars: 2 },
    { id: 'house', name: 'House vi7-ii7-V7-Imaj7', roman: 'vi7 ii7 V7 Imaj7', bars: 1 },
    { id: 'epic', name: 'Epico i-bVI-bVII-i', roman: 'i bVI bVII i', bars: 2 },
    { id: 'neosoul', name: 'Neo-soul IVmaj7-iii7-vi9', roman: 'IVmaj7 iii7 vi9 vi9', bars: 1 }
  ];

  function presetProgression(id, keyRoot, beatsPerBar) {
    var p = null, i;
    for (i = 0; i < PRESETS.length; i++) if (PRESETS[i].id === id) p = PRESETS[i];
    if (!p) return [];
    var toks = p.roman.split(/\s+/), out = [], t = 0;
    for (i = 0; i < toks.length; i++) {
      var ch = T.parseRoman(toks[i], keyRoot || 0);
      var bars = (typeof p.bars === 'number') ? p.bars : (p.bars[i] || 1);
      if (ch) out.push({ start: t, dur: bars * beatsPerBar, chord: ch });
      t += bars * beatsPerBar;
    }
    return out;
  }

  // "C G/B | Am7 - | F | %" : bars separated by '|', chords inside a bar split it evenly,
  // '-' or '.' continues the previous chord, '%' repeats the previous bar, roman numerals allowed.
  function parseText(text, beatsPerBar, keyRoot) {
    var out = [], errors = [];
    if (!text) return { prog: out, length: 0, errors: errors };
    var bars = String(text).replace(/\n/g, '|').split('|'), t = 0, prevBar = null, b, i;
    var cleaned = [];
    for (b = 0; b < bars.length; b++) {
      var s = bars[b].replace(/^\s+|\s+$/g, '');
      if (s.length) cleaned.push(s);
    }
    for (b = 0; b < cleaned.length; b++) {
      var toks = cleaned[b].split(/[\s,]+/);
      if (toks.length === 1 && toks[0] === '%' && prevBar) toks = prevBar;
      var slot = beatsPerBar / toks.length;
      for (i = 0; i < toks.length; i++) {
        var tok = toks[i];
        if (tok === '-' || tok === '.' || tok === '_') {
          if (out.length) out[out.length - 1].dur += slot;
        } else {
          var ch = T.parseChord(tok);
          if (!ch && keyRoot !== undefined && keyRoot !== null) ch = T.parseRoman(tok, keyRoot);
          if (!ch) { errors.push(tok); ch = { nc: true }; }
          var last = out.length ? out[out.length - 1] : null;
          if (last && sameChord(last.chord, ch) && Math.abs(last.start + last.dur - t) < 1e-6) last.dur += slot;
          else out.push({ start: t, dur: slot, chord: ch });
        }
        t += slot;
      }
      prevBar = toks;
    }
    return { prog: out, length: t, errors: errors };
  }

  function sameChord(a, b) {
    if (!a || !b) return false;
    if (a.nc || b.nc) return !!a.nc && !!b.nc;
    return a.root === b.root && a.q === b.q && a.bass === b.bass;
  }

  function toText(prog, beatsPerBar, flats) {
    if (!prog.length) return '';
    var end = prog[prog.length - 1].start + prog[prog.length - 1].dur;
    var nBars = Math.ceil(end / beatsPerBar - 1e-6), bars = [], b, i;
    for (b = 0; b < nBars; b++) {
      var bs = b * beatsPerBar, be = bs + beatsPerBar, half = beatsPerBar / 2, fine = false;
      for (i = 0; i < prog.length; i++) {
        var st = prog[i].start;
        if (st > bs + 1e-6 && st < be - 1e-6 && Math.abs(st - (bs + half)) > 1e-6) fine = true;
      }
      var slots = [bs, bs + half], toks = [];
      if (fine) { slots = []; for (i = 0; i < beatsPerBar; i++) slots.push(bs + i); }
      for (i = 0; i < slots.length; i++) {
        var ev = eventAt(prog, slots[i] + 1e-6);
        if (!ev) toks.push(i > 0 && toks[i - 1] === 'N.C.' ? '-' : 'N.C.');
        else toks.push(Math.abs(ev.start - slots[i]) < 1e-6 ? T.chordName(ev.chord, flats) : '-');
      }
      var allCont = true;
      for (i = 0; i < toks.length; i++) if (toks[i] !== '-') allCont = false;
      if (allCont) toks = ['-'];
      else if (toks.length === 2 && toks[1] === '-') toks = [toks[0]];
      bars.push(toks.join(' '));
    }
    return bars.join(' | ');
  }

  function eventAt(prog, t) {
    for (var i = 0; i < prog.length; i++) {
      if (t >= prog[i].start - 1e-9 && t < prog[i].start + prog[i].dur - 1e-9) return prog[i];
    }
    return null;
  }
  function chordAt(prog, t) {
    var ev = eventAt(prog, t);
    return (ev && !ev.chord.nc) ? ev.chord : null;
  }
  function nextChange(prog, t) {
    for (var i = 0; i < prog.length; i++) if (prog[i].start > t + 1e-9) return prog[i].start;
    return Infinity;
  }

  // loop / trim the progression over [0, length)
  function fitTo(prog, length) {
    if (!prog.length) return [];
    var plen = prog[prog.length - 1].start + prog[prog.length - 1].dur, out = [], off = 0;
    if (plen <= 0) return [];
    while (off < length - 1e-9) {
      for (var i = 0; i < prog.length; i++) {
        var s = prog[i].start + off;
        if (s >= length - 1e-9) break;
        out.push({ start: s, dur: Math.min(prog[i].dur, length - s), chord: prog[i].chord });
      }
      off += plen;
    }
    return out;
  }

  // chord clip content: bass note (octave 1 in Live naming) + close voicing around C3
  function toNotes(prog, opts) {
    opts = opts || {};
    var vel = opts.velocity || 90, out = [], prev = null;
    for (var i = 0; i < prog.length; i++) {
      var ch = prog[i].chord;
      if (!ch || ch.nc) continue;
      var bass = 36 + T.bassPc(ch);
      out.push({ pitch: bass, start: prog[i].start, dur: prog[i].dur, vel: vel });
      var v = BM.voicing.close(ch, ch.iv.length > 4 ? 4 : ch.iv.length, 64, prev, opts.ext || 0);
      prev = v;
      for (var k = 0; k < v.length; k++) out.push({ pitch: v[k], start: prog[i].start, dur: prog[i].dur, vel: vel - 8 });
    }
    return out;
  }

  // Chord ID from MIDI notes. resolution = smallest chord length in beats (1 = per beat).
  function fromNotes(notes, length, opts) {
    opts = opts || {};
    var res = opts.resolution || 2, out = [], t, i;
    if (!length) { length = 0; for (i = 0; i < notes.length; i++) length = Math.max(length, notes[i].start + notes[i].dur); }
    for (t = 0; t < length - 1e-9; t += res) {
      var seg = [], e = Math.min(length, t + res);
      for (i = 0; i < notes.length; i++) {
        var n = notes[i];
        if (n.mute) continue;
        var ov = Math.min(e, n.start + n.dur) - Math.max(t, n.start);
        if (ov > 1e-6) {
          // notes that start inside the segment weigh more than notes ringing over from before
          var w = ov * (n.start >= t - 1e-6 ? 1 : 0.5) * (0.5 + (n.vel || 100) / 254);
          seg.push({ pitch: n.pitch, w: w });
        }
      }
      var ch = seg.length ? T.detectChord(seg) : null;
      var last = out.length ? out[out.length - 1] : null;
      if (!ch) { if (last) last.dur += e - t; continue; }
      if (last && sameChord(last.chord, ch)) last.dur += e - t;
      else out.push({ start: t, dur: e - t, chord: ch });
    }
    // a chord that only differs from its neighbours by a passing bass note: absorb short slash chords
    for (i = out.length - 1; i > 0; i--) {
      var a = out[i - 1], b = out[i];
      if (b.dur < res * 1.01 && a.chord.root === b.chord.root && a.chord.q === b.chord.q) { a.dur += b.dur; out.splice(i, 1); }
    }
    return out;
  }

  BM.chords = {
    PRESETS: PRESETS, presetProgression: presetProgression, parseText: parseText, toText: toText,
    eventAt: eventAt, chordAt: chordAt, nextChange: nextChange, fitTo: fitTo, toNotes: toNotes,
    fromNotes: fromNotes, sameChord: sameChord
  };
})(BM);
