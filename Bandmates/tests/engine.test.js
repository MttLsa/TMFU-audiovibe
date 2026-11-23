'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const BM = require('../engine/load.js').load();
const { theory: T, chords: C, groove: G, drummer: D, bass: B, keys: K } = BM;
// engine objects come from another VM realm: compare plain copies
const plain = (x) => JSON.parse(JSON.stringify(x));
const deq = (a, b, m) => assert.deepStrictEqual(plain(a), plain(b), m);

const PROG_TEXT = 'C G/B | Am7 - | Fmaj7 | G7sus4 G7 | Dm7 | G7 | Cmaj7 | %';
const prog = C.parseText(PROG_TEXT, 4).prog;

function valid(notes, length) {
  for (const n of notes) {
    assert.ok(Number.isInteger(n.pitch) && n.pitch >= 0 && n.pitch <= 127, 'pitch ' + n.pitch);
    assert.ok(n.vel >= 1 && n.vel <= 127, 'vel ' + n.vel);
    assert.ok(n.start >= 0 && n.start < length, 'start ' + n.start);
    assert.ok(n.dur > 0 && n.start + n.dur <= length + 1e-9, 'end ' + (n.start + n.dur));
    assert.ok(Number.isFinite(n.start) && Number.isFinite(n.dur));
  }
}

test('chord symbols parse and print', () => {
  const cases = { 'C': 'C', 'Am7': 'Am7', 'F#m7b5': 'F#m7b5', 'Bbmaj7': 'A#maj7', 'G/B': 'G/B', 'D7sus4': 'D7sus4',
    'Ebm(maj7)': 'D#m(maj7)', 'C6/9': 'C6/9', 'Cø': 'Cm7b5', 'C°7': 'Cdim7', 'E13': 'E13', 'AbM7': 'G#maj7', 'Dm/C': 'Dm/C' };
  for (const [inp, out] of Object.entries(cases)) assert.equal(T.chordName(T.parseChord(inp)), out, inp);
  assert.equal(T.chordName(T.parseChord('Bbmaj7'), true), 'Bbmaj7');
  assert.equal(T.parseChord('H7'), null);
  assert.ok(T.parseChord('N.C.').nc);
});

test('roman numerals', () => {
  const names = 'I V vi IV ii7 V7 Imaj7 bVII viiø iii/5'.split(' ').map((r) => T.chordName(T.parseRoman(r, 2)));
  deq(plain(names), ['D', 'A', 'Bm', 'G', 'Em7', 'A7', 'Dmaj7', 'C', 'C#m7b5', 'F#m/A']);
  assert.equal(T.chordName(T.diatonicChord(9, 'minor', 4, true)), 'Em7');
});

test('chord recognition (Chord ID)', () => {
  const cases = [[[60, 64, 67], 'C'], [[57, 60, 64, 67], 'Am7'], [[55, 59, 62, 65], 'G7'], [[59, 62, 67], 'G/B'],
    [[48, 64, 67, 71], 'Cmaj7'], [[50, 65, 69, 72], 'Dm7'], [[62, 65, 68, 71], 'Ddim7'], [[60, 65, 67], 'Csus4'],
    [[45, 60, 64, 67, 71], 'Cmaj7/A']];
  for (const [ps, name] of cases) {
    const got = T.chordName(T.detectChord(ps.map((p) => ({ pitch: p, w: 1 }))));
    if (name === 'Cmaj7/A') assert.ok(['Cmaj7/A', 'Am9'].includes(got), got); else assert.equal(got, name, ps.join(','));
  }
});

test('progression text round trip and chord clip -> Chord ID', () => {
  const pr = C.parseText(PROG_TEXT, 4);
  deq(plain(pr.errors), []);
  assert.equal(pr.length, 32);
  const again = C.parseText(C.toText(pr.prog, 4), 4).prog;
  deq(again.map((e) => [e.start, e.dur, T.chordName(e.chord)]), pr.prog.map((e) => [e.start, e.dur, T.chordName(e.chord)]));
  const notes = C.toNotes(pr.prog);
  const id = C.fromNotes(notes, 32, { resolution: 2 });
  deq(id.map((e) => T.chordName(e.chord)), pr.prog.map((e) => T.chordName(e.chord)));
});

test('presets build in any key', () => {
  for (const p of C.PRESETS) {
    const pr = C.presetProgression(p.id, 7, 4);
    assert.ok(pr.length >= 2, p.id);
    for (const e of pr) assert.ok(e.chord && !e.chord.nc, p.id);
  }
});

test('swing warps off-beats only', () => {
  assert.equal(G.swingTime(0, 1, 0.5), 0);
  assert.equal(G.swingTime(1, 1, 0.5), 1);
  assert.equal(G.swingTime(0.5, 1, 0.5), 0.75);
  assert.ok(Math.abs(G.swingTime(0.25, 0.667, 0.25) - 0.3335) < 1e-3);
});

