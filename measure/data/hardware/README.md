# Hardware captures (not included)

This directory receives the reference-instrument recordings described in `docs/measurement_protocol.md`.
They are **not** in the repository until a Random*Source Tau Phaser has been measured. Licence for data
placed here: CC-BY 4.0 (state the instrument, edition, serial and capture chain in `PROVENANCE.md`).

Expected layout (96 kHz / 24-bit, DC-coupled interface):

```
sweeps/sweep.wav                        the 8 s exponential sweep that was played (notch_map.py --make-sweep)
sweeps/normal_cv{-3.0..+5.0}.wav        NORMAL output per CV step (PITCH centred, Depth 0, Resonance 0)
sweeps/alternative_cv{...}.wav          ALTERNATIVE output
sweeps/normal_color_cv0.0.wav           COLOR engaged
resonance/normal_res{25,50,75,90,100}_cv0.0.wav
oscillation/osc_cv{-3..+5}.wav          Resonance 100 %, no input
level/sine1k_{-40..+6}dBu.wav           1 kHz sine responses, INPUT fixed
level/sweep_{-40..+6}dBu.wav            sweep responses at each level (first-notch shift)
lfo/noise_rate{min,mid,max}_{hard,smooth}.wav   60 s white-noise responses, Depth 100 %
programme/{drums,pad,noise}_{normal,alternative}_{color0,color1}.wav
results/                                JSON written by the measure scripts from these files (compare.py reads it)
```

Then: `python measure/notch_map.py --in sweeps/sweep.wav --out sweeps/normal_cv0.0.wav --json results/...` etc.,
and `python measure/compare.py` for the acceptance table.
