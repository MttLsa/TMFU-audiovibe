#!/usr/bin/env node
// Builds the Max for Live devices into ../devices:
//   bm_<device>.js  = engine + spec + runtime + device script (one self-contained file per device)
//   Bandmates <Name>.amxd (MIDI effect) + .maxpat (same patcher, to open in Max)
// Usage: node tools/build.js [--check]   (--check fails if the committed devices are out of date)
'use strict';
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const ROOT = path.join(__dirname, '..');
const OUT = path.join(ROOT, 'devices');
const VERSION = JSON.parse(fs.readFileSync(path.join(ROOT, 'package.json'), 'utf8')).version;
const engineSrc = require(path.join(ROOT, 'engine', 'load.js')).source();
const specSrc = fs.readFileSync(path.join(ROOT, 'max', 'spec.js'), 'utf8');

const ctx = { module: { exports: {} } };
vm.createContext(ctx);
vm.runInContext(engineSrc + '\n' + specSrc, ctx);
const BM = ctx.module.exports;
const DEVICES = BM.spec.DEVICES;

// ------------------------------------------------------------------ patcher helpers
let nextId = 1;
function box(props) { const id = 'obj-' + nextId++; return { id, box: Object.assign({ id }, props) }; }
const newobj = (text, rect, nin, outs) => box({ maxclass: 'newobj', text, numinlets: nin, numoutlets: outs.length, outlettype: outs, patching_rect: rect });
const message = (text, rect) => box({ maxclass: 'message', text, numinlets: 2, numoutlets: 1, outlettype: [''], patching_rect: rect });
const pres = (rect) => ({ presentation: 1, presentation_rect: rect });

function label(text, rect) {
  return box(Object.assign({ maxclass: 'live.comment', text, numinlets: 1, numoutlets: 0, fontsize: 9.0,
    patching_rect: rect }, pres(rect)));
}

function control(p) {
  const r = p.rect || [0, 0, 44, 48];
  const base = { varname: p.id, patching_rect: [r[0], r[1] + 300, r[2], r[3]] };
  if (p.rect) Object.assign(base, pres(r));
  const valueof = { parameter_longname: p.label, parameter_shortname: p.short, parameter_initial_enable: 1, parameter_initial: [p.def || 0] };
  switch (p.kind) {
    case 'xy':
    case 'dial':
      Object.assign(valueof, { parameter_type: 0, parameter_mmin: p.min, parameter_mmax: p.max, parameter_unitstyle: p.conv === 'bipct' ? 0 : 5 });
      return box(Object.assign(base, { maxclass: 'live.dial', numinlets: 1, numoutlets: 2, outlettype: ['', 'float'], parameter_enable: 1,
        showname: 1, fontsize: 9.0, saved_attribute_attributes: { valueof } }));
    case 'tab':
      Object.assign(valueof, { parameter_type: 2, parameter_mmax: p.items.length - 1, parameter_enum: p.items, parameter_unitstyle: 9 });
      return box(Object.assign(base, { maxclass: 'live.tab', num_lines_patching: 1, num_lines_presentation: 1, numinlets: 1, numoutlets: 3,
        outlettype: ['', '', 'float'], parameter_enable: 1, fontsize: 8.5, saved_attribute_attributes: { valueof } }));
    case 'menu':
      Object.assign(valueof, { parameter_type: 2, parameter_mmax: p.items.length - 1, parameter_enum: p.items, parameter_unitstyle: 9 });
      return box(Object.assign(base, { maxclass: 'live.menu', numinlets: 1, numoutlets: 3, outlettype: ['', '', 'float'],
        parameter_enable: 1, fontsize: 9.0, saved_attribute_attributes: { valueof } }));
    case 'toggle':
      Object.assign(valueof, { parameter_type: 2, parameter_mmax: 1, parameter_enum: ['off', 'on'], parameter_unitstyle: 9 });
      return box(Object.assign(base, { maxclass: 'live.text', mode: 1, text: p.text || p.short, texton: p.text || p.short,
        numinlets: 1, numoutlets: 2, outlettype: ['', ''], parameter_enable: 1, fontsize: 8.5, saved_attribute_attributes: { valueof } }));
    case 'button':
      return box(Object.assign(base, { maxclass: 'live.text', mode: 0, text: p.text || p.short, texton: p.text || p.short,
        numinlets: 1, numoutlets: 2, outlettype: ['', ''], parameter_enable: 0, fontsize: 9.0 }));
    default: throw new Error('kind ' + p.kind);
  }
}

