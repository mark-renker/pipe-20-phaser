# Block diagram

```
                 INPUT                      feedback sum                                    20-stage all-pass ladder
 audio in ──► attenuator ──► x ──►(+)──► u ──► AP1 ─► AP2 ─► … ─► AP_tap ─► … ─► AP_N ──► wet (y)
 (L+R)/2    (-inf..+6 dB)      │   ▲                      │ (all stages share one pole f_c(t);          │
                               │   │                      │  per-stage offset vector + junction        │
                               │   │                      │  r_e = V_T/I level term, Drive/Match)      │
                               │   │                                                                   │
                               │   └──── × k·sign ◄── tanh limiter ◄── loop LP ◄── loop HP ◄───────────┤
                               │        (Resonance 0..100 % → loop gain 0..gain_max; onset at gain 1)  │
                               │                                                                        │
                               ├──────────────► NORMAL      = x + wet      (COLOR: x − wet)            │
                               ├──────────────► ALTERNATIVE = x − wet      (or x + tap, mix.alt_mode)  │
                               └──────────────► VIBRATO     = wet only (dry path removed)              ▼

 control:   PITCH (Hz → V by inverting the law)  +  1 V/oct CV  +  attenuverter × CV2
            └───────────────── (1 − Depth) ─────────────┐
            LFO: triangle, RATE (0.1..25 Hz, L* ÷ 10) ──(Depth)──(+)──► smoothing (2 ms) ──► expo 2^(V/1V) ──► 1/x compression
                 SMOOTH = one-pole HPF on the LFO CV                           f_expo = f_ref·2^V      f_c = floor + f_expo·f_sat/(f_expo+f_sat)
                                                                                                        └──► g = tan(π f_c / f_engine) → all stages

 engine rate = host rate × {1,2,4,8}: zero-latency elliptic IIR up/down-sampling (src/dsp/OversampleFilters.h).
 The feedback loop is solved without a unit delay: the ladder is linearised per sample (y = A·u + B), the limiter
 around its previous operating point, and the implicit equation solved in closed form (src/dsp/Phaser.h, Engine::tick).
```

Each first-order all-pass stage is the topology-preserving-transform (TPT) section `lp = s + G (x − s); s = 2 lp − s; ap = 2 lp − x`
with `G = g / (1 + g)`. Its phase is −2·atan(tan(πf/f_e)/g): exactly −90° at f_c, and the analogue −2·atan(f/f_c) elsewhere
up to the bilinear warp (quantified in `DEVIATIONS.md`). With N stages the NORMAL sum has notches where the chain phase is an odd
multiple of π: `f_k = f_c · tan((2k+1)π / 2N)`, k = 0 … N/2 − 1 — ten notches for N = 20, which is how N is confirmed from a
measured response (spec 8.1).
