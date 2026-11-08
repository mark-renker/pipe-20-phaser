# Modelling choices, deviations from the spec text, and open questions

Everything below is a tunable in `resources/tunables.json` unless stated otherwise; nothing is baked in.

## Engine rate: 4× by default, not "2× only when nonlinear"
Spec 6 asks for 2× oversampling only when the nonlinearity is engaged and none otherwise, while spec 8.1 demands every notch
below 20 kHz within 2 % of hardware. A first-order TPT all-pass matches the analogue phase only up to the bilinear warp:
a notch the analogue chain puts at f lands at (f_e/π)·atan(tan(πf_c/f_e)·tan(θ)) — about (πf/f_e)²/3 low. Measured on
the model (PITCH 2 kHz, `engine_study.json`): 1× fails badly, 2× misses the top notches by more than 2 %, 4× keeps the
worst notch within ~1 % and 8× within ~0.3 %. The default is therefore `oversample.linear_factor = 4` and
`oversample.nonlinear_factor = 4` (no switching, so no transient on the Resonance threshold). Set both to 1 to reproduce
the spec's "none otherwise" and compare CPU (`measure/cpu_bench.py`). The spec's measure-and-pick instruction for the
coefficient update block is honoured the same way (`control.coef_block`, residual reported in `docs/results.md`).

## Oversampling filters
Elliptic IIR (9th/5th/3rd order per doubling stage, 0.01 dB ripple, 90 dB stop), minimum phase, no look-ahead:
algorithmic latency 0, reported to the host as 0. They do have a low-frequency group delay of ~3/5/5.6 samples at 48 kHz
for 2×/4×/8× (unit test T5). This is phase delay, not latency, and dry and wet pass through it together so notch
positions are unaffected.

## Feedback loop
Solved as a zero-delay loop: the all-pass chain is affine in its input for one sample (y = A·u + B), the tanh limiter is
linearised at the previous sample's operating point, and the implicit equation is solved in closed form. No unit delay,
so no pole shift to compensate. The one-sample lag that remains is inside the level-dependent pole term only (it uses
the previous sample's stage inputs). The loop contains a high-pass and a low-pass. With equal gain at every loop mode f_c·tan(mπ/N) (what an ideal all-pass
chain gives) the oscillating mode is decided by the start-up transient and hops with CV, so the pitch would not track.
To make the model deterministic and 1 V/oct-tracking (Haible: it tracks in self-oscillation) both loop corners follow
f_c by default: low-pass at `feedback.lp_ratio` × f_c (0.7) favours mode 1 (f = 0.158 f_c, ~7 % gain margin over
mode 2), high-pass at max(`feedback.hp_hz`, `feedback.hp_ratio` × f_c) suppresses the near-DC mode that AC coupling
alone lets through at high f_c. Set both ratios to 0 for fixed corners (`feedback.hp_hz`, `feedback.lp_hz`). The tilt
these corners put on the resonance peaks below onset is measurable in the Resonance 25–90 % sweeps (protocol 2) and is
how the ratios get fitted. The onset is at loop gain 1 at the winning mode, i.e. slightly above 100 / `feedback.gain_max`
because of the corner losses (model: 93 % with gain_max 1.18). Sign of the feedback (`feedback.sign`): +1 (peaks
resonate) by default — open question 10.2.

## Level dependence
The junction term ω_i = ω₀·(1 + a1·u + a2·u²) (u = stage input / `ladder.nl_vref`) stands in for r_e = V_T/I. a1 gives
odd/even distortion and "squash", a2 the average pole shift with level (the sign of the first-notch shift is unknown
until measured; the default moves it down). Defaults were set so the model produces ~0.8 % THD+N at −12 dBFS and ~4 %
at 0 dBFS (10 Vpk) at a 1 kHz / 2 kHz setting — plausible for a hot Euro signal, not measured.

## Depth crossfade, LFO
Depth is a crossfade between PITCH+CV and the LFO voltage (spec 3), so at Depth 100 % PITCH has no effect; the LFO's own
centre and swing are `lfo.centre_volts` / `lfo.swing_volts` (default ±3 V = 6 octaves). SMOOTH is a one-pole HPF on the
LFO CV at `lfo.smooth_hp_hz`. Rate range and slow divisor are nominal.

## ALTERNATIVE output
`mix.alt_mode = 0`: dry − wet (the sign-flip derivation, peaks where NORMAL has notches). `mix.alt_mode = 1`: dry + the
output of stage `mix.alt_tap_stage`. Open question 10.3 — the sweep set (protocol 6) decides.

## Not modelled
Power supply, CV mixer add-on, panel. Stage count N is a build constant exposed as a choice (12/16/18/20) for experiments
only; the default 20 follows Haible's page and the 2026 edition; the Euro listing's "18-pole" is to be confirmed by
counting notches (8.1).

## Naming
"Pipe-20 Phaser" is used pending a reply from Random*Source about the use of "Haible" / "Tau Pipe" (spec 9). Their reply,
when it arrives, goes in the README.
