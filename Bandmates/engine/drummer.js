// Bandmates engine - drummer: generates a drum performance (MIDI notes) from performance parameters.
(function (BM) {
  'use strict';
  var G = BM.groove, R = BM.rng;

  // General MIDI drum map (works with Live's Drum Racks laid out from C1 = 36)
  var MAP_GM = {
    kick: 36, sidestick: 37, snare: 38, clap: 39, tomLow: 43, hh: 42, hhPedal: 44, tomMid: 47, hhOpen: 46,
    tomHi: 50, crash: 49, ride: 51, rideBell: 53, tamb: 54, cowbell: 56, congaMute: 62, congaHi: 63,
    congaLow: 64, shaker: 70
  };
  var ROLE_GROUP = {
    kick: 'ks', snare: 'ks', sidestick: 'ks', ghost: 'ks', hh: 'cym', hhOpen: 'cym', hhPedal: 'cym', ride: 'cym',
    rideBell: 'cym', crash: 'cym', tomHi: 'toms', tomMid: 'toms', tomLow: 'toms', clap: 'perc', tamb: 'perc',
    shaker: 'perc', cowbell: 'perc', congaHi: 'perc', congaMute: 'perc', congaLow: 'perc'
  };
  var PERC_TYPES = ['tamb', 'shaker', 'clap', 'cowbell', 'conga'];

  // Pattern library: one char per 16th. X accent, x normal, o soft, . rest. Ordered from simple to complex.
  var LIB = {
    rock: { snare: 'backbeat', ghost: 0.5, open: 0.9, side: false, crashEach: 0.75, fill: 'toms',
      kick: ['X.......X.......', 'X.......X.x.....', 'X......xX.......', 'X.x.....X.x.....', 'X......xX.x.....',
             'X.x....xX.x...x.', 'X.xx...xX.x..x..'],
      hats: ['X...X...X...X...', 'X.x.X.x.X.x.X.x.', 'X.x.X.x.X.x.X.xx', 'XxxxXxxxXxxxXxxx'],
      ride: ['X...X...X...X...', 'X.x.X.x.X.x.X.x.', 'X.xxX.x.X.xxX.x.'] },
    pop: { snare: 'backbeat', ghost: 0.5, open: 0.5, side: true, crashEach: 0.85, fill: 'mixed',
      kick: ['X.......X.......', 'X.......X..x....', 'X..x....X.......', 'X..x..x.X.......', 'X..x....X.x..x..',
             'X.xx..x.X..x..x.'],
      hats: ['X.x.X.x.X.x.X.x.', 'X.xxX.x.X.xxX.x.', 'XxxxXxxxXxxxXxxx', 'Xx.xXxx.Xx.xXxxx'],
      ride: ['X.x.X.x.X.x.X.x.', 'X.xxX.x.X.xxX.x.'] },
    funk: { snare: 'backbeat', ghost: 1.0, open: 0.6, side: false, crashEach: 0.9, fill: 'snare',
      kick: ['X.........X.....', 'X.....x...X.....', 'X..x..x...X.....', 'X..x..x...X..x..', 'X.xx..x..xX...x.',
             'X..x.xx...X.x.x.'],
      hats: ['X.x.X.x.X.x.X.x.', 'XxxxXxxxXxxxXxxx', 'XxXxXxXxXxXxXxXx', 'Xx.xXx.xXx.xXx.x'],
      ride: ['X.x.X.x.X.x.X.x.', 'XxxxXxxxXxxxXxxx'] },
    soul: { snare: 'backbeat', ghost: 0.6, open: 0.4, side: true, crashEach: 0.85, fill: 'snare',
      kick: ['X.......X.......', 'X.......X.x.....', 'X.....x.X.......', 'X..x....X.x.....', 'X..x..x.X..x....', 'X.xx..x.X..x.x..'],
      hats: ['X.x.X.x.X.x.X.x.', 'X.xxX.x.X.xxX.x.', 'XxxxXxxxXxxxXxxx'],
      ride: ['X.x.X.x.X.x.X.x.'] },
    rnb: { snare: 'backbeat', ghost: 0.7, open: 0.35, side: true, crashEach: 2, fill: 'sparse',
      kick: ['X.........X.....', 'X......x..X.....', 'X......x..X..x..', 'X.x....x..X.x...', 'X..x...x.xX....x'],
      hats: ['x.x.x.x.x.x.x.x.', 'X.xxX.x.X.xxX.x.', 'XxxxXxxxXxxxXxxx', 'x.xxx.x.x.xxx.x.'],
      ride: ['x.x.x.x.x.x.x.x.'] },
    alt: { snare: 'backbeat', ghost: 0.35, open: 1.0, side: false, crashEach: 0.6, fill: 'toms', tomGroove: true,
      kick: ['X.......X.......', 'X.....x.X.......', 'X..x....X.x.....', 'X..x..x.X..x....', 'X.x..x..X.x..x..'],
      hats: ['X.x.X.x.X.x.X.x.', 'XxxxXxxxXxxxXxxx'],
      ride: ['X...X...X...X...', 'X.x.X.x.X.x.X.x.', 'X.xxX.x.X.xxX.x.'] },
    jazz: { snare: 'comp', ghost: 0.8, open: 0.2, side: false, crashEach: 2, fill: 'triplets',
      kick: ['o...o...o...o...', '................', 'o.......o.......'],
      hats: ['....x.......x...'],
      ride: ['X...x.xXX...x.xX', 'X...x.x.X...x.x.', 'X...x.xxX..xx.x.'] },
  };

  var PERC = {
    tamb: ['....X.......X...', 'x.x.X.x.x.x.X.x.', 'x.x.X.xxx.x.X.xx', 'xxxxXxxxxxxxXxxx'],
    shaker: ['x.x.x.x.x.x.x.x.', 'XxxxXxxxXxxxXxxx', 'Xx.xXx.xXx.xXx.x', 'XxxXXxxXXxxXXxxX'],
    clap: ['....X.......X...', '....X.......X..x', '....X..x....X.x.', '..x.X..x..x.X.x.'],
    cowbell: ['X...X...X...X...', 'X.x.X.x.X.x.X.x.', 'X..x..x.X.x.x...', 'X.xxX.x.XxxxX.x.'],
    conga: [
      { congaMute: '..x.......x.....', congaHi: '......X.......X.', congaLow: '................' },
      { congaMute: 'x.x...x.x.x...x.', congaHi: '......X.......X.', congaLow: '...............x' },
      { congaMute: 'x.xx..x.x.xx..x.', congaHi: '...x..X.....x.X.', congaLow: '.......x.......x' },
      { congaMute: 'xxx.x.x.xxx.x.x.', congaHi: '...X..X.x..X..Xx', congaLow: '.x.....x.x.....x' }
    ]
  };

  var VAR_OFFSETS = [0, 1, -1, 2, -2];

  function pickIndex(n, cx, variation) {
    var base = Math.round(G.clamp01(cx) * (n - 1));
    var off = VAR_OFFSETS[((variation || 0) % VAR_OFFSETS.length + VAR_OFFSETS.length) % VAR_OFFSETS.length];
    var i = base + off;
    if (i < 0) i = -i;
    if (i > n - 1) i = Math.max(0, 2 * (n - 1) - i);
    return Math.max(0, Math.min(n - 1, i));
  }

  function charAt(p, s) { return p.charAt(s % p.length); }

  function velFor(ch, L) {
    if (ch === 'X') return 70 + 55 * L;
    if (ch === 'x') return 52 + 50 * L;
    if (ch === 'o') return 30 + 30 * L;
    return 0;
  }

  // ------------------------------------------------------------------------------------------
  // opts: see docs/PROGETTO.md (drummer parameters)
  function generate(opts) {
    var o = defaults(opts);
    var fam = LIB[o.family] || LIB.rock;
    var bpb = o.beatsPerBar, spb = Math.round(bpb * 4), nBars = Math.max(1, Math.ceil(o.length / bpb - 1e-6));
    var sec = G.section(o.sectionName);
    var cx = G.clamp01(o.complexity + (o.followSections && sec ? sec.cx : 0));
    var L = G.clamp01(o.loudness + (o.followSections && sec ? sec.ld : 0));
    var varShift = (o.followSections && sec && sec.varShift) ? sec.varShift : 0;
    var crashStart = o.crashStart === null ? (sec ? sec.crash : L > 0.6) : o.crashStart;
    var events = [], crashNext = crashStart && L > 0.3;
    var backSteps = backbeatSteps(fam.snare, spb, bpb, o);

    for (var b = 0; b < nBars; b++) {
      var rng = R.make(o.seed, 'drums', b), ev = [], barStart = b * bpb;
      var barCx = G.clamp01(cx + ((b % 4) === 3 ? 0.08 : 0) + rng.gauss() * 0.05);
      // kick
      var kickPat = timePattern(fam.kick[pickIndex(fam.kick.length, barCx, o.varKick + varShift)], o.timeMode, spb);
      var kickSteps = {};
      if (o.follow && o.follow.length) {
        for (var f = 0; f < o.follow.length; f++) {
          var ft = o.follow[f] - barStart;
          if (ft >= -1e-6 && ft < bpb - 1e-6) kickSteps[Math.min(spb - 1, Math.round(ft * 4))] = (Math.round(ft * 4) % 4 === 0) ? 'X' : 'x';
        }
      } else {
        for (var s = 0; s < spb; s++) if (charAt(kickPat, s) !== '.') kickSteps[s] = charAt(kickPat, s);
        // small per-bar variation on phrase-ending bars
        if ((b % 4) === 3 && rng.chance(0.35 * barCx)) {
          var add = 10 + 2 * rng.int(3);
          if (add < spb && !kickSteps[add]) kickSteps[add] = 'x';
        }
      }
      for (var ks in kickSteps) if (kickSteps.hasOwnProperty(ks)) ev.push({ role: 'kick', pos: +ks, vel: velFor(kickSteps[ks], L) + (+ks === 0 ? 6 : 0) });

      // snare / backbeat
      var backRole = (fam.side && L < 0.3) ? 'sidestick' : 'snare';
      for (var bi = 0; bi < backSteps.length; bi++) {
        ev.push({ role: backRole, pos: backSteps[bi], vel: 78 + 45 * L });
      }
      if (fam.snare === 'comp') { // jazz comping on the snare
        for (var cs = 2; cs < spb; cs += 2) if (rng.chance(0.08 + 0.3 * barCx)) ev.push({ role: 'snare', pos: cs, vel: 35 + 40 * L * rng.next() });
      }

      // ghost notes
      var gAmt = o.ghost * fam.ghost * (0.25 + 0.75 * barCx);
      if (gAmt > 0 && fam.snare !== 'none') {
        for (var gs = 1; gs < spb; gs++) {
          if (gs % 4 === 0 || backSteps.indexOf(gs) >= 0) continue;
          var near = backSteps.indexOf(gs - 1) >= 0 || backSteps.indexOf(gs + 1) >= 0;
          var p = gAmt * (gs % 2 === 1 ? 0.45 : 0.25) * (near ? 1.8 : 1);
          if (rng.chance(p)) ev.push({ role: 'snare', pos: gs, vel: 16 + 18 * L + 6 * rng.next(), ghost: true });
        }
        if ((o.family === 'funk' || o.family === 'rnb') && rng.chance(gAmt * 0.3)) ev.push({ role: 'kick', pos: 7 + 4 * rng.int(2), vel: 40 + 20 * L, ghost: true });
      }

      // cymbals: hi-hat or ride
      var openness = o.hhAuto ? G.clamp01(fam.open * (L - 0.15) * 1.5) : o.hhOpen;
      var useRide = o.cymbal === 'ride' || (fam === LIB.alt && L > 0.7);
      if (useRide) {
        var rp = timePattern(fam.ride[pickIndex(fam.ride.length, barCx, o.varCymbal)], 'normal', spb);
        for (var r = 0; r < spb; r++) {
          var rc = charAt(rp, r);
          if (rc === '.') continue;
          var bell = L > 0.75 && rc === 'X' && o.family !== 'jazz';
          ev.push({ role: bell ? 'rideBell' : 'ride', pos: r, vel: velFor(rc, L) });
        }
        if (o.family === 'jazz' || barCx > 0.3) for (var hp = 0; hp < backSteps.length; hp++) ev.push({ role: 'hhPedal', pos: backSteps[hp], vel: 55 + 30 * L });
      } else {
        var hpat = timePattern(fam.hats[pickIndex(fam.hats.length, barCx, o.varCymbal)], 'normal', spb);
        for (var h = 0; h < spb; h++) {
          var hc = charAt(hpat, h);
          if (hc === '.') continue;
          var role = 'hh';
          if (openness > 0.85) role = 'hhOpen';
          else if (h % 4 === 2 && openness > 0) {
            var pOpen = (h === spb - 2 ? 0.6 : 0.18) * openness * (o.hhAuto ? (0.6 + barCx) : 1.2);
            if (rng.chance(pOpen)) role = 'hhOpen';
          }
          ev.push({ role: role, pos: h, vel: velFor(hc, L) * (role === 'hhOpen' ? 0.95 : 1) });
          if (role === 'hhOpen' && openness <= 0.85 && h + 2 < spb && charAt(hpat, h + 2) === '.') ev.push({ role: 'hhPedal', pos: h + 2, vel: 50 + 25 * L });
        }
      }
      if (fam.tomGroove && o.kitToms && L > 0.55 && barCx > 0.45) {
        for (var tg = 0; tg < spb; tg += 2) if (!kickSteps[tg] || tg % 4 !== 0) ev.push({ role: 'tomLow', pos: tg, vel: (tg % 4 === 0 ? 80 : 60) + 35 * L });
      }

      // percussion
      if (o.kitPerc) {
        if (o.percType === 'conga') {
          var cg = PERC.conga[pickIndex(PERC.conga.length, barCx, o.varPerc)];
          for (var cr in cg) if (cg.hasOwnProperty(cr)) for (var c = 0; c < spb; c++) {
            var cc = charAt(cg[cr], c);
            if (cc !== '.') ev.push({ role: cr, pos: c, vel: velFor(cc, L) });
          }
        } else {
          var plist = PERC[o.percType] || PERC.tamb, pp = plist[pickIndex(plist.length, barCx, o.varPerc)];
          for (var pc = 0; pc < spb; pc++) {
            var pch = charAt(pp, pc);
            if (pch !== '.') ev.push({ role: o.percType, pos: pc, vel: velFor(pch, L) * 0.9 });
          }
        }
      }

      // crash on the downbeat (start of section / after a fill / every phrase when loud)
      var phraseStart = b > 0 && b % (fam.crashEach < 1 ? 4 : 8) === 0 && L > fam.crashEach;
      if (crashNext || phraseStart) {
        ev.push({ role: 'crash', pos: 0, vel: 90 + 35 * L });
        removeAt(ev, 0, ['hh', 'hhOpen', 'ride', 'rideBell']);
        if (!kickSteps[0]) ev.push({ role: 'kick', pos: 0, vel: 95 + 25 * L });
      }
      crashNext = false;

      // fill
      var fill = decideFill(o, b, nBars, rng, sec, barCx);
      if (fill > 0) {
        var fs = Math.max(0, spb - Math.round(fill * 4));
        ev = ev.filter(function (e) { return e.pos < fs - 1e-6 || ROLE_GROUP[e.role] === 'perc' || (e.role === 'kick' && e.pos === fs); });
        var fev = makeFill(fam.fill, fs, spb, L, barCx, o.kitToms, rng);
        for (var fi = 0; fi < fev.length; fi++) ev.push(fev[fi]);
        crashNext = (b < nBars - 1) || o.loopCrash;
        if (b === nBars - 1 && crashStart && L > 0.3) addCrashAtStart(events, L);
      }
      // ending: last bar of an outro ends on a hit
      if (sec && sec.ending && o.followSections && b === nBars - 1) {
        ev = ev.filter(function (e) { return e.pos < spb / 2; });
        ev.push({ role: 'crash', pos: spb / 2, vel: 110 }, { role: 'kick', pos: spb / 2, vel: 110 });
      }

      for (var k = 0; k < ev.length; k++) { ev[k].start = barStart + ev[k].pos / 4; events.push(ev[k]); }
    }

    // kit pieces on/off, then map to notes
    var notes = [];
    for (var i = 0; i < events.length; i++) {
      var e2 = events[i], grp = ROLE_GROUP[e2.role];
      if (grp === 'ks' && !o.kitKickSnare) continue;
      if (grp === 'cym' && !o.kitCymbals) continue;
      if (grp === 'toms' && !o.kitToms) continue;
      if (grp === 'perc' && !o.kitPerc && e2.role !== 'clap') continue;
      if (o.mute && o.mute[e2.role]) continue;
      if (e2.start >= o.length - 1e-9) continue;
      var pitch = (o.map && o.map[e2.role] !== undefined) ? o.map[e2.role] : MAP_GM[e2.role];
      notes.push({ pitch: pitch, start: e2.start, dur: 0.2, vel: G.clampVel(e2.vel), role: e2.role,
                   tight: e2.role === 'kick' || e2.role === 'hh' });
    }
    var hr = R.make(o.seed, 'drums-human');
    notes = G.apply(notes, { swing: o.swing, swingGrid: o.swingGrid, feel: o.feel, humanize: o.humanize, rng: hr,
      length: o.length, feelWeights: { kick: 0.7, snare: 1.25, sidestick: 1.2, clap: 1, hh: 0.5, hhOpen: 0.5, hhPedal: 0.6,
        ride: 0.6, rideBell: 0.6, crash: 0.7 } });
    return G.cleanup(notes);
  }

  function addCrashAtStart(events, L) {
    for (var i = 0; i < events.length; i++) if (events[i].role === 'crash' && events[i].start === 0) return;
    events.push({ role: 'crash', pos: 0, start: 0, vel: 90 + 35 * L });
  }

  function removeAt(ev, pos, roles) {
    for (var i = ev.length - 1; i >= 0; i--) if (ev[i].pos === pos && roles.indexOf(ev[i].role) >= 0 && ev[i].role !== 'crash') ev.splice(i, 1);
  }

  function backbeatSteps(style, spb, bpb, o) {
    if (style === 'none') return [];
    var out = [], beats = Math.round(bpb);
    if (o.compound || Math.abs(bpb - beats) > 1e-6) return [Math.round(spb / 2)];
    if (o.timeMode === 'half') return [beats === 3 ? 8 : Math.round(spb / 2)];
    if (o.timeMode === 'double') { for (var d = 2; d < spb; d += 4) out.push(d); return out; }
    if (beats === 3) return [4, 8];
    if (beats === 1) return [];
    for (var b = 1; b < beats; b += 2) out.push(b * 4);
    if (beats % 2 === 1 && beats > 3) out.push((beats - 1) * 4);
    return out;
  }

  // half / double time variations of a 16-step pattern
  function timePattern(p, mode, spb) {
    var s, out = '';
    if (mode === 'half') { for (s = 0; s < spb; s++) out += (s % 2 === 0) ? charAt(p, s / 2) : '.'; return out; }
    if (mode === 'double') { for (s = 0; s < spb; s++) out += charAt(p, (s * 2) % p.length); return out; }
    return p;
  }

  // returns fill length in beats (0 = no fill)
  function decideFill(o, b, n, rng, sec, cx) {
    var f = G.clamp01(o.fills);
    if (f <= 0) return 0;
    var last = b === n - 1, p;
    if (last && o.endFill) p = 0.25 + 0.75 * f * (sec && o.followSections ? sec.fill : 1);
    else if ((b + 1) % 8 === 0) p = f * 0.9;
    else if ((b + 1) % 4 === 0) p = f * 0.55;
    else if ((b + 1) % 2 === 0) p = f * f * 0.2;
    else p = 0;
    if (!rng.chance(p)) return 0;
    var big = last || (b + 1) % 8 === 0;
    var lens = [0.5, 1, 2, 4], w = [1 - f, 1, f * (0.5 + cx), f * f * (big ? 1.4 : 0.25)];
    if (!big && (b + 1) % 4 !== 0) return 0.5 + 0.5 * rng.int(2);
    return lens[rng.weighted(w)];
  }

  function makeFill(type, fs, spb, L, cx, toms, rng) {
    var ev = [], n = spb - fs, i, base = 70 + 40 * L;
    var drums = toms ? ['snare', 'tomHi', 'tomMid', 'tomLow'] : ['snare', 'snare', 'snare', 'snare'];
    function cresc(i) { return base * (0.75 + 0.35 * (i / Math.max(1, n - 1))); }
    if (type === 'triplets') {
      var trip = Math.round(n / 4 * 3 * (cx > 0.5 ? 2 : 1));
      for (i = 0; i < trip; i++) {
        var pos = fs + i * (n / trip);
        if (rng.chance(0.2 * (1 - cx)) && i > 0 && i < trip - 1) continue;
        ev.push({ role: i >= trip - 3 && toms ? drums[1 + (i % 3)] : 'snare', pos: pos, vel: 50 + 40 * L * (0.6 + 0.4 * i / trip) });
      }
      ev.push({ role: 'kick', pos: fs, vel: 70 + 30 * L });
      return ev;
    }
    if (type === 'sparse') { // hip-hop: a couple of snares / a drop
      ev.push({ role: 'snare', pos: spb - 2, vel: base }, { role: 'snare', pos: spb - 1, vel: base * 0.8 });
      if (n >= 4) ev.push({ role: 'kick', pos: fs, vel: base });
      if (n >= 8 && cx > 0.4) ev.push({ role: 'snare', pos: fs + 3, vel: base * 0.7 }, { role: 'kick', pos: fs + 6, vel: base * 0.9 });
      return ev;
    }
    if (type === 'roll') { // dance build: 8ths then 16ths on clap/snare
      for (i = 0; i < n; i++) {
        if (i < n / 2 && i % 2 === 1) continue;
        ev.push({ role: 'snare', pos: fs + i, vel: cresc(i) });
      }
      for (i = 0; i < n; i += 4) ev.push({ role: 'kick', pos: fs + i, vel: base });
      return ev;
    }
    if (type === 'conga') {
      var cr = ['congaHi', 'congaHi', 'congaMute', 'congaLow'];
      for (i = 0; i < n; i++) if (i % 2 === 0 || rng.chance(cx)) ev.push({ role: cr[Math.floor(i / Math.max(1, n / 4)) % 4], pos: fs + i, vel: cresc(i) });
      return ev;
    }
    // snare / toms / mixed: 16th or 8th runs moving around the kit
    var sixteenths = cx > 0.35 || n <= 4;
    var groups = Math.max(1, Math.round(n / 4));
    for (i = 0; i < n; i++) {
      if (!sixteenths && i % 2 === 1) continue;
      if (i > 0 && i < n - 2 && rng.chance(0.3 * (1 - cx))) continue;
      var g = Math.floor(i / Math.max(1, n / groups)), role;
      if (type === 'snare') role = (i >= n - 4 && toms && cx > 0.5) ? drums[1 + Math.floor((i - (n - 4)) / 2) % 3] : 'snare';
      else if (type === 'mixed') role = (i % 4 < 2) ? 'snare' : drums[1 + (g % 3)];
      else role = groups === 1 ? drums[Math.min(3, Math.floor(i / Math.max(1, n / 4)))] : drums[Math.min(3, g * 4 / groups | 0)];
      ev.push({ role: role, pos: fs + i, vel: cresc(i) });
    }
    for (i = 0; i < n; i += 4) ev.push({ role: 'kick', pos: fs + i, vel: base * 0.9 });
    return ev;
  }

  function defaults(opts) {
    var persona = BM.styles.find(BM.styles.DRUMMERS, opts.persona || 'marco');
    var d = persona.def, o = {};
    var base = {
      length: 16, beatsPerBar: 4, seed: 1, family: persona.family, complexity: 0.5, loudness: 0.5, fills: 0.4, swing: 0,
      swingGrid: 16, feel: 0, humanize: 0.35, ghost: 0.3, hhAuto: true, hhOpen: 0.3, cymbal: 'hat',
      kitKickSnare: true, kitCymbals: true, kitToms: true, kitPerc: false, percType: 'tamb', varKick: 0, varCymbal: 0,
      varPerc: 0, timeMode: 'normal', follow: null, sectionName: '', followSections: true, crashStart: null,
      endFill: true, loopCrash: false, map: null, mute: null, compound: false
    };
    var k;
    for (k in base) if (base.hasOwnProperty(k)) o[k] = base[k];
    for (k in d) if (d.hasOwnProperty(k)) o[k] = d[k];
    for (k in opts) if (opts.hasOwnProperty(k) && opts[k] !== undefined) o[k] = opts[k];
    o.family = persona.family;
    if (opts.family) o.family = opts.family;
    return o;
  }

  // positions (beats) of the kick drum in a generated performance, for the bass player's "follow kick"
  function kickOnsets(notes, kickPitch) {
    var out = [];
    for (var i = 0; i < notes.length; i++) if (notes[i].pitch === (kickPitch || MAP_GM.kick) && notes[i].vel > 45) out.push(notes[i].start);
    return out;
  }

  BM.drummer = { generate: generate, MAP_GM: MAP_GM, LIB: LIB, PERC_TYPES: PERC_TYPES, kickOnsets: kickOnsets,
    defaults: defaults };
})(BM);
