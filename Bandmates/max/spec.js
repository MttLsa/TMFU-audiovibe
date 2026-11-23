// Bandmates - device controls: shared by tools/build.js (to draw the devices) and by the device scripts
// (to read the controls and to save/restore settings in clip names). ES5.
(function (BM) {
  'use strict';
  var S = BM.styles;
  function names(list) { var o = []; for (var i = 0; i < list.length; i++) o.push(list[i].name + ' · ' + list[i].genre); return o; }
  var PRESET_NAMES = ['Standard', 'Strofa', 'Ritornello', 'Morbido', 'Pieno', 'Essenziale'];
  var VARS = ['1', '2', '3', '4', '5'];

  // kinds: xy (two hidden dials driven by the pad), dial, tab, menu, toggle, button
  // conv: pct (0..100 -> 0..1), bipct (-100..100 -> -1..1), enum (-> values[i]), bool, int, raw
  function P(id, kind, label, short, rect, extra) {
    var p = { id: id, kind: kind, label: label, short: short, rect: rect };
    for (var k in extra) if (extra.hasOwnProperty(k)) p[k] = extra[k];
    if (p.conv === undefined) p.conv = kind === 'toggle' ? 'bool' : (kind === 'tab' || kind === 'menu' ? 'enum' : 'pct');
    if (p.min === undefined) p.min = p.conv === 'bipct' ? -100 : 0;
    if (p.max === undefined) p.max = p.items ? p.items.length - 1 : (p.conv === 'bool' ? 1 : 100);
    return p;
  }

  function common(personaLabel, personas) {
    return [
      P('complexity', 'xy', 'Complessita', 'Complex', null, { def: 50, axis: 'x' }),
      P('loudness', 'xy', 'Intensita', 'Intens', null, { def: 50, axis: 'y' }),
      P('persona', 'menu', personaLabel, 'Player', [162, 15, 132, 16], { items: names(personas), values: personas.map(function (x) { return x.id; }), def: 0, noStore: true }),
      P('preset', 'menu', 'Preset', 'Preset', [162, 46, 132, 16], { items: PRESET_NAMES, def: 0, conv: 'int', noStore: true }),
      P('fills', 'dial', 'Fill', 'Fill', [160, 66, 44, 48], { def: 40 }),
      P('swing', 'dial', 'Swing', 'Swing', [206, 66, 44, 48], { def: 0 }),
      P('feel', 'dial', 'Feel (indietro/avanti)', 'Feel', [252, 66, 44, 48], { def: 0, conv: 'bipct' }),
      P('humanize', 'dial', 'Umanizza', 'Human', [160, 116, 44, 48], { def: 35 }),
      P('swingGrid', 'tab', 'Griglia swing', 'Grid', [206, 132, 88, 16], { items: ['8vi', '16mi'], values: [8, 16], def: 1 })
    ];
  }
  var LABELS_COMMON = [['Player', 162, 2], ['Preset', 162, 33]];
  function buttons(x) {
    function r(a) { return [a[0] + x - 600, a[1], a[2], a[3]]; }
    return [
    P('generate', 'button', 'Genera', 'Genera', [600, 4, 152, 26], { conv: 'raw', noStore: true, msg: 'generate', text: '▶  Genera' }),
    P('newvar', 'button', 'Nuova variazione', 'Var', [600, 34, 152, 18], { conv: 'raw', noStore: true, msg: 'newvariation', text: 'Nuova variazione' }),
    P('allscenes', 'button', 'Tutte le scene', 'Scene', [600, 56, 74, 18], { conv: 'raw', noStore: true, msg: 'generate_scenes', text: 'Tutte le scene' }),
    P('arrangement', 'button', 'Arrangement', 'Arr', [678, 56, 74, 18], { conv: 'raw', noStore: true, msg: 'generate_arrangement', text: 'Arrangement' }),
    P('auto', 'toggle', 'Rigenera automatico', 'Auto', [600, 78, 74, 18], { def: 1, noStore: true, text: 'Auto' }),
    P('followSections', 'toggle', 'Segui sezioni', 'Sezioni', [678, 78, 74, 18], { def: 1, text: 'Segui sezioni' })
    ].map(function (p) { p.rect = r(p.rect); return p; });
  }

  var VOICE_LABELS = ['Kick', 'Clap', 'Rim', 'Hat', 'Open', 'Perc', 'Ride', 'Crash'];
  function voiceParams() {
    var out = [], V = BM.electro.VOICES;
    for (var i = 0; i < V.length; i++) {
      var x = 306 + 48 * i;
      out.push(P('on_' + V[i], 'toggle', VOICE_LABELS[i] + ' on', VOICE_LABELS[i], [x, 16, 44, 16], { def: V[i] === 'ride' ? 0 : 1, text: VOICE_LABELS[i] }));
      out.push(P('d_' + V[i], 'dial', VOICE_LABELS[i] + ' densita', VOICE_LABELS[i].substr(0, 4) + 'D', [x, 38, 44, 48], { def: 50 }));
    }
    return out;
  }

  var DEVICES = {
    drums: {
      title: 'Bandmates Drums', file: 'bm_drums.js', width: 760,
      params: common('Batterista', S.DRUMMERS).concat([
        P('ghost', 'dial', 'Ghost notes', 'Ghost', [252, 116, 44, 48], { def: 30 }),
        P('kitPerc', 'toggle', 'Percussioni', 'Perc', [306, 18, 76, 16], { def: 0, text: 'Percussioni' }),
        P('percType', 'tab', 'Tipo percussioni', 'PercT', [386, 18, 120, 16], { items: ['Tamb', 'Shkr', 'Clap', 'Cowb', 'Conga'], values: ['tamb', 'shaker', 'clap', 'cowbell', 'conga'], def: 0 }),
        P('varPerc', 'tab', 'Variazione percussioni', 'PercV', [510, 18, 84, 16], { items: VARS, def: 0, conv: 'int' }),
        P('kitCymbals', 'toggle', 'Piatti', 'Cymb', [306, 40, 76, 16], { def: 1, text: 'Piatti' }),
        P('cymbal', 'tab', 'Hi-hat / Ride', 'HH/Ride', [386, 40, 120, 16], { items: ['Hi-hat', 'Ride'], values: ['hat', 'ride'], def: 0 }),
        P('varCymbal', 'tab', 'Variazione piatti', 'CymV', [510, 40, 84, 16], { items: VARS, def: 0, conv: 'int' }),
        P('kitKickSnare', 'toggle', 'Cassa e rullante', 'K+S', [306, 62, 76, 16], { def: 1, text: 'Cassa+Rull.' }),
        P('timeMode', 'tab', 'Tempo', 'Time', [386, 62, 120, 16], { items: ['1/2', 'Normale', 'x2'], values: ['half', 'normal', 'double'], def: 1 }),
        P('varKick', 'tab', 'Variazione cassa e rullante', 'KSV', [510, 62, 84, 16], { items: VARS, def: 0, conv: 'int' }),
        P('kitToms', 'toggle', 'Tom', 'Toms', [306, 84, 76, 16], { def: 1, text: 'Tom' }),
        P('hhAuto', 'toggle', 'Hi-hat automatico', 'HHAuto', [386, 84, 60, 16], { def: 1, text: 'HH auto' }),
        P('hhOpen', 'dial', 'Apertura hi-hat', 'HHOpen', [306, 110, 44, 48], { def: 30 }),
        P('followOn', 'toggle', 'Segui ritmo', 'Follow', [356, 112, 70, 16], { def: 0, text: 'Segui ritmo' })
      ]),
      labels: LABELS_COMMON.concat([['Kit', 306, 2], ['Pattern', 510, 2], ['Cassa segue la traccia:', 356, 130]]),
      menus: [{ id: 'followTrack', rect: [356, 144, 150, 18] }]
    },
    beats: {
      title: 'Bandmates Beats', file: 'bm_beats.js', width: 860, buttonsX: 700,
      params: common('Beatmaker', S.BEATMAKERS).concat(voiceParams()).concat([
        P('micro', 'dial', 'Micro-timing', 'Micro', [252, 116, 44, 48], { def: 10 })
      ]),
      labels: LABELS_COMMON.concat([['Kit elettronico: on/off e densita di ogni pezzo (il pad XY li sposta tutti)', 306, 2],
        ['House \u00B7 Tech House \u00B7 Techno \u00B7 Microhouse \u00B7 Rominimal', 306, 96],
        ['Sezioni dai nomi delle scene: Intro, Build, Drop, Break, Outro', 306, 112]])
    },
    bass: {
      title: 'Bandmates Bass', file: 'bm_bass.js', width: 760,
      params: common('Bassista', S.BASSISTS).concat([
        P('slides', 'dial', 'Slide', 'Slides', [306, 14, 44, 48], { def: 20 }),
        P('deadNotes', 'dial', 'Dead notes', 'Dead', [354, 14, 44, 48], { def: 20 }),
        P('followKick', 'dial', 'Segui cassa', 'Kick', [402, 14, 44, 48], { def: 0 }),
        P('sustain', 'dial', 'Durata note', 'Length', [450, 14, 44, 48], { def: 70 }),
        P('slideMode', 'tab', 'Modo slide', 'SlideM', [306, 70, 120, 16], { items: ['Nota di grazia', 'Legato'], values: ['grace', 'legato'], def: 0 }),
        P('octave', 'tab', 'Ottava', 'Oct', [430, 70, 90, 16], { items: ['-1', '0', '+1'], values: [-1, 0, 1], def: 1 }),
        P('variation', 'tab', 'Variazione', 'Var', [306, 104, 120, 16], { items: VARS, def: 0, conv: 'int' })
      ]),
      labels: LABELS_COMMON.concat([['Ottava', 430, 56], ['Slide', 306, 56], ['Variazione ritmo', 306, 90], ['Accordi: traccia "Bandmates Chords"', 306, 128], ['Cassa: traccia "Bandmates Drums"', 306, 142]])
    },
    keys: {
      title: 'Bandmates Keys', file: 'bm_keys.js', width: 760,
      params: common('Tastierista', S.KEYBOARDISTS).concat([
        P('lh', 'menu', 'Mano sinistra', 'LH', [306, 15, 110, 16], { items: ['Off', 'Fondamentale', 'Ottave', 'Fond. + quinta', 'Arpeggiata', 'Shell (jazz)', 'Stride'], values: ['off', 'root', 'octaves', 'root5', 'broken', 'shell', 'stride'], def: 2 }),
        P('rhVoicing', 'menu', 'Voicing mano destra', 'RHV', [420, 15, 110, 16], { items: ['Triade', 'Stretto 4 voci', 'Aperto', 'Drop 2', 'Shell', 'Quarte', 'Ottava sopra', 'Ricco'], values: ['triad', 'close', 'open', 'drop2', 'shell', 'quartal', 'octave', 'rich'], def: 1 }),
        P('rhMove', 'menu', 'Movimento mano destra', 'RHM', [420, 46, 110, 16], { items: ['Tenuto', 'Pulsato', 'Comping', 'Spezzato', 'Arpeggio', 'In levare'], values: ['sustain', 'pulse', 'comp', 'broken', 'arp', 'offbeat'], def: 1 }),
        P('muteLH', 'toggle', 'Muto mano sinistra', 'MuteL', [306, 46, 110, 16], { def: 0, text: 'Muto sinistra' }),
        P('muteRH', 'toggle', 'Muto mano destra', 'MuteR', [534, 15, 60, 16], { def: 0, text: 'Muto' }),
        P('arpDir', 'tab', 'Direzione arpeggio', 'Arp', [534, 46, 60, 16], { items: ['↑', '↓', '↕'], values: ['up', 'down', 'updown'], def: 2 }),
        P('range', 'dial', 'Registro mano destra', 'Range', [306, 70, 44, 48], { def: 50 }),
        P('ext', 'dial', 'Estensioni', 'Ext', [354, 70, 44, 48], { def: 30 }),
        P('pedal', 'dial', 'Pedale', 'Pedal', [402, 70, 44, 48], { def: 30 }),
        P('variation', 'tab', 'Variazione', 'Var', [454, 84, 140, 16], { items: VARS, def: 0, conv: 'int' })
      ]),
      labels: LABELS_COMMON.concat([['Mano sinistra', 306, 2], ['Mano destra', 420, 2], ['Movimento', 420, 33], ['Variazione', 454, 70], ['Accordi: traccia "Bandmates Chords"', 306, 128]])
    },
    chords: {
      title: 'Bandmates Chords', file: 'bm_chords.js', width: 640, noXY: true,
      params: [
        P('progPreset', 'menu', 'Progressione', 'Prog', [6, 15, 170, 16], { items: BM.chords.PRESETS.map(function (p) { return p.name; }), def: 0, conv: 'int', noStore: true }),
        P('key', 'menu', 'Tonalita', 'Key', [180, 15, 54, 16], { items: BM.theory.SHARP.concat(['Live']), def: 12, conv: 'int' }),
        P('flats', 'toggle', 'Bemolli', 'Flats', [238, 15, 54, 16], { def: 0, text: 'Bemolli' }),
        P('idRes', 'tab', 'Risoluzione Chord ID', 'IDRes', [296, 15, 120, 16], { items: ['1/4', '1/2', '1 bar'], values: [1, 2, 4], def: 1 }),
        P('ext', 'dial', 'Estensioni nel clip', 'Ext', [420, 2, 44, 48], { def: 0 }),
        P('apply', 'button', 'Usa preset', 'Use', [6, 34, 170, 16], { conv: 'raw', noStore: true, msg: 'use_preset', text: 'Usa questa progressione' }),
        P('write', 'button', 'Scrivi nel clip', 'Write', [470, 4, 164, 24], { conv: 'raw', noStore: true, msg: 'write', text: '▶  Scrivi nel clip' }),
        P('read', 'button', 'Chord ID dal clip selezionato', 'Read', [470, 32, 164, 18], { conv: 'raw', noStore: true, msg: 'read', text: 'Chord ID dal clip selezionato' }),
        P('transUp', 'button', 'Trasponi su', 'Up', [296, 34, 58, 16], { conv: 'raw', noStore: true, msg: 'transpose 1', text: '+1 st' }),
        P('transDown', 'button', 'Trasponi giu', 'Down', [358, 34, 58, 16], { conv: 'raw', noStore: true, msg: 'transpose -1', text: '-1 st' })
      ],
      labels: [['Progressione', 6, 2], ['Tonalita', 180, 2], ['Chord ID', 296, 2],
               ['Accordi: battute separate da |, "-" prolunga, "%" ripete la battuta. Es: C G/B | Am7 - | F | G7sus4 G7', 6, 54]],
      text: { id: 'progText', rect: [6, 68, 628, 70] }
    }
  };
  for (var d in DEVICES) if (DEVICES.hasOwnProperty(d) && !DEVICES[d].noXY) DEVICES[d].params = DEVICES[d].params.concat(buttons(DEVICES[d].buttonsX || 600));

  // UI value -> engine value
  function toEngine(p, v) {
    switch (p.conv) {
      case 'pct': return v / 100;
      case 'bipct': return v / 100;
      case 'bool': return !!v;
      case 'int': return Math.round(v);
      case 'enum': return p.values ? p.values[Math.max(0, Math.min(p.values.length - 1, Math.round(v)))] : Math.round(v);
      default: return v;
    }
  }
  // engine value -> UI value (for presets)
  function fromEngine(p, x) {
    switch (p.conv) {
      case 'pct': case 'bipct': return Math.round(x * 100);
      case 'bool': return x ? 1 : 0;
      case 'enum':
        if (p.values) { for (var i = 0; i < p.values.length; i++) if (p.values[i] === x) return i; return null; }
        return x;
      default: return x;
    }
  }

  // settings <-> compact clip-name tag, e.g. "[bm1 2s3c...]"
  var B36 = '0123456789abcdefghijklmnopqrstuvwxyz';
  function enc(n) { n = Math.max(0, Math.min(1295, Math.round(n))); return B36.charAt(Math.floor(n / 36)) + B36.charAt(n % 36); }
  function dec(s) { return B36.indexOf(s.charAt(0)) * 36 + B36.indexOf(s.charAt(1)); }
  function storable(dev) {
    var out = [], ps = DEVICES[dev].params;
    for (var i = 0; i < ps.length; i++) if (!ps[i].noStore && ps[i].kind !== 'button') out.push(ps[i]);
    return out;
  }
  // values: {id: uiValue}; persona index and seed are stored too
  function encodeTag(dev, values, personaIdx, seed) {
    var s = 'bm1 ' + enc(personaIdx) + enc(seed % 1296) + enc(Math.floor(seed / 1296) % 1296);
    var ps = storable(dev);
    for (var i = 0; i < ps.length; i++) s += enc((values[ps[i].id] || 0) - ps[i].min);
    return '[' + s + ']';
  }
  function decodeTag(dev, name) {
    var m = /\[bm1 ([0-9a-z]+)\]/.exec(name || '');
    if (!m) return null;
    var body = m[1], ps = storable(dev);
    if (body.length !== 6 + 2 * ps.length) return null;
    var out = { persona: dec(body.substr(0, 2)), seed: dec(body.substr(2, 2)) + 1296 * dec(body.substr(4, 2)), values: {} };
    for (var i = 0; i < ps.length; i++) out.values[ps[i].id] = dec(body.substr(6 + 2 * i, 2)) + ps[i].min;
    return out;
  }
  function stripTag(name) { return String(name || '').replace(/\s*\[bm1 [0-9a-z]+\]/, ''); }

  BM.spec = { DEVICES: DEVICES, PRESET_NAMES: PRESET_NAMES, toEngine: toEngine, fromEngine: fromEngine,
    encodeTag: encodeTag, decodeTag: decodeTag, stripTag: stripTag, storable: storable };
})(BM);
