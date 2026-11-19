'use strict';
// Runs the built Max for Live scripts (devices/*.js) against a simulated Live set.
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('fs');
const path = require('path');
const { makeSet, loadDevice } = require('./live_mock.js');

const DEV_DIR = path.join(__dirname, '..', 'devices');

test('device scripts use only ES5 syntax (Max js object)', () => {
  const forbidden = /=>|\blet |\bconst |`|\bclass |\(\.\.\.|\[\.\.\.|\.includes\(|Object\.assign|Array\.from|\.startsWith\(|\.padStart\(|Math\.imul/;
  for (const f of fs.readdirSync(DEV_DIR).filter((x) => x.endsWith('.js'))) {
    fs.readFileSync(path.join(DEV_DIR, f), 'utf8').split('\n').forEach((line, i) => {
      if (/^\s*\/\//.test(line)) return;
      assert.ok(!forbidden.test(line), f + ':' + (i + 1) + ' ' + line.trim());
    });
  }
});

test('amxd container wraps the patcher', () => {
  for (const f of fs.readdirSync(DEV_DIR).filter((x) => x.endsWith('.amxd'))) {
    const b = fs.readFileSync(path.join(DEV_DIR, f));
    assert.equal(b.toString('latin1', 0, 4), 'ampf');
    assert.equal(b.toString('latin1', 8, 12), 'mmmm');
    assert.equal(b.toString('latin1', 24, 28), 'ptch');
    const len = b.readUInt32LE(28);
    assert.equal(b.length, 32 + len);
    const p = JSON.parse(b.toString('utf8', 32, 32 + len - 1));
    assert.ok(p.patcher.boxes.length > 10);
  }
});

test('chords -> drums -> bass -> keys -> beats in a simulated Live set', () => {
  const set = makeSet();
  // 1) chord track: type a progression and write it into scene 1
  const chords = loadDevice(set, 'bm_chords.js', 0);
  chords.init();
  set.view.highlighted = 'live_set tracks 0 clip_slots 0';
  chords.write();
  chords.text('text', 'Am7', '|', 'Dm7', '|', 'G7', '|', 'Cmaj7');
  const cc = set.tracks[0].slots[0].clip;
  assert.ok(cc, 'chord clip created');
  assert.equal(cc.length, 16);
  assert.ok(cc.notes.length >= 16);
  // Chord ID on the written clip gives the progression back
  set.view.detail = 'live_set tracks 0 clip_slots 0 clip';
  chords.read();
  const te = chords.__out.filter((o) => o[1] === 'textedit' && o[2] === 'set').pop();
  assert.equal(te[3], 'Am7 | Dm7 | G7 | Cmaj7');
  chords.use_preset();
  set.view.detail = null;

  // 2) drummer on track 1, same scene
  const drums = loadDevice(set, 'bm_drums.js', 1);
  drums.init();
  set.view.highlighted = 'live_set tracks 1 clip_slots 0';
  drums.generate();
  const dc = set.tracks[1].slots[0].clip;
  assert.ok(dc && dc.notes.length > 30, 'drum clip');
  assert.equal(dc.length, 16, 'length follows the chord clip');
  assert.match(dc.name, /Strofa \[bm1 /);

  // 3) bass follows chords and kick
  const bass = loadDevice(set, 'bm_bass.js', 2);
  bass.init();
  set.view.highlighted = 'live_set tracks 2 clip_slots 0';
  bass.param('followKick', 100);
  bass.patcher.controls.followKick = 100;
  bass.patcher.controls.fills = 0;
  bass.generate();
  const bc = set.tracks[2].slots[0].clip;
  assert.ok(bc && bc.notes.length > 4);
  const roots = { 0: 9, 4: 2, 8: 7, 12: 0 };
  for (const [t, pc] of Object.entries(roots)) {
    const n = bc.notes.filter((x) => Math.abs(x.start_time - t) < 0.05 && x.velocity > 40)[0];
    assert.ok(n, 'bass note at beat ' + t);
    assert.equal(n.pitch % 12, pc, 'root at beat ' + t);
  }

  // 4) keys
  const keys = loadDevice(set, 'bm_keys.js', 3);
  keys.init();
  set.view.highlighted = 'live_set tracks 3 clip_slots 0';
  keys.generate();
  assert.ok(set.tracks[3].slots[0].clip.notes.length > 8);

  // 5) electronic drummer
  const beats = loadDevice(set, 'bm_beats.js', 4);
  beats.init();
  set.view.highlighted = 'live_set tracks 4 clip_slots 0';
  beats.patcher.controls.persona = 3; // Microhouse
  beats.generate();
  assert.match(set.tracks[4].slots[0].clip.name, /^Mica/);

  // 6) selecting a generated clip restores its settings (per-clip performance)
  drums.patcher.controls.fills = 99;
  set.view.detail = 'live_set tracks 1 clip_slots 0 clip';
  drums.onSelection();
  assert.equal(drums.patcher.controls.fills, 40); // value used when the clip was generated

  // 7) changing a control regenerates the selected clip (Auto)
  const before = JSON.stringify(dc.notes);
  drums.patcher.controls.complexity = 100;
  drums.param('complexity', 100);
  assert.notEqual(JSON.stringify(set.tracks[1].slots[0].clip.notes), before);

  // 8) all scenes: a second chord clip in scene 2 gets its own drum clip, named after the scene
  set.tracks[0].slots[1].clip = set.clip(8, cc.notes.filter((n) => n.start_time < 8), 'Am7 | Dm7');
  set.view.detail = null;
  drums.generate_scenes();
  assert.match(set.tracks[1].slots[1].clip.name, /Ritornello/);
  assert.equal(set.tracks[1].slots[1].clip.length, 8);

  // 9) arrangement: clips are created under the chord track's arrangement clips
  set.tracks[0].arrangement.push(Object.assign(set.clip(16, cc.notes), { start_time: 32 }));
  bass.generate_arrangement();
  assert.equal(set.tracks[2].arrangement.length, 1);
  assert.match(set.tracks[2].arrangement[0].name, /Chorus/);
});
