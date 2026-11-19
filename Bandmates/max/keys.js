// Bandmates Keys - keyboard player device (follows "Bandmates Chords")
var DEV = { id: 'keys', personas: BM.styles.KEYBOARDISTS, needs: { chords: true },
  build: function (p) { return BM.keys.generate(p); } };
