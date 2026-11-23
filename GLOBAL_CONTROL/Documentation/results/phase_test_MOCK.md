# GLOBAL CONTROL - PoC phase test — MOCK LOM (Node fixture, NOT Ableton Live)

2026-11-19 09:40, device `Pro-C 2`, parameter `Threshold`

| Step | Result | Detail |
|---|---|---|
| Bridge handshake | PASS | Live 12.1.5, bridge 0.1.0, project  |
| Phase 2 - instances found | PASS | 10 instances of Pro-C 2 on 7 tracks (scan 13 ms) |
| Phase 3 - Threshold readable | PASS | Kick=-12.00 dB, Kick=-6.00 dB, Snare=-8.00 dB, Bass=-15.00 dB, Vocal=-10.00 dB, Vocal=-20.00 dB, Drum Bus=-30.00 dB, Drum Bus=-3.00 dB, Synth=-18.00 dB, Reverb=-24.00 dB |
| Scale analysis | PASS | kind=linear unit='dB', 65 numeric samples |
| Phase 5/6 - absolute -12 dB on all | PASS | 10/10 confirmed in 30 ms: ['-12.00 dB'] |
| Phase 7 - relative -2 dB preserves differences | PASS | -12->-14, -6->-8, -8->-10, -15->-17, -10->-12, -20->-22, -30->-32, -3->-5, -18->-20, -24->-26 |
| Restore original values | PASS | 10/10 restored |

**ALL PASSED**
