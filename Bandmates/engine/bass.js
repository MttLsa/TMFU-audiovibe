// Bandmates engine - bass player: follows the chord track and (optionally) the drummer's kick.
(function (BM) {
  'use strict';
  var G = BM.groove, R = BM.rng, T = BM.theory, C = BM.chords;

  // X accent, x note, o soft note, - sustain, . rest, d dead note. Simple -> complex.
  var LIB = {
    soul: { sus: 0.8, oct: 0.4, rhythms: ['X--.--x.X--.----', 'X--x--x-X--.x-x-', 'X--x-x--X--x-xx-', 'X-xx-x-xX-xx-x-x'] },
    walking: { sus: 0.92, oct: 0.1, walk: true, rhythms: ['X---x---x---x---', 'X---x---x---x-x-', 'X---x-x-x---x-x-'] },
    rnb: { sus: 0.9, oct: 0.3, rhythms: ['X-----------....', 'X------x--x-....', 'X-----.x--x-.x--', 'X--x--.x-xx-.x-x'] },
    rock: { sus: 0.85, oct: 0.3, rhythms: ['X-------X-------', 'X---x---X---x---', 'X-x-x-x-X-x-x-x-', 'X-x-x-xxX-x-x-xx', 'X-xxx-x-X-xxx-xx'] },
    pop: { sus: 0.85, oct: 0.25, rhythms: ['X-------X-------', 'X-------X---x---', 'X-----x-X---x---', 'X--x--x-X--x-x--'] },
    funk: { sus: 0.5, oct: 1.0, rhythms: ['X-----x.X-......', 'X--x..x.X-.x.x..', 'X.dxx.x.dx.xX.x.', 'X.xX.dx.x.dxX.xd', 'Xdxx.dxXdx.xxdx.'] },
    // electronic: the kick sits on the beats, the bass fills the gaps between kicks
    house: { sus: 0.7, oct: 0.8, electronic: true, rhythms: ['..X-..X-..X-..X-', '..X-..X-..X-..xx', '..X.x.X-..X.x.X-', '.xX.x.X-.xX.x.Xx'] },
    techhouse: { sus: 0.6, oct: 0.5, electronic: true, rhythms: ['..X...X...X...X.', '..Xx..X...Xx..X.', '.xXx..Xx.xXx..Xx', '.xXx.xXx.xXx.xXx'] },
    techno: { sus: 0.5, oct: 0.15, electronic: true, rhythms: ['..x...x...x...x.', '.xx..xx..xx..xx.', '.xxx.xxx.xxx.xxx', '.xxx.xxx.xxx.xxx'] },
    microhouse: { sus: 0.45, oct: 0.9, electronic: true, rhythms: ['...x.......x....', '...x..x....x....', '..x..x.x...x..x.', '.x.x..x.x..xx..x'] }
  };
  var VAR = [0, 1, -1, 2, -2];

  function pickRhythm(list, cx, v) {
    var n = list.length, i = Math.round(G.clamp01(cx) * (n - 1)) + VAR[((v || 0) % 5 + 5) % 5];
    if (i < 0) i = -i;
    if (i > n - 1) i = Math.max(0, 2 * (n - 1) - i);
    return list[Math.max(0, Math.min(n - 1, i))];
  }

  function defaults(opts) {
    var persona = BM.styles.find(BM.styles.BASSISTS, opts.persona || 'lino'), o = {}, k;
    var base = { length: 16, beatsPerBar: 4, seed: 1, complexity: 0.4, intensity: 0.5, fills: 0.3, swing: 0, swingGrid: 16,
      feel: 0, humanize: 0.3, slides: 0.2, slideMode: 'grace', deadNotes: 0.2, followKick: 0, kick: null, octave: 0,
      prog: null, sectionName: '', followSections: true, variation: 0, sustain: null, fallbackRoot: 0, endFill: true };
    for (k in base) if (base.hasOwnProperty(k)) o[k] = base[k];
    for (k in persona.def) if (persona.def.hasOwnProperty(k)) o[k] = persona.def[k];
    for (k in opts) if (opts.hasOwnProperty(k) && opts[k] !== undefined) o[k] = opts[k];
    o.family = opts.family || persona.family;
    return o;
  }

  // nearest pitch of class pc to `ref`, inside [lo, hi]
  function place(pc, ref, lo, hi) {
    var best = null, bd = 1e9;
    for (var p = lo; p <= hi; p++) {
      if (T.mod12(p) !== pc) continue;
      var d = Math.abs(p - ref);
      if (d < bd) { bd = d; best = p; }
    }
    return best === null ? lo + T.mod12(pc - lo) : best;
  }

  function generate(opts) {
    var o = defaults(opts), fam = LIB[o.family] || LIB.rock;
    var bpb = o.beatsPerBar, spb = Math.round(bpb * 4), nBars = Math.max(1, Math.ceil(o.length / bpb - 1e-6));
    var sec = G.section(o.sectionName);
    var cx = G.clamp01(o.complexity + (o.followSections && sec ? sec.cx : 0));
    var I = G.clamp01(o.intensity + (o.followSections && sec ? sec.ld : 0));
    var prog = (o.prog && o.prog.length) ? C.fitTo(o.prog, o.length) : [{ start: 0, dur: o.length, chord: T.makeChord(o.fallbackRoot, 'maj', null) }];
    var lo = 28 + 12 * o.octave, hi = lo + 26, center = lo + 9;
    var sus = o.sustain === null ? fam.sus : 0.3 + 0.7 * o.sustain;
    var onsets = []; // {t, ch: char}

    // 1) rhythm
    for (var b = 0; b < nBars; b++) {
      var rng = R.make(o.seed, 'bass-r', b), barStart = b * bpb;
      var barCx = G.clamp01(cx + ((b % 4) === 3 ? 0.08 : 0));
      var pat = pickRhythm(fam.rhythms, barCx, o.variation), steps = {};
      for (var s = 0; s < spb; s++) {
        var c = pat.charAt(s % pat.length);
        if (c !== '-' && c !== '.') steps[s] = c;
        else if (c === '-') steps[s] = steps[s] || null;
      }
      // follow the drummer's kick
      if (o.kick && o.kick.length && o.followKick > 0) {
        var kickSteps = {};
        for (var ki = 0; ki < o.kick.length; ki++) {
          var kt = o.kick[ki] - barStart;
          if (kt >= -1e-6 && kt < bpb - 1e-6) kickSteps[Math.round(kt * 4)] = 1;
        }
        for (var ks in kickSteps) if (kickSteps.hasOwnProperty(ks) && !steps[ks] && rng.chance(o.followKick)) steps[ks] = 'x';
        for (var st in steps) {
          if (!steps.hasOwnProperty(st) || !steps[st] || +st === 0 || kickSteps[st]) continue;
          if (steps[st] !== 'd' && rng.chance(o.followKick * 0.6)) steps[st] = null;
        }
      }
      // every chord change gets a note (sometimes anticipated by a 16th)
      for (var e = 0; e < prog.length; e++) {
        var cs = prog[e].start - barStart;
        if (cs < -1e-6 || cs >= bpb - 1e-6) continue;
        var cstep = Math.round(cs * 4);
        if (!steps[cstep] && !(fam.electronic && cstep % 4 === 0 && cs > 0 && cs < bpb)) {
          if (fam.electronic) { var nx = cstep; while (nx < spb && !steps[nx]) nx++; if (nx < spb) continue; }
          var antic = cstep > 0 && o.family !== 'walking' && rng.chance(cx * 0.3) && !steps[cstep - 1];
          steps[antic ? cstep - 1 : cstep] = 'X';
        }
      }
      // extra dead notes
      if (o.deadNotes > 0 && o.family !== 'walking') {
        for (var ds = 1; ds < spb; ds++) if (!steps[ds] && ds % 2 === 1 && rng.chance(o.deadNotes * 0.2 * cx)) steps[ds] = 'd';
      }
      for (var k2 = 0; k2 < spb; k2++) {
        if (!steps[k2]) continue;
        if (steps[k2] === 'd' && !rng.chance(o.deadNotes)) continue;
        onsets.push({ t: barStart + k2 / 4, c: steps[k2], bar: b });
      }
    }
    onsets.sort(function (a, c2) { return a.t - c2.t; });

    // 2) fills: replace the end of phrase bars with a run
    var fills = [];
    if (o.fills > 0) {
      for (var fb = 0; fb < nBars; fb++) {
        var fr = R.make(o.seed, 'bass-f', fb), last = fb === nBars - 1;
        var p = last && o.endFill ? 0.2 + 0.6 * o.fills : ((fb + 1) % 8 === 0 ? o.fills * 0.8 : ((fb + 1) % 4 === 0 ? o.fills * 0.5 : ((fb + 1) % 2 === 0 ? o.fills * o.fills * 0.15 : 0)));
        if (!fr.chance(p)) continue;
        var flen = fr.chance(o.fills * cx) ? 2 : 1, fstart = (fb + 1) * bpb - flen;
        fills.push({ start: fstart, end: (fb + 1) * bpb, step: cx > 0.45 && o.family !== 'walking' ? 0.25 : 0.5, up: fr.chance(0.5) });
      }
      onsets = onsets.filter(function (on) {
        for (var i = 0; i < fills.length; i++) if (on.t >= fills[i].start - 1e-6 && on.t < fills[i].end - 1e-6) return false;
        return true;
      });
      for (var fi = 0; fi < fills.length; fi++) for (var ft = fills[fi].start; ft < fills[fi].end - 1e-6; ft += fills[fi].step) onsets.push({ t: ft, c: 'x', fill: fills[fi] });
      onsets.sort(function (a, c2) { return a.t - c2.t; });
    }

    // 3) pitches
    var notes = [], prev = center, prevPc = null, prevCh = null;
    for (var i = 0; i < onsets.length; i++) {
      var on = onsets[i], pr = R.make(o.seed, 'bass-p', Math.round(on.t * 16));
      var ev = C.eventAt(prog, on.t + 1e-6), ch = ev && !ev.chord.nc ? ev.chord : null;
      if (!ch) { prevCh = null; continue; }
      var nextT = i + 1 < onsets.length ? onsets[i + 1].t : o.length;
      var change = C.nextChange(prog, on.t);
      var nextEv = change < o.length ? C.eventAt(prog, change + 1e-6) : null;
      var nextCh = nextEv && !nextEv.chord.nc ? nextEv.chord : null;
      var isStart = !prevCh || !C.sameChord(prevCh, ch) || Math.abs(ev.start - on.t) < 1e-6;
      var anticipates = nextCh && change - on.t <= 0.25 + 1e-6 && change - on.t > 1e-6;
      var lastBeforeChange = nextCh && nextT >= change - 1e-6;
      var pitch, vel = 62 + 48 * I, dead = on.c === 'd', dur = (nextT - on.t) * sus;

      if (dead) {
        pitch = prevPc === null ? place(T.bassPc(ch), prev, lo, hi) : prev;
        notes.push({ pitch: pitch, start: on.t, dur: 0.06, vel: 22 + 18 * I, role: 'dead' });
        continue;
      }
      if (anticipates) { ch = nextCh; isStart = true; }
      if (on.fill) {
        // scale run towards the next chord's bass note
        var target = nextCh ? T.bassPc(nextCh) : T.bassPc(ch);
        var sc = T.chordScale(ch), dir = on.fill.up ? 1 : -1;
        var remaining = Math.round((on.fill.end - on.t) / on.fill.step);
        var tp = place(target, prev + dir * 5, lo, hi);
        if (remaining === 1) pitch = tp - dir;          // chromatic approach
        else {
          var cand = tp - dir * remaining * 2;
          pitch = snapToScale(cand, ch.root, sc);
        }
        pitch = clampRange(pitch, lo, hi);
        vel = 58 + 40 * I + (remaining === 1 ? 6 : 0);
        dur = on.fill.step * 0.85;
      } else if (isStart) {
        pitch = place(T.bassPc(ch), prevPc === null ? center : prev, lo, hi);
        if (pitch > center + 7 && pitch - 12 >= lo) pitch -= 12;
        vel += 8;
      } else if (lastBeforeChange && pr.chance(0.15 + 0.6 * cx) && nextCh) {
        // approach note into the next chord (chromatic / scale / dominant)
        var tgt = place(T.bassPc(nextCh), prev, lo, hi), kind = pr.weighted([0.5 + cx, 0.8, 0.4]);
        if (kind === 0) pitch = tgt + (pr.chance(0.6) ? -1 : 1);
        else if (kind === 1) pitch = snapToScale(tgt + (tgt > prev ? -2 : 2), ch.root, T.chordScale(ch));
        else pitch = tgt + 7 > hi ? tgt - 5 : tgt + 7;
        vel -= 6;
      } else if (fam.walk) {
        // walking: stepwise through chord/scale tones towards the next root
        var goal = nextCh ? place(T.bassPc(nextCh), prev, lo, hi) : place(ch.root, prev, lo, hi) + (pr.chance(0.5) ? 12 : 0);
        var stepDir = goal > prev ? 1 : -1, tonesC = T.chordPcs(ch);
        pitch = prev + stepDir * (pr.chance(0.5) ? 2 : (pr.chance(0.5) ? 3 : 4));
        pitch = snapToChordOrScale(pitch, ch, tonesC, pr.chance(0.7));
        if (pitch === prev) pitch += stepDir * 2;
        pitch = clampRange(pitch, lo, hi);
      } else {
        // root / octave / fifth / third / seventh
        var sev = T.seventhOf(ch);
        var w = [1.2 - 0.6 * cx, fam.oct * (0.4 + cx) * (0.5 + I), 0.3 + 0.5 * cx, 0.1 + 0.4 * cx * cx, sev !== null ? 0.3 * cx : 0];
        var choice = pr.weighted(w), rootP = place(T.bassPc(ch), prev, lo, hi);
        if (choice === 0) pitch = rootP;
        else if (choice === 1) pitch = rootP + 12 <= hi && (prev <= rootP || pr.chance(0.5)) ? rootP + 12 : rootP;
        else if (choice === 2) pitch = place(T.mod12(ch.root + T.fifthOf(ch)), prev, lo, hi);
        else if (choice === 3) pitch = place(T.mod12(ch.root + T.thirdOf(ch)), prev, lo, hi);
        else pitch = place(T.mod12(ch.root + sev), prev, lo, hi);
        if (on.c === 'o') vel -= 12;
      }
      if (on.c === 'X') vel += 6;
      if (Math.abs(on.t - Math.round(on.t)) > 1e-6) vel -= 5;
      // never ring over a chord change
      if (!anticipates && change < Infinity && on.t + dur > change) dur = Math.max(0.1, change - on.t - 0.02);
      dur = Math.max(0.08, dur);

      // slides into chord-start notes and octave jumps
      var slid = false;
      if (o.slides > 0 && notes.length && (isStart || Math.abs(pitch - prev) >= 7) && !on.fill && pr.chance(o.slides * 0.55)) {
        slid = true;
        if (o.slideMode === 'legato') {
          var pn = lastPitched(notes);
          if (pn) pn.dur = Math.max(pn.dur, on.t - pn.start + 0.06);
        } else {
          var from = pitch + (pitch > prev ? -2 : 2) * (pr.chance(0.5) ? 1 : 0.5);
          from = Math.round(from);
          var gs = Math.max(0, on.t - 0.125);
          notes.push({ pitch: clampRange(from, lo - 2, hi + 2), start: gs, dur: on.t - gs + 0.02, vel: vel - 15, role: 'slide', tight: true });
        }
      }
      notes.push({ pitch: pitch, start: on.t, dur: dur, vel: G.clampVel(vel), role: slid ? 'slid' : 'bass' });
      prev = pitch; prevPc = T.mod12(pitch); prevCh = ch;
    }

    var hr = R.make(o.seed, 'bass-human');
    notes = G.apply(notes, { swing: o.swing, swingGrid: o.swingGrid, feel: o.feel, humanize: o.humanize, rng: hr, length: o.length });
    return G.cleanup(notes);
  }

  function lastPitched(notes) {
    for (var i = notes.length - 1; i >= 0; i--) if (notes[i].role !== 'dead' && notes[i].role !== 'slide') return notes[i];
    return null;
  }
  function clampRange(p, lo, hi) { while (p < lo) p += 12; while (p > hi) p -= 12; return p; }
  function snapToScale(p, root, sc) {
    for (var d = 0; d < 6; d++) {
      if (sc.indexOf(T.mod12(p - d - root)) >= 0) return p - d;
      if (sc.indexOf(T.mod12(p + d - root)) >= 0) return p + d;
    }
    return p;
  }
  function snapToChordOrScale(p, ch, tones, chordOnly) {
    if (!chordOnly) return snapToScale(p, ch.root, T.chordScale(ch));
    for (var d = 0; d < 6; d++) {
      if (tones.indexOf(T.mod12(p + d)) >= 0) return p + d;
      if (tones.indexOf(T.mod12(p - d)) >= 0) return p - d;
    }
    return p;
  }

  BM.bass = { generate: generate, LIB: LIB, defaults: defaults };
})(BM);
