// Bandmates engine - seeded random numbers (ES5: runs in the Max `js` object and in Node)
(function (BM) {
  'use strict';

  // Math.imul is missing in the old SpiderMonkey of Max's js object
  function imul(a, b) {
    var ah = (a >>> 16) & 0xffff, al = a & 0xffff, bh = (b >>> 16) & 0xffff, bl = b & 0xffff;
    return ((al * bl) + ((((ah * bl + al * bh) << 16) >>> 0)) | 0);
  }

  function hash(value) {
    var s = String(value), h = 2166136261 >>> 0;
    for (var i = 0; i < s.length; i++) {
      h ^= s.charCodeAt(i);
      h = imul(h, 16777619) >>> 0;
    }
    return h >>> 0;
  }

  // combine any number of numbers/strings into a 32-bit seed
  function mix() {
    var h = 0x9e3779b9;
    for (var i = 0; i < arguments.length; i++) {
      h = (imul(h ^ hash(arguments[i]), 0x85ebca6b) ^ (h >>> 13)) >>> 0;
    }
    return h >>> 0;
  }

  function Rng(seed) { this.s = (seed >>> 0) || 1; }
  Rng.prototype.next = function () { // mulberry32
    var t = (this.s = (this.s + 0x6D2B79F5) >>> 0);
    t = imul(t ^ (t >>> 15), t | 1);
    t ^= t + imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
  Rng.prototype.range = function (a, b) { return a + (b - a) * this.next(); };
  Rng.prototype.int = function (n) { return Math.floor(this.next() * n); };
  Rng.prototype.chance = function (p) { return this.next() < p; };
  Rng.prototype.pick = function (arr) { return arr[this.int(arr.length)]; };
  // roughly normal in [-1, 1]
  Rng.prototype.gauss = function () { return (this.next() + this.next() + this.next() - 1.5) / 1.5; };
  Rng.prototype.weighted = function (weights) {
    var tot = 0, i;
    for (i = 0; i < weights.length; i++) tot += Math.max(0, weights[i]);
    if (tot <= 0) return 0;
    var r = this.next() * tot;
    for (i = 0; i < weights.length; i++) {
      r -= Math.max(0, weights[i]);
      if (r < 0) return i;
    }
    return weights.length - 1;
  };

  BM.rng = {
    Rng: Rng,
    hash: hash,
    mix: mix,
    make: function () { return new Rng(mix.apply(null, arguments)); }
  };
})(BM);
