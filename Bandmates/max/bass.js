// Bandmates Bass - bass player device (follows "Bandmates Chords" and the drummer's kick)
var DEV = { id: 'bass', personas: BM.styles.BASSISTS, needs: { chords: true, kick: true },
  build: function (p) { return BM.bass.generate(p); } };