test('drummer: valid, deterministic, parameters do what they say', () => {
  for (const p of BM.styles.DRUMMERS) {
    const n = D.generate({ persona: p.id, length: 32, seed: 7 });
    assert.ok(n.length > 0, p.id);
    valid(n, 32);
    deq(plain(D.generate({ persona: p.id, length: 32, seed: 7 })), plain(n));
  }
  const base = { persona: 'marco', length: 64, seed: 11, humanize: 0, swing: 0, feel: 0, fills: 0 };
  const simple = D.generate({ ...base, complexity: 0 }), complex = D.generate({ ...base, complexity: 1 });
  assert.ok(complex.length > simple.length);
  const avg = (ns) => ns.reduce((a, n) => a + n.vel, 0) / ns.length;
  assert.ok(avg(D.generate({ ...base, loudness: 1 })) > avg(D.generate({ ...base, loudness: 0 })) + 20);
  // kit pieces
  const noCym = D.generate({ ...base, kitCymbals: false });
  assert.ok(!noCym.some((n) => [42, 46, 44, 51, 49].includes(n.pitch)));
  const noKs = D.generate({ ...base, kitKickSnare: false });
  assert.ok(!noKs.some((n) => [36, 38, 37].includes(n.pitch)));
  // fills add tom hits
  const fills = D.generate({ ...base, fills: 1, complexity: 0.8 });
  assert.ok(fills.some((n) => [43, 47, 50].includes(n.pitch)));
  // straight snare backbeat on 2 and 4, half time on 3
  const sn = (ns) => [...new Set(ns.filter((n) => n.pitch === 38 && n.vel > 60).map((n) => n.start % 4))].sort();
  deq(sn(D.generate({ ...base, loudness: 0.7, ghost: 0 })), [1, 3]);
  deq(sn(D.generate({ ...base, loudness: 0.7, ghost: 0, timeMode: 'half' })), [2]);
  // follow: the kick plays exactly the reference rhythm
  const follow = [0, 0.75, 2.5, 4, 4.75, 6.5];
  const kicks = D.generate({ ...base, length: 8, follow }).filter((n) => n.pitch === 36).map((n) => n.start);
  deq(kicks, follow);
  // swing & feel move notes but keep the grid order
  const sw = D.generate({ ...base, swing: 1, swingGrid: 8, length: 4 }).filter((n) => n.pitch === 42).map((n) => n.start);
  assert.ok(sw.includes(0.75));
  const late = D.generate({ ...base, feel: -1, length: 4 }).filter((n) => n.pitch === 38)[0].start;
  assert.ok(late > 1);
});

test('drummer handles odd meters', () => {
  for (const bpb of [3, 5, 6, 7, 1.5]) valid(D.generate({ persona: 'leo', length: bpb * 4, beatsPerBar: bpb, seed: 2 }), bpb * 4);
});

test('bass: in range, roots on chord changes, follows the kick', () => {
  for (const p of BM.styles.BASSISTS) {
    const n = B.generate({ persona: p.id, prog, length: 32, seed: 5 });
    assert.ok(n.length > 0, p.id);
    valid(n, 32);
    for (const x of n) assert.ok(x.pitch >= 14 && x.pitch <= 70, p.id + ' ' + x.pitch);
  }
  const n = B.generate({ persona: 'lino', prog, length: 32, seed: 5, humanize: 0, swing: 0, feel: 0, fills: 0, slides: 0, deadNotes: 0 });
  for (const ev of prog) {
    const first = n.find((x) => Math.abs(x.start - ev.start) < 1e-6 && x.vel > 40);
    assert.ok(first, 'note at chord change ' + ev.start);
    assert.equal(T.mod12(first.pitch), T.bassPc(ev.chord), 'bass note at ' + ev.start);
  }
  const kick = [0, 0.75, 2, 2.5, 4, 4.75, 6, 6.5];
  const f = B.generate({ persona: 'lino', prog, length: 8, seed: 1, humanize: 0, feel: 0, swing: 0, followKick: 1, kick, fills: 0, deadNotes: 0, slides: 0 });
  const onKick = f.filter((x) => kick.some((k) => Math.abs(k - x.start) < 1e-6)).length;
  assert.ok(onKick / f.length > 0.7, 'kick alignment ' + onKick + '/' + f.length);
});

