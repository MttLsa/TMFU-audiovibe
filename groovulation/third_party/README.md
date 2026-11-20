Third-party code (MIT licence, see the LICENSE.txt files):

- `signalsmith-stretch/` — Signalsmith Stretch 1.3.2 (polyphonic pitch/time stretching), https://github.com/Signalsmith-Audio/signalsmith-stretch
- `signalsmith-linear/` — Signalsmith Linear 0.6.6 (FFT/STFT used by Stretch), https://github.com/Signalsmith-Audio/linear

The only local change: the `#include` of `stft.h` in `signalsmith-stretch.h` points to the vendored copy.
