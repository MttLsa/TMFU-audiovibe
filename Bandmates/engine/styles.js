// Bandmates engine - original player personas and their presets.
// Names, descriptions and patterns are original to this project.
(function (BM) {
  'use strict';

  // Reference genres. Acoustic: Soul, Jazz, R&B, Rock, Pop, Funk.
  // Electronic (Beats device + electronic bass/keys): House, Tech House, Techno, Microhouse / Rominimal.
  var GENRES = ['Soul', 'Jazz', 'R&B', 'Rock', 'Pop', 'Funk'];
  var ELECTRONIC_GENRES = ['House', 'Tech House', 'Techno', 'Microhouse', 'Rominimal'];

  // ---------------- drummers (acoustic kit) ----------------
  // family -> pattern library in drummer.js
  var DRUMMERS = [
    { id: 'gil', name: 'Gil', genre: 'Soul', family: 'soul', desc: 'Soul classico: rullante su 2 e 4, tamburello, cassa con riprese',
      def: { complexity: 0.4, loudness: 0.5, fills: 0.4, swing: 0.1, swingGrid: 16, feel: -0.05, ghost: 0.4, cymbal: 'hat', kitPerc: true, percType: 'tamb' } },
    { id: 'nina', name: 'Nina', genre: 'Jazz', family: 'jazz', desc: 'Ride swing, charleston col pedale su 2 e 4, comping di rullante',
      def: { complexity: 0.5, loudness: 0.4, fills: 0.35, swing: 0.67, swingGrid: 8, feel: 0.05, ghost: 0.5, cymbal: 'ride' } },
    { id: 'dalia', name: 'Dalia', genre: 'R&B', family: 'rnb', desc: 'R&B moderno, dietro al tempo, swing sui sedicesimi, sidestick',
      def: { complexity: 0.45, loudness: 0.5, fills: 0.25, swing: 0.4, swingGrid: 16, feel: -0.4, ghost: 0.45, cymbal: 'hat' } },
    { id: 'marco', name: 'Marco', genre: 'Rock', family: 'rock', desc: 'Rock potente e diretto, crash generosi',
      def: { complexity: 0.45, loudness: 0.65, fills: 0.5, swing: 0, feel: 0.1, ghost: 0.3, cymbal: 'hat' } },
    { id: 'rafa', name: 'Rafa', genre: 'Rock', family: 'alt', desc: 'Rock alternativo con tom ossessivi e ride aperto',
      def: { complexity: 0.5, loudness: 0.6, fills: 0.55, swing: 0, feel: 0.15, ghost: 0.2, cymbal: 'ride' } },
    { id: 'leo', name: 'Leo', genre: 'Pop', family: 'pop', desc: 'Pop moderno e pulito, sedicesimi sul charleston',
      def: { complexity: 0.5, loudness: 0.55, fills: 0.4, swing: 0, feel: 0, ghost: 0.25, cymbal: 'hat' } },
    { id: 'ines', name: 'Ines', genre: 'Pop', family: 'pop', desc: 'Pop acustico delicato, sidestick nelle parti morbide',
      def: { complexity: 0.3, loudness: 0.35, fills: 0.3, swing: 0, feel: -0.1, ghost: 0.35, cymbal: 'hat' } },
    { id: 'teo', name: 'Teo', genre: 'Funk', family: 'funk', desc: 'Ghost notes, charleston a sedicesimi, molto groove',
      def: { complexity: 0.6, loudness: 0.55, fills: 0.4, swing: 0.15, swingGrid: 16, feel: 0.05, ghost: 0.75, cymbal: 'hat' } }
  ];

  // ---------------- electronic drummers (Beats device) ----------------
  var BEATMAKERS = [
    { id: 'volt', name: 'Volt', genre: 'House', family: 'house', desc: 'House classica: cassa dritta, clap, charleston aperto in levare, shaker',
      def: { complexity: 0.45, loudness: 0.6, fills: 0.35, swing: 0.25, swingGrid: 16, feel: 0, humanize: 0.2, micro: 0.1 } },
    { id: 'tess', name: 'Tess', genre: 'Tech House', family: 'techhouse', desc: 'Groove shuffle, percussioni sincopate, cassa con ghost',
      def: { complexity: 0.55, loudness: 0.6, fills: 0.3, swing: 0.35, swingGrid: 16, feel: -0.05, humanize: 0.2, micro: 0.2 } },
    { id: 'kern', name: 'Kern', genre: 'Techno', family: 'techno', desc: 'Techno dritta e ipnotica: hat a sedicesimi, ride, rumble',
      def: { complexity: 0.5, loudness: 0.7, fills: 0.3, swing: 0.05, swingGrid: 16, feel: 0.05, humanize: 0.1, micro: 0.05 } },
    { id: 'mica', name: 'Mica', genre: 'Microhouse', family: 'microhouse', desc: 'Click, rim e micro-tempi fuori griglia, groove saltellante',
      def: { complexity: 0.55, loudness: 0.45, fills: 0.25, swing: 0.4, swingGrid: 16, feel: -0.1, humanize: 0.35, micro: 0.6 } },
    { id: 'ilio', name: 'Ilio', genre: 'Rominimal', family: 'rominimal', desc: 'Groove rotolante su due battute, percussioni lunghe, molto spazio',
      def: { complexity: 0.6, loudness: 0.5, fills: 0.2, swing: 0.3, swingGrid: 16, feel: -0.15, humanize: 0.3, micro: 0.45 } }
  ];

  // ---------------- bass players ----------------
  var BASSISTS = [
    { id: 'bruno', name: 'Bruno', genre: 'Soul', family: 'soul', desc: 'Linee melodiche con note di passaggio',
      def: { complexity: 0.55, intensity: 0.5, fills: 0.5, slides: 0.25, deadNotes: 0.25, followKick: 0.2, octave: 0 } },
    { id: 'mira', name: 'Mira', genre: 'Jazz', family: 'walking', desc: 'Walking bass a semiminime con cromatismi',
      def: { complexity: 0.55, intensity: 0.45, fills: 0.3, slides: 0.1, deadNotes: 0.15, followKick: 0, octave: 0, swing: 0.67, swingGrid: 8 } },
    { id: 'rhea', name: 'Rhea', genre: 'R&B', family: 'rnb', desc: 'Note lunghe e scivolate, riprese a sedicesimi, molto spazio',
      def: { complexity: 0.45, intensity: 0.45, fills: 0.35, slides: 0.5, deadNotes: 0.1, followKick: 0.4, octave: 0, swing: 0.35, swingGrid: 16, slideMode: 'legato' } },
    { id: 'lino', name: 'Lino', genre: 'Rock', family: 'rock', desc: 'Ottavi sulla fondamentale, solido e compatto',
      def: { complexity: 0.35, intensity: 0.6, fills: 0.3, slides: 0.15, deadNotes: 0.1, followKick: 0.5, octave: 0 } },
    { id: 'ada', name: 'Ada', genre: 'Pop', family: 'pop', desc: 'Pop semplice e cantabile, segue la cassa',
      def: { complexity: 0.35, intensity: 0.5, fills: 0.25, slides: 0.15, deadNotes: 0.05, followKick: 0.5, octave: 0 } },
    { id: 'gaia', name: 'Gaia', genre: 'Funk', family: 'funk', desc: 'Sedicesimi sincopati, ottave e dead note',
      def: { complexity: 0.6, intensity: 0.6, fills: 0.45, slides: 0.3, deadNotes: 0.6, followKick: 0.3, octave: 0, swing: 0.1, swingGrid: 16 } },
    { id: 'sub', name: 'Sub', genre: 'House', family: 'house', desc: 'Basso house in levare, ottave e pompaggio',
      def: { complexity: 0.4, intensity: 0.6, fills: 0.2, slides: 0.1, deadNotes: 0, followKick: 0, octave: 0, swing: 0.25, swingGrid: 16, sustain: 0.4 } },
    { id: 'roll', name: 'Roll', genre: 'Tech House', family: 'techhouse', desc: 'Linea rotolante a sedicesimi con pause, molto groove',
      def: { complexity: 0.5, intensity: 0.6, fills: 0.2, slides: 0.15, deadNotes: 0, followKick: 0, octave: 0, swing: 0.35, swingGrid: 16, sustain: 0.35 } },
    { id: 'ohm', name: 'Ohm', genre: 'Techno', family: 'techno', desc: 'Rumble e sedicesimi ipnotici tra le casse',
      def: { complexity: 0.5, intensity: 0.65, fills: 0.1, slides: 0, deadNotes: 0, followKick: 0, octave: 0, swing: 0.05, swingGrid: 16, sustain: 0.3 } },
    { id: 'dot', name: 'Dot', genre: 'Microhouse / Rominimal', family: 'microhouse', desc: 'Note corte e sparse, sincopi, ottave saltellanti',
      def: { complexity: 0.5, intensity: 0.5, fills: 0.15, slides: 0.2, deadNotes: 0, followKick: 0, octave: 0, swing: 0.4, swingGrid: 16, sustain: 0.25 } }
  ];

  // ---------------- keyboard players ----------------
  var KEYBOARDISTS = [
    { id: 'sol', name: 'Sol', genre: 'Soul', family: 'gospel', desc: 'Accordi ricchi, estensioni e passaggi gospel',
      def: { complexity: 0.55, intensity: 0.5, fills: 0.5, lh: 'octaves', rhVoicing: 'rich', rhMove: 'comp', range: 0.55, ext: 0.75, pedal: 0.5, swing: 0.15, swingGrid: 16 } },
    { id: 'bebo', name: 'Bebo', genre: 'Jazz', family: 'jazz', desc: 'Comping sincopato con voicing drop-2 e shell',
      def: { complexity: 0.55, intensity: 0.4, fills: 0.3, lh: 'shell', rhVoicing: 'drop2', rhMove: 'comp', range: 0.5, ext: 0.7, pedal: 0.1, swing: 0.67, swingGrid: 8 } },
    { id: 'rina', name: 'Rina', genre: 'R&B', family: 'rnb', desc: 'Neo-soul: accordi estesi, spezzati e pigri',
      def: { complexity: 0.5, intensity: 0.45, fills: 0.4, lh: 'root5', rhVoicing: 'rich', rhMove: 'comp', range: 0.55, ext: 0.8, pedal: 0.6, swing: 0.35, swingGrid: 16 } },
    { id: 'bea', name: 'Bea', genre: 'Rock', family: 'rock', desc: 'Accordi pieni a ottavi, ottave alla sinistra',
      def: { complexity: 0.45, intensity: 0.65, fills: 0.3, lh: 'octaves', rhVoicing: 'octave', rhMove: 'pulse', range: 0.45, ext: 0, pedal: 0.1 } },
    { id: 'ettore', name: 'Ettore', genre: 'Pop', family: 'pop', desc: 'Accordi ritmici a ottavi, voicing compatti',
      def: { complexity: 0.45, intensity: 0.55, fills: 0.3, lh: 'octaves', rhVoicing: 'triad', rhMove: 'pulse', range: 0.5, ext: 0.15, pedal: 0.2 } },
    { id: 'clara', name: 'Clara', genre: 'Pop', family: 'ballad', desc: 'Ballad: arpeggi alla sinistra, accordi tenuti alla destra',
      def: { complexity: 0.35, intensity: 0.4, fills: 0.3, lh: 'broken', rhVoicing: 'close', rhMove: 'sustain', range: 0.5, ext: 0.3, pedal: 0.8 } },
    { id: 'lou', name: 'Lou', genre: 'Funk', family: 'funk', desc: 'Stab corti e sincopati, molto spazio',
      def: { complexity: 0.6, intensity: 0.55, fills: 0.3, lh: 'off', rhVoicing: 'shell', rhMove: 'comp', range: 0.55, ext: 0.6, pedal: 0, swing: 0.12, swingGrid: 16 } },
    { id: 'pixel', name: 'Pixel', genre: 'House', family: 'house', desc: 'Piano/organo house: accordi sincopati in levare',
      def: { complexity: 0.5, intensity: 0.6, fills: 0.2, lh: 'off', rhVoicing: 'close', rhMove: 'comp', range: 0.55, ext: 0.5, pedal: 0, swing: 0.25, swingGrid: 16 } },
    { id: 'deep', name: 'Deep', genre: 'Tech House', family: 'techhouse', desc: 'Stab di accordi minori settima, corti e ripetuti',
      def: { complexity: 0.45, intensity: 0.55, fills: 0.1, lh: 'off', rhVoicing: 'shell', rhMove: 'comp', range: 0.5, ext: 0.6, pedal: 0, swing: 0.35, swingGrid: 16 } },
    { id: 'kraft', name: 'Kraft', genre: 'Techno', family: 'techno', desc: 'Un solo stab ipnotico ripetuto, voicing a quarte',
      def: { complexity: 0.4, intensity: 0.6, fills: 0.05, lh: 'off', rhVoicing: 'quartal', rhMove: 'comp', range: 0.45, ext: 0.4, pedal: 0, swing: 0.05, swingGrid: 16 } },
    { id: 'dub', name: 'Dub', genre: 'Microhouse / Rominimal', family: 'microhouse', desc: 'Accordi dub sparsi, code lunghe, sincopi',
      def: { complexity: 0.45, intensity: 0.45, fills: 0.1, lh: 'off', rhVoicing: 'rich', rhMove: 'comp', range: 0.5, ext: 0.7, pedal: 0.4, swing: 0.4, swingGrid: 16 } }
  ];

  // Presets: per-persona parameter overrides, like the per-player presets in the editor.
  var PRESET_SHAPES = [
    { name: 'Strofa', o: { complexity: -0.1, loudness: -0.1, intensity: -0.1, fills: -0.1 } },
    { name: 'Ritornello', o: { complexity: 0.1, loudness: 0.15, intensity: 0.15, fills: 0.1 } },
    { name: 'Morbido', o: { complexity: -0.2, loudness: -0.3, intensity: -0.3, fills: -0.2 } },
    { name: 'Pieno', o: { complexity: 0.25, loudness: 0.3, intensity: 0.3, fills: 0.25 } },
    { name: 'Essenziale', o: { complexity: -0.35, loudness: 0, intensity: 0, fills: -0.3 } }
  ];

  function presetsFor(persona) {
    var out = [{ name: 'Standard', params: copy(persona.def) }];
    for (var i = 0; i < PRESET_SHAPES.length; i++) {
      var p = copy(persona.def), o = PRESET_SHAPES[i].o;
      for (var k in o) if (o.hasOwnProperty(k) && p.hasOwnProperty(k)) p[k] = BM.groove.clamp01(p[k] + o[k]);
      out.push({ name: PRESET_SHAPES[i].name, params: p });
    }
    return out;
  }

  function copy(o) { var r = {}; for (var k in o) if (o.hasOwnProperty(k)) r[k] = o[k]; return r; }

  function find(list, id) {
    for (var i = 0; i < list.length; i++) if (list[i].id === id || list[i].name === id) return list[i];
    return list[0];
  }

  BM.styles = { GENRES: GENRES, ELECTRONIC_GENRES: ELECTRONIC_GENRES, DRUMMERS: DRUMMERS, BEATMAKERS: BEATMAKERS, BASSISTS: BASSISTS, KEYBOARDISTS: KEYBOARDISTS, presetsFor: presetsFor,
    find: find, copy: copy };
})(BM);