test('keys: right hand plays chord tones, smooth voice leading, hands can be muted', () => {
  for (const p of BM.styles.KEYBOARDISTS) {
    const n = K.generate({ persona: p.id, prog, length: 32, seed: 9 });
    assert.ok(n.length > 0, p.id);
    valid(n, 32);
  }
  const n = K.generate({ persona: 'ettore', prog, length: 32, seed: 9, humanize: 0, swing: 0, feel: 0, fills: 0, ext: 0, lh: 'off' });
  for (const x of n) {
    const ch = C.chordAt(prog, x.start + 1e-6);
    assert.ok(T.chordPcs(ch).includes(T.mod12(x.pitch)), T.pitchName(x.pitch) + ' over ' + T.chordName(ch));
  }
  // voice leading: top voice of consecutive chords moves by small steps
  const tops = [];
  for (const ev of prog) {
    const at = n.filter((x) => Math.abs(x.start - ev.start) < 1e-6);
    if (at.length) tops.push(Math.max(...at.map((x) => x.pitch)));
  }
  for (let i = 1; i < tops.length; i++) assert.ok(Math.abs(tops[i] - tops[i - 1]) <= 7, 'top voice jump ' + tops[i - 1] + '->' + tops[i]);
  const lhOnly = K.generate({ persona: 'clara', prog, length: 32, seed: 9, muteRH: true });
  assert.ok(lhOnly.every((x) => x.pitch < 64));
  assert.equal(K.generate({ persona: 'clara', prog, length: 32, seed: 9, muteRH: true, muteLH: true }).length, 0);
  // register follows the range control
  const avg = (ns) => ns.reduce((a, x) => a + x.pitch, 0) / ns.length;
  const lo = K.generate({ persona: 'ettore', prog, length: 32, seed: 9, lh: 'off', range: 0 });
  const hi = K.generate({ persona: 'ettore', prog, length: 32, seed: 9, lh: 'off', range: 1 });
  assert.ok(avg(hi) - avg(lo) > 12);
});

test('sections change the performance', () => {
  const base = { persona: 'marco', length: 32, seed: 3, humanize: 0 };
  const verse = D.generate({ ...base, sectionName: 'Strofa 1' }), chorus = D.generate({ ...base, sectionName: 'Chorus' });
  const avg = (ns) => ns.reduce((a, n) => a + n.vel, 0) / ns.length;
  assert.ok(avg(chorus) > avg(verse));
  assert.ok(chorus.some((n) => n.pitch === 49 && n.start === 0));
  assert.equal(G.section('Pre-Chorus').id, 'pre');
  assert.equal(G.section('Ritornello').id, 'chorus');
});

test('electronic drummer: genres, per-piece switches and density, sections, micro-timing', () => {
  const E = BM.electro;
  for (const p of BM.styles.BEATMAKERS) {
    const n = E.generate({ persona: p.id, length: 32, seed: 4, fills: 0 });
    valid(n, 32);
    deq(E.generate({ persona: p.id, length: 32, seed: 4, fills: 0 }), n);
    valid(E.generate({ persona: p.id, length: 32, seed: 4, fills: 1, sectionName: 'Drop' }), 32);
    // four on the floor, kick never pushed off the grid
    const kicks = n.filter((x) => x.pitch === 36 && x.vel > 80).map((x) => x.start);
    for (let b = 0; b < 32; b++) assert.ok(kicks.includes(b), p.id + ' kick on beat ' + b);
  }
  const base = { persona: 'volt', length: 32, seed: 2, fills: 0, humanize: 0, micro: 0, swing: 0 };
  const off = E.generate({ ...base, on_clap: false, on_hat: false });
  assert.ok(!off.some((x) => x.pitch === 39 || x.pitch === 42));
  const sparse = E.generate({ ...base, d_hat: 0 }).filter((x) => x.pitch === 42).length;
  const dense = E.generate({ ...base, d_hat: 1 }).filter((x) => x.pitch === 42).length;
  assert.ok(dense > sparse + 20);
  assert.ok(!E.generate({ ...base, sectionName: 'Break' }).some((x) => x.pitch === 36));
  const buildN = E.generate({ ...base, sectionName: 'Build up' }).filter((x) => x.pitch === 38).length;
  assert.ok(buildN > 16, 'build roll');
  // micro-timing pushes rim/perc off the grid, kick stays put
  const micro = E.generate({ persona: 'mica', length: 16, seed: 2, micro: 1, swing: 0, humanize: 0 });
  assert.ok(micro.filter((x) => x.pitch === 37).some((x) => Math.abs(x.start * 4 - Math.round(x.start * 4)) > 0.02));
  assert.ok(micro.filter((x) => x.pitch === 36).every((x) => Math.abs(x.start * 4 - Math.round(x.start * 4)) < 1e-6));
});

test('genres: acoustic and electronic line-ups', () => {
  const g = (list) => [...new Set(list.map((p) => p.genre))].sort();
  deq(g(BM.styles.DRUMMERS), ['Funk', 'Jazz', 'Pop', 'R&B', 'Rock', 'Soul']);
  deq(g(BM.styles.BEATMAKERS), ['House', 'Microhouse', 'Rominimal', 'Tech House', 'Techno']);
  for (const list of [BM.styles.BASSISTS, BM.styles.KEYBOARDISTS]) {
    const genres = g(list).join(',');
    for (const x of ['Soul', 'Jazz', 'R&B', 'Rock', 'Pop', 'Funk', 'House', 'Tech House', 'Techno', 'Microhouse']) assert.ok(genres.includes(x), x);
  }
});
