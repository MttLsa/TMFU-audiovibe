// Bandmates engine - chord voicings with voice leading
(function (BM) {
  'use strict';
  var T = BM.theory;

  function sum(a) { var s = 0; for (var i = 0; i < a.length; i++) s += a[i]; return s; }

  // distance between two voicings (sorted pitch arrays), tolerant of different sizes
  function distance(a, b) {
    if (!a || !b || !a.length || !b.length) return 0;
    var d = 0, i, j;
    for (i = 0; i < a.length; i++) {
      var best = 999;
      for (j = 0; j < b.length; j++) best = Math.min(best, Math.abs(a[i] - b[j]));
      d += best;
    }
    d += Math.abs(a[a.length - 1] - b[b.length - 1]) * 0.8; // keep the top voice smooth
    return d;
  }

  // choose `count` pitch classes from the (possibly extended) chord: guide tones first
  function pickPcs(ch, count, ext) {
    var iv = T.extendIntervals(ch, ext || 0), root = ch.root, out = [];
    var third = T.thirdOf(ch), fifth = T.fifthOf(ch), sev = T.seventhOf(ch);
    var order = [];
    if (iv.indexOf(third) >= 0) order.push(third);
    var ext7 = null, k;
    for (k = 0; k < iv.length; k++) if (iv[k] === 10 || iv[k] === 11 || (iv[k] === 9 && ch.q === 'dim7')) ext7 = iv[k];
    if (ext7 === null && sev !== null) ext7 = sev;
    if (ext7 !== null) order.push(ext7);
    for (k = 0; k < iv.length; k++) if (iv[k] > 12) order.push(iv[k] - 12);
    for (k = 0; k < iv.length; k++) if (iv[k] === 9 && order.indexOf(9) < 0) order.push(9);
    if (order.indexOf(fifth) < 0) order.push(fifth);
    order.push(0);
    for (k = 0; k < iv.length; k++) { var x = iv[k] % 12; if (order.indexOf(x) < 0) order.push(x); }
    // with few voices drop the root when colour notes exist, otherwise keep root + fifth for triads
    var chosen = [];
    for (k = 0; k < order.length && chosen.length < count; k++) {
      var pc = T.mod12(root + order[k]);
      if (chosen.indexOf(pc) < 0) chosen.push(pc);
    }
    if (count >= 3 && iv.length <= 3) chosen = T.chordPcs(ch).slice(0, count);
    while (chosen.length < count) chosen.push(chosen[(chosen.length) % Math.max(1, chosen.length)] );
    for (k = 0; k < chosen.length; k++) out.push(chosen[k]);
    return out;
  }

  // all close-position inversions of the pcs whose centre lies near `center`
  function closeCandidates(pcs, center) {
    var cands = [], n = pcs.length, i, k;
    var uniq = [];
    for (i = 0; i < n; i++) if (uniq.indexOf(pcs[i]) < 0) uniq.push(pcs[i]);
    for (var rot = 0; rot < uniq.length; rot++) {
      for (var base = center - 14; base <= center + 2; base++) {
        if (T.mod12(base) !== uniq[rot]) continue;
        var v = [base], cur = base;
        for (k = 1; k < uniq.length; k++) {
          var pc = uniq[(rot + k) % uniq.length], p = cur + 1;
          while (T.mod12(p) !== pc) p++;
          v.push(p); cur = p;
        }
        cands.push(v);
      }
    }
    return cands;
  }

  function choose(cands, center, prev) {
    var best = null, bestCost = 1e9;
    for (var i = 0; i < cands.length; i++) {
      var v = cands[i], mid = sum(v) / v.length;
      var cost = Math.abs(mid - center) * (prev ? 0.35 : 1) + (prev ? distance(v, prev) : 0);
      // avoid muddy low clusters
      if (v[0] < 52 && v.length > 1 && v[1] - v[0] < 3) cost += 6;
      if (cost < bestCost) { bestCost = cost; best = v; }
    }
    return best ? best.slice() : [];
  }

  function close(ch, count, center, prev, ext) {
    return choose(closeCandidates(pickPcs(ch, count, ext), center), center, prev);
  }

  function drop2(ch, center, prev, ext) {
    var cands = closeCandidates(pickPcs(ch, 4, ext), center + 4), out = [];
    for (var i = 0; i < cands.length; i++) {
      var v = cands[i].slice();
      if (v.length >= 4) { var x = v.splice(v.length - 2, 1)[0] - 12; v.unshift(x); }
      out.push(v);
    }
    return choose(out, center, prev);
  }

  function open(ch, center, prev, ext) { // spread triad: root-fifth-third(+octave)
    var pcs = pickPcs(ch, 4, ext), cands = closeCandidates(pcs, center), out = [];
    for (var i = 0; i < cands.length; i++) {
      var v = cands[i].slice();
      if (v.length >= 3) { v[1] += 12; v.sort(function (a, b) { return a - b; }); }
      out.push(v);
    }
    return choose(out, center, prev);
  }

  function shell(ch, center, prev, ext) { // 3rd + 7th (+ one colour note)
    var t3 = T.mod12(ch.root + T.thirdOf(ch)), s = T.seventhOf(ch);
    var t7 = T.mod12(ch.root + (s === null ? (T.isMinor(ch) ? 10 : (ch.q === 'maj' ? 11 : 9)) : s));
    var pcs = [t3, t7];
    if (ext > 0.4) pcs.push(T.mod12(ch.root + (ch.q === 'dim' || ch.q === 'm7b5' ? 5 : 14)));
    return choose(closeCandidates(pcs, center), center, prev);
  }

  function quartal(ch, center, prev) { // stacked fourths from the third/ninth (modern "So What"-style)
    var start = T.mod12(ch.root + (T.isMinor(ch) ? 3 : 2)), out = [];
    for (var b = center - 12; b <= center; b++) {
      if (T.mod12(b) !== start) continue;
      out.push([b, b + 5, b + 10, b + 15]);
      out.push([b + 5, b + 10, b + 15]);
    }
    if (!out.length) return close(ch, 3, center, prev, 0);
    return choose(out, center, prev);
  }

  function octaveTop(ch, center, prev, ext) { // close triad with the top note doubled an octave below
    var v = close(ch, 3, center, prev, ext);
    if (v.length) v.unshift(v[v.length - 1] - 12);
    return v;
  }

  function voice(type, ch, center, prev, ext) {
    switch (type) {
      case 'shell': return shell(ch, center, prev, ext);
      case 'drop2': return drop2(ch, center, prev, ext);
      case 'open': return open(ch, center, prev, ext);
      case 'quartal': return quartal(ch, center, prev);
      case 'octave': return octaveTop(ch, center, prev, ext);
      case 'rich': return close(ch, 5, center, prev, Math.max(ext, 0.6));
      case 'triad': return close(ch, 3, center, prev, 0);
      default: return close(ch, 4, center, prev, ext);
    }
  }

  BM.voicing = { distance: distance, pickPcs: pickPcs, close: close, drop2: drop2, open: open, shell: shell,
    quartal: quartal, voice: voice, TYPES: ['triad', 'close', 'open', 'drop2', 'shell', 'quartal', 'octave', 'rich'] };
})(BM);