function buildPatcher(devId) {
  nextId = 1;
  const d = DEVICES[devId], boxes = [], lines = [];
  const add = (b) => { boxes.push({ box: b.box }); return b.id; };
  const link = (a, ao, b, bi) => lines.push({ patchline: { source: [a, ao], destination: [b, bi] } });

  const midiin = add(newobj('midiin', [20, 20, 40, 22], 1, ['int']));
  const midiout = add(newobj('midiout', [20, 60, 50, 22], 1, []));
  link(midiin, 0, midiout, 0);
  const thisdev = add(newobj('live.thisdevice', [100, 20, 90, 22], 1, ['bang', 'int', 'int']));
  const initMsg = add(message('init', [100, 50, 35, 22]));
  link(thisdev, 0, initMsg, 0);
  const js = add(newobj('js ' + d.file, [100, 200, 160, 22], 1, ['', '']));
  link(initMsg, 0, js, 0);

  // status line
  const sx = d.buttonsX || 600;
  const statusRect = devId === 'chords' ? [6, 142, 628, 24] : [sx, 100, 152, 64];
  const statusBox = add(box(Object.assign({ maxclass: 'comment', text: 'Bandmates ' + VERSION, linecount: 4, fontsize: 9.0,
    textcolor: [0.85, 0.85, 0.85, 1.0], numinlets: 1, numoutlets: 0, patching_rect: [300, 200, 200, 50] }, pres(statusRect))));
  link(js, 0, statusBox, 0);
  const router = add(newobj('route xy followmenu textedit', [100, 240, 200, 22], 1, ['', '', '', '']));
  link(js, 1, router, 0);

  for (const [text, x, y] of d.labels || []) add(label(text, [x, y, Math.max(60, text.length * 5.2), 14]));

  let px = 20;
  const ids = {};
  for (const p of d.params) {
    const c = add(control(p));
    ids[p.id] = c;
    if (p.kind === 'button') {
      const m = add(message(p.msg, [px, 150, 80, 22]));
      link(c, 0, m, 0); link(m, 0, js, 0);
    } else {
      const pre = add(newobj('prepend param ' + p.id, [px, 150, 120, 22], 1, ['']));
      link(c, 0, pre, 0); link(pre, 0, js, 0);
    }
    px += 130;
  }

  if (!d.noXY) {
    const xy = add(box(Object.assign({ maxclass: 'jsui', filename: 'bm_xy.js', varname: 'xy', numinlets: 1, numoutlets: 1, outlettype: [''],
      parameter_enable: 0, patching_rect: [20, 400, 150, 150] }, pres([6, 6, 150, 156]))));
    const unpack = add(newobj('unpack f f', [20, 560, 70, 22], 1, ['float', 'float']));
    link(xy, 0, unpack, 0);
    link(unpack, 0, ids.complexity, 0);
    link(unpack, 1, ids.loudness, 0);
    const setx = add(newobj('prepend setx', [200, 560, 80, 22], 1, ['']));
    const sety = add(newobj('prepend sety', [290, 560, 80, 22], 1, ['']));
    link(ids.complexity, 0, setx, 0); link(setx, 0, xy, 0);
    link(ids.loudness, 0, sety, 0); link(sety, 0, xy, 0);
    const setxy = add(newobj('prepend setxy', [380, 560, 90, 22], 1, ['']));
    link(router, 0, setxy, 0); link(setxy, 0, xy, 0);
  }
  for (const m of d.menus || []) {
    const um = add(box(Object.assign({ maxclass: 'umenu', varname: m.id, numinlets: 1, numoutlets: 3, outlettype: ['int', '', ''],
      items: [], fontsize: 9.0, patching_rect: [500, 400, 150, 22] }, pres(m.rect))));
    const pre = add(newobj('prepend followtrack', [500, 430, 120, 22], 1, ['']));
    link(router, 1, um, 0);
    link(um, 1, pre, 0); link(pre, 0, js, 0);
  }
  if (d.text) {
    const te = add(box(Object.assign({ maxclass: 'textedit', varname: d.text.id, keymode: 1, lines: 3, wordwrap: 1, fontsize: 11.0,
      numinlets: 1, numoutlets: 4, outlettype: ['', 'int', '', ''], patching_rect: [500, 460, 300, 60],
      text: 'C G/B | Am7 - | Fmaj7 | G7sus4 G7' }, pres(d.text.rect))));
    const pre = add(newobj('prepend text', [500, 530, 80, 22], 1, ['']));
    link(router, 2, te, 0);
    link(te, 0, pre, 0); link(pre, 0, js, 0);
  }

  return {
    patcher: {
      fileversion: 1,
      appversion: { major: 8, minor: 6, revision: 0, architecture: 'x64', modernui: 1 },
      classnamespace: 'box',
      rect: [80, 100, 1100, 700],
      openrect: [0, 0, 0, 169],
      openinpresentation: 1,
      default_fontsize: 10.0, default_fontface: 0, default_fontname: 'Arial Bold',
      gridonopen: 1, gridsize: [8, 8], gridsnaponopen: 1, objectsnaponopen: 1, statusbarvisible: 2, toolbarvisible: 1,
      boxanimatetime: 200, enablehscroll: 1, enablevscroll: 1, devicewidth: d.width,
      description: d.title + ' ' + VERSION + ' - TMFU audiovibe', digest: '', tags: '', style: '', subpatcher_template: '',
      assistshowspatchername: 0,
      boxes, lines,
      dependency_cache: [
        { name: d.file, bootpath: '.', type: 'TEXT', implicit: 1 },
        ...(d.noXY ? [] : [{ name: 'bm_xy.js', bootpath: '.', type: 'TEXT', implicit: 1 }])
      ],
      latency: 0, is_mpe: 0, minimum_live_version: '', minimum_max_version: '', platform_compatibility: 0, autosave: 0
    }
  };
}

