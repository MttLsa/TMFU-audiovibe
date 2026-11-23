// Bandmates engine - keyboard player: left hand + right hand, voicings with voice leading, follows the chord track.
(function (BM) {
  'use strict';
  var G = BM.groove, R = BM.rng, T = BM.theory, C = BM.chords, V = BM.voicing;

  // right-hand rhythms per movement (X accent, x hit, - hold, . rest); simple -> complex
  var MOVES = {
    sustain: { sus: 0.97, r: ['X---------------', 'X-------x-------', 'X-----x-x-------'] },
    pulse: { sus: 0.6, r: ['X---x---X---x---', 'X-x-X-x-X-x-X-x-', 'X-xxX-x-X-xxX-x-', 'XxxxXxxxXxxxXxxx'] },
    offbeat: { sus: 0.3, r: ['....x.......x...', '..x...x...x...x.', '..x.x.x...x.x.x.'] },
    comp: { sus: 0.7, r: ['X-----x-X-------', 'X--x--x-X--x----', 'X--x--x-x--x-x--'] },
    broken: { sus: 0.9, r: ['x-x-x-x-x-x-x-x-', 'x-x-x-x-x-x-x-x-', 'xxxxxxxxxxxxxxxx'] },
    arp: { sus: 0.8, r: ['x-x-x-x-x-x-x-x-', 'xxxxxxxxxxxxxxxx', 'xxxxxxxxxxxxxxxx'] }
  };
  // family-specific comping rhythms
  var COMP = {
    funk: { sus: 0.3, r: ['....X.......X...', '..x.X..x..x.X...', '.xX..x.X.xX..x.x', 'xX.x.xX..xX.x.xX'] },
    jazz: { sus: 0.65, r: ['X-----x.........', '...x-.....x-....', 'X-.x-.....x-..x-', '.x-..x-.x-...x-.'] },
    gospel: { sus: 0.75, r: ['X-----x---x-x---', 'X--x--x-X--x-x--', 'X-xx-x-xX-x--xx-'] },
    rnb: { sus: 0.85, r: ['X---------x-----', 'X-----x---x-----', 'X--x---x--x--x--', 'X-.x-x.--x-.x-x-'] },
    house: { sus: 0.45, r: ['..x...x...x...x.', 'x..x..x...x..x..', 'x..x..x.x..x..x.', '.xx..x.x.xx..x.x'] },
    techhouse: { sus: 0.3, r: ['......x.......x.', '...x..x....x..x.', '...x..x..x.x..x.'] },
    techno: { sus: 0.35, r: ['......x.........', '......x.......x.', '...x..x....x..x.'] },
    microhouse: { sus: 0.9, r: ['.......x--------', '...x-------x----', '..x---.x---x---.'] }
  };
  // electronic comping leaves the downbeat to the kick
  var ELECTRONIC = { house: 1, techhouse: 1, techno: 1, microhouse: 1 };
  var VAR = [0, 1, -1, 2, -2];

  function pick(list, cx, v) {
    var n = list.length, i = Math.round(G.clamp01(cx) * (n - 1)) + VAR[((v || 0) % 5 + 5) % 5];
    if (i < 0) i = -i;
    if (i > n - 1) i = Math.max(0, 2 * (n - 1) - i);
    return list[Math.max(0, Math.min(n - 1, i))];
  }

  function defaults(opts) {
    var persona = BM.styles.find(BM.styles.KEYBOARDISTS, opts.persona || 'clara'), o = {}, k;
    var base = { length: 16, beatsPerBar: 4, seed: 1, complexity: 0.4, intensity: 0.5, fills: 0.3, swing: 0, swingGrid: 16,
      feel: 0, humanize: 0.3, lh: 'octaves', rhVoicing: 'close', rhMove: 'pulse', range: 0.5, ext: 0.3, pedal: 0.3,
      muteLH: false, muteRH: false, prog: null, variation: 0, sectionName: '', followSections: true, arpDir: 'updown',
      fallbackRoot: 0, endFill: true };
    for (k in base) if (base.hasOwnProperty(k)) o[k] = base[k];
    for (k in persona.def) if (persona.def.hasOwnProperty(k)) o[k] = persona.def[k];
    for (k in opts) if (opts.hasOwnProperty(k) && opts[k] !== undefined) o[k] = opts[k];
    o.family = opts.family || persona.family;
    return o;
  }

  function place(pc, ref) { var p = ref - 6; while (T.mod12(p) !== T.mod12(pc)) p++; return p; }

  function generate(opts) {
    var o = defaults(opts);
    var bpb = o.beatsPerBar, spb = Math.round(bpb * 4), nBars = Math.max(1, Math.ceil(o.length / bpb - 1e-6));
    var sec = G.section(o.sectionName);
    var cx = G.clamp01(o.complexity + (o.followSections && sec ? sec.cx : 0));
    var I = G.clamp01(o.intensity + (o.followSections && sec ? sec.ld : 0));
    var prog = (o.prog && o.prog.length) ? C.fitTo(o.prog, o.length) : [{ start: 0, dur: o.length, chord: T.makeChord(o.fallbackRoot, 'maj', null) }];
    var center = Math.round(57 + 20 * G.clamp01(o.range));
    var move = MOVES[o.rhMove] || MOVES.pulse;
    if (o.rhMove === 'comp' && COMP[o.family]) move = COMP[o.family];
    var notes = [], b, i;

    // fills decided per bar
    var fillBars = {};
    for (b = 0; b < nBars && o.fills > 0; b++) {
      var fr = R.make(o.seed, 'keys-f', b), last = b === nBars - 1;
      var p = last && o.endFill ? 0.15 + 0.6 * o.fills : ((b + 1) % 8 === 0 ? o.fills * 0.8 : ((b + 1) % 4 === 0 ? o.fills * 0.45 : 0));
      if (fr.chance(p)) fillBars[b] = fr.chance(o.fills * cx) ? 2 : 1;
    }

    // ---------------- right hand ----------------
    if (!o.muteRH) {
      var prevV = null, arpIdx = 0;
      for (b = 0; b < nBars; b++) {
        var rng = R.make(o.seed, 'keys-r', b), barStart = b * bpb, barCx = G.clamp01(cx + ((b % 4) === 3 ? 0.06 : 0));
        var pat = pick(move.r, barCx, o.variation), onsets = [];
        for (var s = 0; s < spb; s++) {
          var c = pat.charAt(s % pat.length);
          if (c === 'X' || c === 'x') onsets.push({ t: barStart + s / 4, acc: c === 'X' });
        }
        // a chord change always gets a hit (unless the movement plays continuously anyway)
        for (var e = 0; e < prog.length; e++) {
          var cs = prog[e].start;
          if (cs < barStart - 1e-6 || cs >= barStart + bpb - 1e-6) continue;
          var hit = false;
          for (i = 0; i < onsets.length; i++) if (Math.abs(onsets[i].t - cs) < 1e-6) hit = true;
          if (!hit && o.rhMove !== 'offbeat' && !(o.rhMove === 'comp' && ELECTRONIC[o.family])) onsets.push({ t: cs, acc: true });
        }
        onsets.sort(function (a, c2) { return a.t - c2.t; });
        var fillFrom = fillBars[b] ? barStart + bpb - fillBars[b] : Infinity;
        for (i = 0; i < onsets.length; i++) {
          var on = onsets[i];
          if (on.t >= fillFrom - 1e-6) break;
          var ev = C.eventAt(prog, on.t + 1e-6);
          if (!ev || ev.chord.nc) continue;
          var ch = ev.chord, change = C.nextChange(prog, on.t);
          var nextT = i + 1 < onsets.length ? onsets[i + 1].t : barStart + bpb;
          var dur = (nextT - on.t) * move.sus;
          if (o.pedal > 0.5 && (o.rhMove === 'sustain' || o.rhMove === 'broken' || o.rhMove === 'arp')) dur = Math.max(dur, Math.min(change, barStart + bpb * 2) - on.t - 0.02);
          dur = Math.max(0.08, Math.min(dur, change - on.t - 0.01));
          if (prevV === null || Math.abs(ev.start - on.t) < 1e-6 || !prevV.length) prevV = V.voice(o.rhVoicing, ch, center, prevV, o.ext);
          var voicing = prevV, vel = 55 + 48 * I + (on.acc ? 6 : 0) - (Math.abs(on.t - Math.round(on.t)) > 1e-6 ? 5 : 0);
          if (o.rhMove === 'arp' || o.rhMove === 'broken') {
            var seq = arpSequence(voicing, o.rhMove === 'arp' ? o.arpDir : 'broken', cx > 0.6);
            var pitch = seq[arpIdx % seq.length];
            arpIdx++;
            notes.push({ pitch: pitch, start: on.t, dur: o.pedal > 0.5 ? Math.max(dur, Math.min(change - on.t - 0.02, 2)) : dur, vel: G.clampVel(vel - 4), role: 'rh' });
          } else {
            for (var k = 0; k < voicing.length; k++) {
              notes.push({ pitch: voicing[k], start: on.t, dur: dur, vel: G.clampVel(vel + (k === voicing.length - 1 ? 6 : 0) - (k === 0 ? 4 : 0)),
                           role: 'rh', strum: o.rhMove === 'sustain' ? k * 0.008 * o.humanize : 0 });
            }
          }
        }
        if (fillBars[b]) {
          var fev = makeFill(prog, fillFrom, barStart + bpb, prevV, center, cx, I, rng, o.length);
          for (i = 0; i < fev.length; i++) notes.push(fev[i]);
          if (fev.length) prevV = null;
        }
      }
    }

    // ---------------- left hand ----------------
    if (!o.muteLH && o.lh !== 'off') {
      var lhCenter = 43;
      for (b = 0; b < nBars; b++) {
        var lrng = R.make(o.seed, 'keys-l', b), bs = b * bpb;
        var hits = lhRhythm(o.lh, bpb, cx, lrng);
        for (var e2 = 0; e2 < prog.length; e2++) {
          var st2 = prog[e2].start;
          if (st2 > bs + 1e-6 && st2 < bs + bpb - 1e-6) {
            var exists = false;
            for (i = 0; i < hits.length; i++) if (Math.abs(bs + hits[i].t - st2) < 1e-6) exists = true;
            if (!exists) hits.push({ t: st2 - bs, k: 0, len: 0 });
          }
        }
        hits.sort(function (a, c2) { return a.t - c2.t; });
        for (i = 0; i < hits.length; i++) {
          var t = bs + hits[i].t, ev2 = C.eventAt(prog, t + 1e-6);
          if (!ev2 || ev2.chord.nc || t >= o.length - 1e-6) continue;
          var ch2 = ev2.chord, ch2Change = C.nextChange(prog, t);
          var nt = i + 1 < hits.length ? bs + hits[i + 1].t : bs + bpb;
          var d2 = hits[i].len ? hits[i].len : (nt - t) * (o.pedal > 0.3 ? 0.98 : 0.85);
          if (o.pedal > 0.5 && o.lh === 'broken') d2 = Math.max(d2, Math.min(ch2Change, bs + bpb) - t - 0.02);
          d2 = Math.max(0.1, Math.min(d2, ch2Change - t - 0.01));
          var root = place(T.bassPc(ch2), lhCenter - 4), lv = 50 + 42 * I + (hits[i].t === 0 ? 6 : 0);
          var pcs = lhPitches(o.lh, hits[i].k, ch2, root);
          for (var q = 0; q < pcs.length; q++) notes.push({ pitch: pcs[q], start: t, dur: d2, vel: G.clampVel(lv - q * 3), role: 'lh' });
        }
      }
    }

    var hr = R.make(o.seed, 'keys-human');
    notes = G.apply(notes, { swing: o.swing, swingGrid: o.swingGrid, feel: o.feel, humanize: o.humanize, rng: hr, length: o.length });
    return G.cleanup(notes);
  }

  // LH rhythm: list of {t (beats in bar), k (pattern index), len}
  function lhRhythm(mode, bpb, cx, rng) {
    var out = [], t, n;
    switch (mode) {
      case 'root': out.push({ t: 0, k: 0 }); if (cx > 0.55) out.push({ t: bpb / 2, k: 0 }); break;
      case 'octaves':
        out.push({ t: 0, k: 0 });
        if (cx > 0.3) out.push({ t: bpb / 2, k: 0 });
        if (cx > 0.65) { out.push({ t: bpb / 2 - 0.5, k: 1, len: 0.4 }); }
        break;
      case 'root5':
        if (cx > 0.5) { for (t = 0; t < bpb; t++) out.push({ t: t, k: [0, 1, 2, 1][t % 4] }); }
        else { out.push({ t: 0, k: 0 }); out.push({ t: bpb / 2, k: 1 }); }
        break;
      case 'broken':
        var step = cx > 0.7 ? 0.25 : 0.5;
        for (t = 0, n = 0; t < bpb - 1e-6; t += step, n++) out.push({ t: t, k: n, len: 0 });
        break;
      case 'shell':
        out.push({ t: 0, k: 0 });
        if (cx > 0.4) out.push({ t: 1.5, k: 0, len: 0.4 });
        if (cx > 0.7 && rng.chance(0.5)) out.push({ t: 3, k: 0, len: 0.4 });
        break;
      case 'stride':
        for (t = 0; t < bpb; t++) out.push({ t: t, k: t % 2 === 0 ? 0 : 9, len: 0.45 });
        break;
      default: out.push({ t: 0, k: 0 });
    }
    return out;
  }

  // root = the bass note (slash bass when present); chord tones are taken from the chord's own root
  function lhPitches(mode, k, ch, root) {
    var cr = root - T.mod12(T.bassPc(ch) - ch.root);
    var fifth = cr + T.fifthOf(ch), third = cr + T.thirdOf(ch) + 12, sev = T.seventhOf(ch);
    if (fifth <= root) fifth += 12;
    switch (mode) {
      case 'root': return [root];
      case 'octaves': return k === 1 ? [root + 12] : [root, root + 12];
      case 'root5': return [[root, fifth, root + 12][k % 3]];
      case 'broken': return [[root, fifth, root + 12, third, root + 12, fifth][k % 6]];
      case 'shell':
        var top = cr + (sev !== null ? sev : T.thirdOf(ch) + 12);
        while (top <= root + 2) top += 12;
        while (top - root > 16) top -= 12;
        return [root, top];
      case 'stride': return k === 0 ? [root] : V.close(ch, 3, 55, null, 0);
      default: return [root];
    }
  }

  function arpSequence(v, dir, wide) {
    var up = v.slice(), seq;
    if (wide) up.push(v[0] + 12);
    if (dir === 'down') seq = up.slice().reverse();
    else if (dir === 'up') seq = up;
    else if (dir === 'broken') seq = up.length >= 3 ? [up[0], up[up.length - 1], up[1], up[up.length - 1]].concat(up.length > 3 ? [up[2], up[up.length - 1]] : []) : up;
    else { seq = up.slice(); for (var i = up.length - 2; i > 0; i--) seq.push(up[i]); }
    return seq;
  }

  // right-hand fill: chord-scale run leading into the next chord's top voice
  function makeFill(prog, from, to, prevV, center, cx, I, rng, length) {
    var out = [], ev = C.eventAt(prog, from + 1e-6);
    if (!ev || ev.chord.nc) return out;
    var ch = ev.chord, nextEv = C.eventAt(prog, Math.min(length - 1e-3, to + 1e-6)) || ev;
    var nch = nextEv.chord.nc ? ch : nextEv.chord;
    var targetV = V.voice('close', nch, center, prevV, 0), target = targetV[targetV.length - 1];
    var step = cx > 0.5 ? 0.25 : 0.5, n = Math.round((to - from) / step), sc = T.chordScale(ch);
    var up = rng.chance(0.65), p = target + (up ? -1 : 1) * Math.round(n * 1.7);
    for (var i = 0; i < n; i++) {
      var q = p + (up ? 1 : -1) * Math.round(i * 1.7);
      for (var d = 0; d < 3; d++) { if (sc.indexOf(T.mod12(q - ch.root)) >= 0) break; q += up ? 1 : -1; }
      out.push({ pitch: q, start: from + i * step, dur: step * 0.9, vel: G.clampVel(55 + 40 * I + i * 2), role: 'rh' });
    }
    return out;
  }

  BM.keys = { generate: generate, MOVES: MOVES, defaults: defaults,
    LH_MODES: ['off', 'root', 'octaves', 'root5', 'broken', 'shell', 'stride'],
    RH_MOVES: ['sustain', 'pulse', 'comp', 'broken', 'arp', 'offbeat'] };
})(BM);
