// Bandmates engine - electronic drummer ("Beats"): House, Tech House, Techno, Microhouse, Rominimal.
// Every kit piece has its own on/off switch and density control; the XY pad shifts them all.
(function (BM) {
  'use strict';
  var G = BM.groove, R = BM.rng;

  var VOICES = ['kick', 'clap', 'rim', 'hat', 'ohat', 'perc', 'ride', 'crash'];
  // GM / Drum Rack notes (C1 = 36)
  var MAP = { kick: 36, rim: 37, snare: 38, clap: 39, hat: 42, ohat: 46, crash: 49, perc: 50, ride: 51 };

  // One char per 16th; 16 chars = 1 bar, 32 chars = 2-bar pattern. X accent, x normal, o ghost, . rest.
  // Lists go from sparse to dense.
  var LIB = {
    house: {
      fills: ['roll', 'drop', 'openhats'], phrase: 8,
      kick: ['X...X...X...X...', 'X...X...X...X..o', 'X...X...X..oX...', 'X...X..oX...X..o'],
      clap: ['....X.......X...', '....X.......X..x', '....X..x....X...', '....X..x....X.xo'],
      rim: ['................', '..........x.....', '...x......x.....', '...x..x...x..x..'],
      hat: ['................', 'x.x.x.x.x.x.x.x.', 'xxo.xxo.xxo.xxo.', 'xxxxxxxxxxxxxxxx'],
      ohat: ['..X...X...X...X.', '..X...X...X...X.', '..X...X...X...Xo'],
      perc: ['................', 'x.xxx.xxx.xxx.xx', 'xxxxXxxxxxxxXxxx', '..x..x.x..x..xx.'],
      ride: ['................', 'x.x.x.x.x.x.x.x.', 'x.xxx.xxx.xxx.xx']
    },
    techhouse: {
      fills: ['drop', 'roll', 'perc'], phrase: 8,
      kick: ['X...X...X...X...', 'X...X...X...X..o', 'X...X..oX...X...', 'X..oX...X..oX..o'],
      clap: ['....X.......X...', '....X.......X...', '....X..o....X.o.', '....X..o.o..X.o.'],
      rim: ['......x.......x.', '...x..x....x..x.', '.x.x..x..x.x..x.', 'ox.xo.x.ox.xo.xx'],
      hat: ['................', 'x.x.x.x.x.x.x.x.', 'xx.xxx.xxx.xxx.x', 'xxxxxxxxxxxxxxxx'],
      ohat: ['..X...X...X...X.', '..X...X...X...X.', '..X...X..oX...X.'],
      perc: ['..........x.....................', '..........x.....x...........x...', '...x...x..x...x....x...x..x.x..x', '.x.x..x.x.xx..x..x.x..x.x.x...xx'],
      ride: ['................', 'X.x.X.x.X.x.X.x.']
    },
    techno: {
      fills: ['drop', 'perc', 'roll'], phrase: 8,
      kick: ['X...X...X...X...', 'X...X...X...X...', 'X...X...X...X.o.', 'X..oX..oX..oX..o'],
      clap: ['................', '....X.......X...', '....X.......X...', '....X..x....X..x'],
      rim: ['................', '.........x......', '...x.....x.....x', '...x..x..x..x..x'],
      hat: ['..x...x...x...x.', 'x.X.x.X.x.X.x.X.', 'xxXxxxXxxxXxxxXx', 'xxXxxxXxxxXxxxXx'],
      ohat: ['................', '..X...X...X...X.', '..X...X...X...X.'],
      perc: ['................', '......x.......x.', '..x...x..x....x.', '.xx..x.x.xx..x.x'],
      ride: ['................', '..x...x...x...x.', 'x.x.x.x.x.x.x.x.']
    },
    microhouse: {
      fills: ['drop', 'perc'], phrase: 8, skippy: true,
      kick: ['X...X...X...X...', 'X...X...X...X...', 'X...X..oX...X...', 'X..oX...X.o.X...'],
      clap: ['................', '........X.......', '....x.......X...', '....x..o....X.o.'],
      rim: ['..x.......x.....', '..x..x....x..x..', '.xx..x.x..x..x.x', 'oxx.ox.xoox.xxox'],
      hat: ['..x...x...x...x.', 'x.x.x.x.x.x.x.x.', 'xx.x.xx.x.xx.x.x', 'oxxoxxoxoxxoxxox'],
      ohat: ['................', '..............x.', '......x.......x.'],
      perc: ['...........x....................', '...x.......x.......x.....x......', '.x.x...x..x...x..x...x.x...x..x.', 'ox.xo.xxo.x.oxx.xo.x.ox.o.xxo.x.'],
      ride: ['................', '..x...x...x...x.']
    },
    rominimal: {
      fills: ['perc', 'drop'], phrase: 16, skippy: true,
      kick: ['X...X...X...X...', 'X...X...X...X...', 'X...X..oX...X...', 'X...X..oX...X..o'],
      clap: ['................', '........x...............x.......', '....x.......X.......x.......X...'],
      rim: ['.....x.......x..', '..x..x....x..x..', '.x.x.xx..x.x.xx.'],
      hat: ['..x...x...x...x.', 'xxx.xxx.xxx.xxx.', 'x.xxx.xxx.xxx.xx'],
      ohat: ['................', '..x...x...x...x.', '..X...X...X...X.'],
      perc: ['..x.....x.......x.x.....x.......', '..x..x..x..x..x.x.x..x..x..x.x..', 'xx.x.xx.xx.x.xx.xx.x.xx.x.xx.xx.'],
      ride: ['................', 'x.x.x.x.x.x.x.x.']
    }
  };

  // timing personality per voice, in beats at micro = 1 (positive = late)
  var MICRO = { clap: 0.012, rim: 0.02, hat: 0.008, ohat: 0.006, perc: 0.025, ride: 0.004, snare: 0.004 };

  function velFor(ch, L) {
    if (ch === 'X') return 92 + 33 * L;
    if (ch === 'x') return 64 + 40 * L;
    if (ch === 'o') return 34 + 26 * L;
    return 0;
  }

  function defaults(opts) {
    var persona = BM.styles.find(BM.styles.BEATMAKERS, opts.persona || 'volt'), o = {}, k, v;
    var base = { length: 16, beatsPerBar: 4, seed: 1, complexity: 0.5, loudness: 0.6, fills: 0.3, swing: 0.2, swingGrid: 16,
      feel: 0, humanize: 0.2, micro: 0.1, sectionName: '', followSections: true, map: null, endFill: true };
    for (v = 0; v < VOICES.length; v++) {
      base['on_' + VOICES[v]] = VOICES[v] !== 'ride';
      base['d_' + VOICES[v]] = 0.5;
    }
    for (k in base) if (base.hasOwnProperty(k)) o[k] = base[k];
    for (k in persona.def) if (persona.def.hasOwnProperty(k)) o[k] = persona.def[k];
    for (k in opts) if (opts.hasOwnProperty(k) && opts[k] !== undefined) o[k] = opts[k];
    o.family = opts.family || persona.family;
    return o;
  }

  function pickPattern(list, eff, variation) {
    var n = list.length, i = Math.round(G.clamp01(eff) * (n - 1));
    return list[Math.max(0, Math.min(n - 1, i))];
  }

  function generate(opts) {
    var o = defaults(opts), fam = LIB[o.family] || LIB.house;
    var bpb = o.beatsPerBar, spb = Math.round(bpb * 4), nBars = Math.max(1, Math.ceil(o.length / bpb - 1e-6));
    var sec = o.followSections ? G.section(o.sectionName) : null;
    var cx = G.clamp01(o.complexity + (sec ? sec.cx : 0)), L = G.clamp01(o.loudness + (sec ? sec.ld : 0));
    var isBreak = sec && sec.id === 'break', isBuild = sec && sec.id === 'pre', isIntro = sec && (sec.id === 'intro' || sec.id === 'outro');
    var events = [], v, s, b;

    for (b = 0; b < nBars; b++) {
      var rng = R.make(o.seed, 'beats', b), barStart = b * bpb, ev = [];
      var phraseLen = fam.phrase, barCx = G.clamp01(cx + ((b % 4) === 3 ? 0.06 : 0));
      for (v = 0; v < VOICES.length; v++) {
        var voice = VOICES[v];
        if (voice === 'crash' || !o['on_' + voice] || !fam[voice]) continue;
        if (voice === 'kick' && isBreak) continue;
        if (isIntro && cx < 0.5 && (voice === 'clap' || voice === 'perc') && b < nBars / 2) continue;
        var eff = G.clamp01(o['d_' + voice] + (barCx - 0.5) * 0.6);
        var pat = pickPattern(fam[voice], eff);
        var off = (pat.length > 16) ? (b % Math.round(pat.length / spb || 1)) * spb : 0;
        for (s = 0; s < spb; s++) {
          var c = pat.charAt((off + s) % pat.length);
          if (c === '.') continue;
          // skippy genres drop / add the odd hit so bars don't repeat exactly
          if (fam.skippy && voice !== 'kick' && rng.chance(0.12 * barCx) && c !== 'X') continue;
          ev.push({ role: voice, pos: s, vel: velFor(c, L) * (isBreak && voice !== 'perc' ? 0.8 : 1) });
        }
        if (fam.skippy && (voice === 'rim' || voice === 'perc') && rng.chance(0.35 * barCx)) {
          var extra = 1 + 2 * rng.int(spb / 2);
          ev.push({ role: voice, pos: Math.min(spb - 1, extra), vel: velFor('o', L) + 10 * rng.next() });
        }
      }
      // open hat closes the closed hat on the same step
      var ohSteps = {};
      for (var i = 0; i < ev.length; i++) if (ev[i].role === 'ohat') ohSteps[ev[i].pos] = 1;
      ev = ev.filter(function (e) { return !(e.role === 'hat' && ohSteps[e.pos]); });

      // crash / FX on phrase starts
      var phraseStart = b % phraseLen === 0 && (b > 0 || (sec ? sec.crash : L > 0.55));
      if (o.on_crash && phraseStart && L > 0.3 && !isBreak) ev.push({ role: 'crash', pos: 0, vel: 85 + 35 * L * o.d_crash });

      // fills / builds at phrase ends
      var lastOfPhrase = (b + 1) % phraseLen === 0 || (b + 1) % (phraseLen / 2) === 0 && o.fills > 0.6;
      var isLast = b === nBars - 1;
      if (isBuild && b >= nBars - 2) ev = ev.concat(build(b - (nBars - 2), spb, L, ev, o));
      else if (o.fills > 0 && (lastOfPhrase || (isLast && o.endFill)) && rng.chance(0.3 + 0.7 * o.fills)) {
        var type = fam.fills[rng.weighted(fam.fills.map(function (x, k) { return k === 0 ? 1.4 : 1; }))];
        var beats = o.fills > 0.66 && rng.chance(0.5) ? 2 : 1, fs = spb - beats * 4;
        ev = fill(type, ev, fs, spb, L, o, rng);
      }
      for (var k = 0; k < ev.length; k++) { ev[k].start = barStart + ev[k].pos / 4; events.push(ev[k]); }
    }

    var notes = [];
    for (var n = 0; n < events.length; n++) {
      var e = events[n];
      if (e.start >= o.length - 1e-9) continue;
      if (e.role !== 'snare' && !o['on_' + e.role]) continue;
      if (e.role === 'snare' && !o.on_clap) continue;
      var pitch = (o.map && o.map[e.role] !== undefined) ? o.map[e.role] : MAP[e.role];
      notes.push({ pitch: pitch, start: e.start, dur: e.role === 'ohat' || e.role === 'crash' ? 0.5 : 0.2,
                   vel: G.clampVel(e.vel), role: e.role, locked: e.role === 'kick' });
    }
    notes = G.apply(notes, { swing: o.swing, swingGrid: o.swingGrid, feel: o.feel, humanize: o.humanize * 0.6,
      rng: R.make(o.seed, 'beats-human'), length: o.length, feelWeights: { kick: 0, clap: 1, snare: 1, rim: 1, hat: 0.6, ohat: 0.6, perc: 1, ride: 0.5, crash: 0 } });
    // micro-timing: each voice leans off the grid (late), plus small random drift; the kick stays on the grid
    if (o.micro > 0) {
      var mr = R.make(o.seed, 'beats-micro');
      for (n = 0; n < notes.length; n++) {
        var r = notes[n].role;
        if (r === 'kick' || r === 'crash') continue;
        var sh = (MICRO[r] || 0) * o.micro + mr.gauss() * 0.008 * o.micro;
        notes[n].start = Math.max(0, notes[n].start + sh);
        if (notes[n].start + notes[n].dur > o.length) notes[n].dur = Math.max(0.01, o.length - notes[n].start);
        if (notes[n].start >= o.length) notes[n].start = o.length - 0.01;
      }
    }
    return G.cleanup(notes);
  }

  function fill(type, ev, fs, spb, L, o, rng) {
    var n = spb - fs, i;
    if (type === 'drop') {
      ev = ev.filter(function (e) { return !(e.role === 'kick' && e.pos >= fs); });
      if (o.on_clap) ev.push({ role: 'clap', pos: spb - 1, vel: 70 + 30 * L });
      return ev;
    }
    if (type === 'openhats' && o.on_ohat) {
      ev = ev.filter(function (e) { return !((e.role === 'hat' || e.role === 'ohat') && e.pos >= fs); });
      for (i = 0; i < n; i++) ev.push({ role: 'ohat', pos: fs + i, vel: 55 + 40 * L * (i + 1) / n });
      return ev;
    }
    if (type === 'perc' && (o.on_perc || o.on_rim)) {
      var role = o.on_perc ? 'perc' : 'rim';
      ev = ev.filter(function (e) { return !(e.role === role && e.pos >= fs); });
      for (i = 0; i < n; i++) if (i % 2 === 0 || rng.chance(0.6)) ev.push({ role: role, pos: fs + i, vel: 60 + 40 * L * (i + 1) / n });
      return ev;
    }
    // snare roll: 8ths, then 16ths
    for (i = 0; i < n; i++) {
      if (i < n / 2 && i % 2 === 1) continue;
      ev.push({ role: 'snare', pos: fs + i, vel: 50 + 55 * L * (0.4 + 0.6 * i / n) });
    }
    return ev;
  }

  // build-up over the last two bars of a "Build" / "Pre" section: accelerating roll, kick out on the very end
  function build(k, spb, L, ev, o) {
    var out = [], step = k === 0 ? 2 : 1, i;
    for (i = 0; i < spb; i += step) out.push({ role: 'snare', pos: i, vel: 45 + 70 * L * ((k * spb + i) / (2 * spb)) });
    if (k === 1) {
      for (i = spb / 2; i < spb; i++) out.push({ role: 'snare', pos: i + 0.5, vel: 60 + 60 * L * (i / spb) });
      for (i = ev.length - 1; i >= 0; i--) if (ev[i].role === 'kick' && ev[i].pos >= spb - 4) ev.splice(i, 1);
    }
    return out;
  }

  function kickOnsets(notes, kickPitch) {
    var out = [];
    for (var i = 0; i < notes.length; i++) if (notes[i].pitch === (kickPitch || MAP.kick) && notes[i].vel > 45) out.push(notes[i].start);
    return out;
  }

  BM.electro = { generate: generate, VOICES: VOICES, MAP: MAP, LIB: LIB, defaults: defaults, kickOnsets: kickOnsets };
})(BM);
