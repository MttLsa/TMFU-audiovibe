// Bandmates Drums - acoustic drummer device
var DEV = { id: 'drums', personas: BM.styles.DRUMMERS, needs: { follow: true },
  build: function (p) { return BM.drummer.generate(p); } };
