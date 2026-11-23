// Bandmates engine - music theory: note names, chord symbols, chord recognition, scales, roman numerals
(function (BM) {
  'use strict';

  var SHARP = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
  var FLAT = ['C', 'Db', 'D', 'Eb', 'E', 'F', 'Gb', 'G', 'Ab', 'A', 'Bb', 'B'];
  var LETTER = { C: 0, D: 2, E: 4, F: 5, G: 7, A: 9, B: 11 };

  function mod12(n) { return ((n % 12) + 12) % 12; }

  // parse a note name at the start of str: returns {pc, len} or null
  function parseNoteName(str) {
    if (!str) return null;
    var L = str.charAt(0).toUpperCase();
    if (!LETTER.hasOwnProperty(L)) return null;
    var pc = LETTER[L], i = 1;
    while (i < str.length) {
      var c = str.charAt(i);
      if (c === '#' || c === '♯') pc++;
      else if (c === 'b' || c === '♭') pc--;
      else break;
      i++;
    }
    return { pc: mod12(pc), len: i };
  }

  function noteName(pc, flats) { return (flats ? FLAT : SHARP)[mod12(pc)]; }

  // Live shows middle C (60) as C3
  function pitchName(p, flats) { return noteName(p, flats) + (Math.floor(p / 12) - 2); }

  // ---- chord qualities -------------------------------------------------------------------
  // id, display suffix, aliases (exact match first, then case-insensitive except M/m), intervals
  var QUALITIES = [
    ['maj', '', ['', 'maj', 'M', 'major', 'Maj'], [0, 4, 7]],
    ['min', 'm', ['m', 'min', '-', 'mi', 'minor'], [0, 3, 7]],
    ['dim', 'dim', ['dim', '°', 'o'], [0, 3, 6]],
    ['aug', 'aug', ['aug', '+'], [0, 4, 8]],
    ['sus2', 'sus2', ['sus2', '2'], [0, 2, 7]],
    ['sus4', 'sus4', ['sus4', 'sus', '4'], [0, 5, 7]],
    ['5', '5', ['5', 'no3'], [0, 7]],
    ['6', '6', ['6', 'maj6', 'M6'], [0, 4, 7, 9]],
    ['m6', 'm6', ['m6', 'min6', '-6'], [0, 3, 7, 9]],
    ['69', '6/9', ['6/9', '69', '6add9'], [0, 4, 7, 9, 14]],
    ['7', '7', ['7', 'dom7', 'dom'], [0, 4, 7, 10]],
    ['maj7', 'maj7', ['maj7', 'M7', 'Δ7', 'Δ', 'ma7', 'Maj7', 'j7'], [0, 4, 7, 11]],
    ['m7', 'm7', ['m7', 'min7', '-7', 'mi7'], [0, 3, 7, 10]],
    ['mMaj7', 'm(maj7)', ['mMaj7', 'm(maj7)', 'mM7', '-Δ7', 'mmaj7', 'minmaj7'], [0, 3, 7, 11]],
    ['m7b5', 'm7b5', ['m7b5', 'ø', 'ø7', '-7b5', 'min7b5', 'half-dim'], [0, 3, 6, 10]],
    ['dim7', 'dim7', ['dim7', '°7', 'o7'], [0, 3, 6, 9]],
    ['7sus4', '7sus4', ['7sus4', '7sus', '7sus2'], [0, 5, 7, 10]],
    ['aug7', '7#5', ['7#5', 'aug7', '+7', '7+5'], [0, 4, 8, 10]],
    ['7b5', '7b5', ['7b5', '7-5'], [0, 4, 6, 10]],
    ['add9', 'add9', ['add9', 'add2', '(add9)'], [0, 4, 7, 14]],
    ['madd9', 'madd9', ['madd9', 'm(add9)', 'madd2', '-add9'], [0, 3, 7, 14]],
    ['9', '9', ['9', 'dom9'], [0, 4, 7, 10, 14]],
    ['maj9', 'maj9', ['maj9', 'M9', 'Δ9', 'ma9', 'Maj9'], [0, 4, 7, 11, 14]],
    ['m9', 'm9', ['m9', 'min9', '-9', 'mi9'], [0, 3, 7, 10, 14]],
    ['7b9', '7b9', ['7b9', '7-9'], [0, 4, 7, 10, 13]],
    ['7#9', '7#9', ['7#9', '7+9'], [0, 4, 7, 10, 15]],
    ['7#11', '7#11', ['7#11', '7+11'], [0, 4, 7, 10, 18]],
    ['maj7#11', 'maj7#11', ['maj7#11', 'M7#11', 'Δ7#11'], [0, 4, 7, 11, 18]],
    ['11', '11', ['11', 'dom11'], [0, 7, 10, 14, 17]],
    ['m11', 'm11', ['m11', 'min11', '-11'], [0, 3, 7, 10, 14, 17]],
    ['13', '13', ['13', 'dom13'], [0, 4, 7, 10, 14, 21]],
    ['m13', 'm13', ['m13', 'min13', '-13'], [0, 3, 7, 10, 14, 21]],
    ['maj13', 'maj13', ['maj13', 'M13', 'Δ13'], [0, 4, 7, 11, 14, 21]]
  ];
  var Q = {}, ALIAS = {}, ALIAS_LC = {};
  (function () {
    for (var i = 0; i < QUALITIES.length; i++) {
      var q = QUALITIES[i];
      Q[q[0]] = { id: q[0], suffix: q[1], iv: q[3] };
      for (var a = 0; a < q[2].length; a++) {
        ALIAS[q[2][a]] = q[0];
        var lc = q[2][a].toLowerCase();
        if (q[2][a] !== 'M' && q[2][a] !== 'M7' && q[2][a] !== 'M9' && q[2][a] !== 'M6' &&
            q[2][a] !== 'M13' && q[2][a] !== 'M7#11' && !ALIAS_LC.hasOwnProperty(lc)) ALIAS_LC[lc] = q[0];
      }
    }
  })();

  function makeChord(root, qid, bass) {
    var q = Q[qid];
    if (!q) return null;
    var ch = { root: mod12(root), q: qid, iv: q.iv.slice(), bass: (bass === null || bass === undefined) ? null : mod12(bass) };
    if (ch.bass === ch.root) ch.bass = null;
    return ch;
  }

  function lookupQuality(s) {
    if (ALIAS.hasOwnProperty(s)) return ALIAS[s];
    var stripped = s.replace(/[()\s]/g, '');
    if (ALIAS.hasOwnProperty(stripped)) return ALIAS[stripped];
    var lc = stripped.toLowerCase();
    if (ALIAS_LC.hasOwnProperty(lc)) return ALIAS_LC[lc];
    return null;
  }

  // "F#m7b5/C" -> {root:6, q:'m7b5', iv:[...], bass:0}; "N.C." -> {nc:true}; invalid -> null
  function parseChord(sym) {
    if (sym === null || sym === undefined) return null;
    var s = String(sym).replace(/^\s+|\s+$/g, '');
    if (!s) return null;
    if (/^(n\.?c\.?|x)$/i.test(s)) return { nc: true };
    var r = parseNoteName(s);
    if (!r) return null;
    var rest = s.substring(r.len), bass = null;
    if (rest.indexOf('6/9') !== 0) {
      var slash = rest.lastIndexOf('/');
      if (slash >= 0) {
        var b = parseNoteName(rest.substring(slash + 1));
        if (b && b.len === rest.length - slash - 1) {
          bass = b.pc;
          rest = rest.substring(0, slash);
        }
      }
    } else if (rest.length > 3 && rest.charAt(3) === '/') {
      var b2 = parseNoteName(rest.substring(4));
      if (!b2) return null;
      bass = b2.pc;
      rest = '6/9';
    }
    var qid = lookupQuality(rest);
    if (qid === null) return null;
    return makeChord(r.pc, qid, bass);
  }

  function chordName(ch, flats) {
    if (!ch) return '';
    if (ch.nc) return 'N.C.';
    var s = noteName(ch.root, flats) + Q[ch.q].suffix;
    if (ch.bass !== null && ch.bass !== undefined) s += '/' + noteName(ch.bass, flats);
    return s;
  }

  function chordPcs(ch) {
    var out = [], seen = {};
    if (!ch || ch.nc) return out;
    for (var i = 0; i < ch.iv.length; i++) {
      var pc = mod12(ch.root + ch.iv[i]);
      if (!seen[pc]) { seen[pc] = 1; out.push(pc); }
    }
    return out;
  }

  function bassPc(ch) { return (ch.bass !== null && ch.bass !== undefined) ? ch.bass : ch.root; }

  function isMinor(ch) { return ch.iv.indexOf(3) >= 0 && ch.iv.indexOf(4) < 0; }

  // interval (semitones above root, < 12) of the chord's third / fifth / seventh
  function thirdOf(ch) {
    if (ch.iv.indexOf(4) >= 0) return 4;
    if (ch.iv.indexOf(3) >= 0) return 3;
    if (ch.iv.indexOf(5) >= 0) return 5;
    if (ch.iv.indexOf(2) >= 0) return 2;
    return 7;
  }
  function fifthOf(ch) {
    if (ch.iv.indexOf(7) >= 0) return 7;
    if (ch.iv.indexOf(6) >= 0) return 6;
    if (ch.iv.indexOf(8) >= 0) return 8;
    return 7;
  }
  function seventhOf(ch) {
    if (ch.iv.indexOf(10) >= 0) return 10;
    if (ch.iv.indexOf(11) >= 0) return 11;
    if (ch.q === 'dim7') return 9;
    return null;
  }

  // add 7th / 9th / 13th colour notes depending on amount (0..1); returns new interval list
  function extendIntervals(ch, amount) {
    var iv = ch.iv.slice();
    function has(x) { return iv.indexOf(x) >= 0 || iv.indexOf(x + 12) >= 0; }
    if (amount <= 0 || ch.q === '5') return iv;
    var minor = isMinor(ch), dimish = ch.q === 'dim' || ch.q === 'm7b5' || ch.q === 'dim7', sus = ch.q.indexOf('sus') >= 0;
    if (amount > 0.3 && seventhOf(ch) === null && ch.q !== '6' && ch.q !== 'm6' && ch.q !== 'aug' && ch.q !== '69') {
      iv.push(dimish ? (ch.q === 'dim' ? 9 : 10) : (minor || sus ? 10 : 11));
    }
    if (amount > 0.55 && !has(14) && !has(13) && !has(15) && !dimish) iv.push(14);
    if (amount > 0.85 && !dimish && !has(21) && !has(17) && !minor && ch.q !== 'aug') iv.push(21);
    if (amount > 0.85 && minor && !has(17)) iv.push(17);
    return iv;
  }

  // ---- chord recognition ("Chord ID") --------------------------------------------------
  var DETECT = ['maj', 'min', 'dim', 'aug', 'sus2', 'sus4', '5', '6', 'm6', '7', 'maj7', 'm7', 'mMaj7',
                'm7b5', 'dim7', '7sus4', 'add9', 'madd9', '9', 'maj9', 'm9'];
  var COMPLEXITY_PENALTY = { maj: 0, min: 0, '5': 0.25, dim: 0.12, aug: 0.15, sus2: 0.1, sus4: 0.08, '7': 0.02,
    maj7: 0.03, m7: 0.03, '6': 0.06, m6: 0.07, mMaj7: 0.12, m7b5: 0.05, dim7: 0.08, '7sus4': 0.08,
    add9: 0.06, madd9: 0.07, '9': 0.08, maj9: 0.09, m9: 0.09 };

  // notes: array of {pitch, w} (w = weight, e.g. duration). Returns a chord or null.
  function detectChord(notes) {
    if (!notes || !notes.length) return null;
    var w = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0], tot = 0, low = 999, i;
    for (i = 0; i < notes.length; i++) {
      var ww = notes[i].w === undefined ? 1 : notes[i].w;
      w[mod12(notes[i].pitch)] += ww; tot += ww;
      if (notes[i].pitch < low) low = notes[i].pitch;
    }
    if (tot <= 0) return null;
    for (i = 0; i < 12; i++) w[i] /= tot;
    var bass = mod12(low), present = 0;
    for (i = 0; i < 12; i++) if (w[i] > 0.02) present++;
    if (present === 1) { var single = makeChord(bass, 'maj', null); single.guessed = true; return single; }
    var best = null, bestScore = -1e9;
    for (var root = 0; root < 12; root++) {
      if (w[root] <= 0) continue;
      for (var d = 0; d < DETECT.length; d++) {
        var q = Q[DETECT[d]], inT = {}, score = 0, missing = 0;
        for (var k = 0; k < q.iv.length; k++) inT[mod12(root + q.iv[k])] = 1;
        for (var pc = 0; pc < 12; pc++) {
          if (inT[pc]) { if (w[pc] > 0.02) score += w[pc]; else missing++; }
          else score -= w[pc] * 1.4;
        }
        score -= missing * 0.22;
        score -= COMPLEXITY_PENALTY[q.id] || 0;
        if (root === bass) score += 0.12;
        score += w[root] * 0.2;
        if (score > bestScore) { bestScore = score; best = makeChord(root, q.id, bass); }
      }
    }
    return best;
  }

  // ---- scales & keys ---------------------------------------------------------------------
  var SCALES = {
    major: [0, 2, 4, 5, 7, 9, 11], minor: [0, 2, 3, 5, 7, 8, 10], dorian: [0, 2, 3, 5, 7, 9, 10],
    phrygian: [0, 1, 3, 5, 7, 8, 10], lydian: [0, 2, 4, 6, 7, 9, 11], mixolydian: [0, 2, 4, 5, 7, 9, 10],
    locrian: [0, 1, 3, 5, 6, 8, 10], harmonicminor: [0, 2, 3, 5, 7, 8, 11], melodicminor: [0, 2, 3, 5, 7, 9, 11],
    majorpentatonic: [0, 2, 4, 7, 9], minorpentatonic: [0, 3, 5, 7, 10], blues: [0, 3, 5, 6, 7, 10]
  };
  function scaleSteps(name) {
    var k = String(name || 'major').toLowerCase().replace(/[^a-z]/g, '');
    if (k === 'ionian') k = 'major';
    if (k === 'aeolian' || k === 'naturalminor') k = 'minor';
    return SCALES[k] || SCALES.major;
  }

  // scale that fits a chord (for passing tones / fills)
  function chordScale(ch) {
    if (ch.q === 'dim' || ch.q === 'm7b5') return [0, 1, 3, 5, 6, 8, 10];
    if (ch.q === 'dim7') return [0, 2, 3, 5, 6, 8, 9, 11];
    if (ch.q === 'aug' || ch.q === 'aug7') return [0, 2, 4, 6, 8, 10];
    if (isMinor(ch)) return ch.q === 'mMaj7' ? SCALES.melodicminor : (ch.q === 'm6' ? SCALES.dorian : SCALES.dorian);
    if (seventhOf(ch) === 10 || ch.q.indexOf('sus') >= 0 || ch.q === '9' || ch.q === '13' || ch.q === '11') return SCALES.mixolydian;
    if (ch.q === 'maj7#11') return SCALES.lydian;
    return SCALES.major;
  }

  // ---- roman numerals ("I V vi IV", "ii7 V7 Imaj7", "i bVI bIII bVII") -------------------
  var ROMAN = { I: 0, II: 1, III: 2, IV: 3, V: 4, VI: 5, VII: 6 };
  function parseRoman(tok, keyRoot) {
    var m = /^([b#♭♯]*)(VII|VI|V|IV|III|II|I|vii|vi|v|iv|iii|ii|i)(.*)$/.exec(String(tok).replace(/\s/g, ''));
    if (!m) return null;
    var acc = 0, i;
    for (i = 0; i < m[1].length; i++) acc += (m[1].charAt(i) === '#' || m[1].charAt(i) === '♯') ? 1 : -1;
    var upper = m[2] === m[2].toUpperCase();
    var deg = ROMAN[m[2].toUpperCase()];
    var rest = m[3], bass = null, slash = rest.indexOf('/');
    if (slash >= 0) {
      var bt = rest.substring(slash + 1);
      rest = rest.substring(0, slash);
      var bdeg = /^([b#]*)([1-7])$/.exec(bt);
      if (bdeg) {
        var bacc = 0;
        for (i = 0; i < bdeg[1].length; i++) bacc += bdeg[1].charAt(i) === '#' ? 1 : -1;
        bass = keyRoot + SCALES.major[parseInt(bdeg[2], 10) - 1] + bacc;
      }
    }
    var root = keyRoot + SCALES.major[deg] + acc, qid;
    if (rest === '°' || rest === 'o' || rest === 'dim') qid = 'dim';
    else if (rest === '°7' || rest === 'o7' || rest === 'dim7') qid = 'dim7';
    else if (rest === 'ø' || rest === 'ø7' || rest === 'm7b5') qid = 'm7b5';
    else if (rest === '+') qid = 'aug';
    else if (rest === '') qid = upper ? 'maj' : 'min';
    else if (rest === '7') qid = upper ? '7' : 'm7';
    else if (rest === '9') qid = upper ? '9' : 'm9';
    else if (rest === '6') qid = upper ? '6' : 'm6';
    else if (rest === '11') qid = upper ? '11' : 'm11';
    else if (rest === 'maj7' || rest === 'M7' || rest === 'Δ7') qid = upper ? 'maj7' : 'mMaj7';
    else if (rest === 'maj9' || rest === 'M9') qid = upper ? 'maj9' : 'm9';
    else if (rest === 'add9') qid = upper ? 'add9' : 'madd9';
    else { qid = lookupQuality(rest); if (qid === null) return null; }
    return makeChord(root, qid, bass);
  }

  // diatonic triads / sevenths of a key, for building progressions from the song's scale
  function diatonicChord(keyRoot, scaleName, degree, sevenths) {
    var st = scaleSteps(scaleName);
    if (st.length !== 7) st = SCALES.major;
    function at(n) { return st[n % 7] + 12 * Math.floor(n / 7); }
    var r = at(degree), third = at(degree + 2) - r, fifth = at(degree + 4) - r, sev = at(degree + 6) - r, qid;
    if (third === 4 && fifth === 7) qid = sevenths ? (sev === 11 ? 'maj7' : '7') : 'maj';
    else if (third === 3 && fifth === 7) qid = sevenths ? (sev === 10 ? 'm7' : 'mMaj7') : 'min';
    else if (third === 3 && fifth === 6) qid = sevenths ? (sev === 10 ? 'm7b5' : 'dim7') : 'dim';
    else if (third === 4 && fifth === 8) qid = 'aug';
    else qid = 'maj';
    return makeChord(keyRoot + r, qid, null);
  }

  BM.theory = {
    SHARP: SHARP, FLAT: FLAT, SCALES: SCALES, QUALITY_IDS: QUALITIES.map(function (q) { return q[0]; }),
    mod12: mod12, parseNoteName: parseNoteName, noteName: noteName, pitchName: pitchName,
    makeChord: makeChord, parseChord: parseChord, chordName: chordName, chordPcs: chordPcs, bassPc: bassPc,
    isMinor: isMinor, thirdOf: thirdOf, fifthOf: fifthOf, seventhOf: seventhOf, extendIntervals: extendIntervals,
    detectChord: detectChord, scaleSteps: scaleSteps, chordScale: chordScale, parseRoman: parseRoman,
    diatonicChord: diatonicChord, qualitySuffix: function (qid) { return Q[qid] ? Q[qid].suffix : ''; }
  };
})(BM);