// .amxd container: 'ampf' header, device type ('mmmm' = MIDI effect), meta chunk, 'ptch' chunk with the patcher JSON
function amxd(patcher) {
  const json = Buffer.from(JSON.stringify(patcher, null, '\t') + '\n\0', 'utf8');
  const u32 = (n) => { const b = Buffer.alloc(4); b.writeUInt32LE(n); return b; };
  return Buffer.concat([Buffer.from('ampf'), u32(4), Buffer.from('mmmm'), Buffer.from('meta'), u32(4), u32(0),
    Buffer.from('ptch'), u32(json.length), json]);
}

function checkParams(devId) {
  const names = new Set();
  for (const p of DEVICES[devId].params) {
    if (p.kind === 'button') continue;
    if (names.has(p.label)) throw new Error(devId + ': duplicate parameter name ' + p.label);
    names.add(p.label);
  }
}

function build() {
  const files = {};
  const runtime = fs.readFileSync(path.join(ROOT, 'max', 'runtime.js'), 'utf8');
  for (const devId of Object.keys(DEVICES)) {
    checkParams(devId);
    const d = DEVICES[devId];
    const devSrc = fs.readFileSync(path.join(ROOT, 'max', devId + '.js'), 'utf8');
    files[d.file] = '// ' + d.title + ' ' + VERSION + ' - TMFU audiovibe. Generated by tools/build.js, do not edit.\n' +
      engineSrc + '\n' + specSrc + '\n' + runtime + '\n' + devSrc;
    const patcher = buildPatcher(devId);
    files[d.title + '.maxpat'] = JSON.stringify(patcher, null, '\t') + '\n';
    files[d.title + '.amxd'] = amxd(patcher);
  }
  files['bm_xy.js'] = fs.readFileSync(path.join(ROOT, 'max', 'bm_xy.js'), 'utf8');
  return files;
}

function main() {
  const check = process.argv.includes('--check');
  const files = build();
  let stale = [];
  if (!check) fs.mkdirSync(OUT, { recursive: true });
  for (const [name, content] of Object.entries(files)) {
    const p = path.join(OUT, name);
    const buf = Buffer.isBuffer(content) ? content : Buffer.from(content, 'utf8');
    if (check) {
      if (!fs.existsSync(p) || !fs.readFileSync(p).equals(buf)) stale.push(name);
    } else fs.writeFileSync(p, buf);
  }
  if (check && stale.length) { console.error('Devices out of date, run: node tools/build.js\n  ' + stale.join('\n  ')); process.exit(1); }
  console.log((check ? 'Devices up to date: ' : 'Built: ') + Object.keys(files).join(', '));
}

if (require.main === module) main();
module.exports = { build, buildPatcher, amxd };
