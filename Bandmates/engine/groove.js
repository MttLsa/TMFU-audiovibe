// Bandmates engine - timing & dynamics: swing, push/pull feel, humanize, sections
(function (BM) {
  'use strict';

  function clampVel(v) { return Math.max(1, Math.min(127, Math.round(v))); }
  function clamp01(x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }

  // swing: amount 0..1 maps the off-beat of each pair from 50% to 75% (0.667 = triplet feel).
  // grid = length of one subdivision in beats (0.5 = 8ths, 0.25 = 16ths). Piecewise-linear time warp.
  function swingTime(t, amount, grid) {
    if (!amount || amount <= 0) return t;
    var pair = grid * 2, r = 0.5 + 0.25 * clamp01(amount);
    var base = Math.floor(t / pair + 1e-9) * pair, p = t - base;
    if (p < grid) return base + p * (r * pair / grid);
    return base + r * pair + (p - grid) * ((1 - r) * pair / grid);
  }

  // section names (scene names / arrangement locators) -> performance offsets, like following arrangement markers
  var SECTIONS = [
    { id: 'intro', re: /intro/i, cx: -0.2, ld: -0.15, crash: false, fill: 0.6 },
    { id: 'verse', re: /verse|strofa|vers|couplet|strophe/i, cx: -0.1, ld: -0.05, crash: false, fill: 0.7 },
    { id: 'pre', re: /pre[- ]?(chorus|rit)|build|bridge ?up|ponte ?pre/i, cx: 0.05, ld: 0.05, crash: false, fill: 1 },
    { id: 'chorus', re: /chorus|ritornell|refrain|hook|drop|coro|estribillo/i, cx: 0.1, ld: 0.15, crash: true, fill: 0.9 },
    { id: 'bridge', re: /bridge|ponte|middle ?8|interlud/i, cx: 0.0, ld: 0.0, crash: true, fill: 0.8, varShift: 2 },
    { id: 'solo', re: /solo|instrumental/i, cx: 0.15, ld: 0.1, crash: true, fill: 0.8, varShift: 1 },
    { id: 'break', re: /break|stacco|breakdown/i, cx: -0.35, ld: -0.3, crash: false, fill: 0.5 },
    { id: 'outro', re: /outro|coda|ending|finale|fine/i, cx: -0.05, ld: 0.0, crash: true, fill: 0.4, ending: true }
  ];
  function section(name) {
    if (!name) return null;
    for (var i = 0; i < SECTIONS.length; i++) if (SECTIONS[i].re.test(name)) return SECTIONS[i];
    return null;
  }

  // Applies swing, feel and humanize to notes: {pitch,start,dur,vel,role}.
  // opts: swing 0..1, swingGrid 8|16, feel -1..1 (+ = push/ahead), feelWeights {role: w},
  //       humanize 0..1, rng, length (clip length, notes are kept inside), strum (beats, chords)
  function apply(notes, opts) {
    var out = [], rng = opts.rng, grid = (opts.swingGrid === 8) ? 0.5 : 0.25;
    var maxShift = 0.035; // ~17 ms at 120 BPM
    for (var i = 0; i < notes.length; i++) {
      var n = notes[i], s = n.start, e = n.start + n.dur;
      var straight = Math.abs(s / grid - Math.round(s / grid)) < 1e-6;
      if (straight && !n.noSwing) {
        var ns = swingTime(s, opts.swing || 0, grid);
        var ne = Math.abs(e / grid - Math.round(e / grid)) < 1e-6 ? swingTime(e, opts.swing || 0, grid) : e + (ns - s);
        s = ns; e = Math.max(ns + 0.02, ne);
      }
      var fw = (opts.feelWeights && n.role && opts.feelWeights.hasOwnProperty(n.role)) ? opts.feelWeights[n.role] : 1;
      var shift = -(opts.feel || 0) * maxShift * fw;
      var h = opts.humanize || 0, vel = n.vel;
      if (h > 0 && rng) {
        if (!n.locked) shift += rng.gauss() * h * 0.018 * (n.tight ? 0.4 : 1);
        vel += rng.gauss() * h * 10;
      }
      s += shift; e += shift;
      if (n.strum) { s += n.strum; }
      if (s < 0) { e -= s; s = 0; }
      if (opts.length && s >= opts.length) continue;
      if (opts.length && e > opts.length) e = opts.length;
      var dur = Math.max(0.01, e - s);
      var o = { pitch: n.pitch, start: s, dur: dur, vel: clampVel(vel) };
      if (n.role) o.role = n.role;
      if (n.mute) o.mute = 1;
      out.push(o);
    }
    out.sort(function (a, b) { return a.start - b.start || a.pitch - b.pitch; });
    return out;
  }

  // keep only one note per pitch at (almost) the same time, remove overlaps on the same pitch
  function cleanup(notes) {
    notes.sort(function (a, b) { return a.pitch - b.pitch || a.start - b.start; });
    var out = [];
    for (var i = 0; i < notes.length; i++) {
      var n = notes[i], prev = out.length ? out[out.length - 1] : null;
      if (prev && prev.pitch === n.pitch) {
        if (n.start - prev.start < 0.02) { prev.vel = Math.max(prev.vel, n.vel); prev.dur = Math.max(prev.dur, n.dur); continue; }
        if (prev.start + prev.dur > n.start) prev.dur = Math.max(0.01, n.start - prev.start);
      }
      out.push(n);
    }
    out.sort(function (a, b) { return a.start - b.start || a.pitch - b.pitch; });
    return out;
  }

  // phrase helper: is bar b (0-based) the last bar of a phrase of length len?
  function phraseEnd(b, len) { return ((b + 1) % len) === 0; }

  BM.groove = { clampVel: clampVel, clamp01: clamp01, swingTime: swingTime, section: section, SECTIONS: SECTIONS,
    apply: apply, cleanup: cleanup, phraseEnd: phraseEnd };
})(BM);
