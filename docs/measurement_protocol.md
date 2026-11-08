# Measurement protocol (reference hardware)

Reference: Random*Source "Haible TAU Phaser" (Eurorack, 2016/2021) or "TAU Phaser 'The Pipe' 2026", ±12 V supply,
DC-coupled interface (Expert Sleepers ES-8/ES-9 class) at 96 kHz / 24-bit. Record both NORMAL and ALTERNATIVE for
every capture. Keep the module's INPUT at a marked, repeatable position and note it. Place everything in
`measure/data/hardware/` (layout in its README) and write a `PROVENANCE.md` (edition, serial, supply, interface, levels).

Level reference: sample value 1.0 = `io.volts_at_fullscale` (default 10 Vpk = +19.2 dBu). Calibrate the interface so that
a known DC or 1 kHz level reads correctly and record the calibration in PROVENANCE.md.

| # | Capture | Settings | Script | Spec |
|---|---------|----------|--------|------|
| 1 | Sweep set | `notch_map.py --make-sweep sweep.wav --fs 96000` played at −12 dBFS; Depth 0, Resonance 0, PITCH centred; CV −3 … +5 V in 0.5 V steps; repeat at PITCH min and max | `notch_map.py --in --out` per file → `control_law_fit.py --points` | 8.1, 8.2 |
| 2 | Resonance set | as 1 at CV 0, Resonance 25/50/75/90/100 % | `notch_map.py` (peak/notch depth), `osc_pitch.py --out` at 100 % | 8.3 |
| 3 | Self-oscillation | no input, Resonance 100 %, CV −3 … +5 V in 1 V steps | `osc_pitch.py --out` | 8.3 |
| 4 | Level series | 1 kHz sine −40 … +6 dBu in 5 dB steps, static mid setting; plus the sweep at each level | `thd_n.py --in --out`, `notch_map.py` | 8.4 |
| 5 | LFO | 2 kHz sine probe (−12 dBFS) 60 s at RATE min/mid/max, Depth 100 %, HARD and SMOOTH; plus `p20.periodic_noise` 60 s at RATE min | `lfo_measure.py --rate-wav / --shape-wav` | 8.6 |
| 6 | Outputs | sweep at CV 0 through NORMAL, ALTERNATIVE, and with COLOR | `notch_map.py` on each | 8.5 |
| 7 | Programme | 30 s drums, 30 s pad, 30 s noise through each output and COLOR setting, static settings written down | `null_test.py reference.wav model.wav` after rendering the same input with `pipe20_render` | 8 null test |

Then:

```
python measure/compare.py --model docs/results --hardware measure/data/hardware/results
```

prints the acceptance table (8.1–8.6). Re-tune `resources/tunables.json` (control law from `control_law_fit.py`,
`feedback.gain_max` from the onset, `ladder.nl_*` from the THD/notch-shift curves, `lfo.*` from the LFO fit, per-stage
offsets from the notch depths) and rerun `measure/run_all.py --write-baseline`.
