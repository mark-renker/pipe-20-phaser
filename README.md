# Pipe-20 Phaser

An independent, non-commercial behavioural model inspired by the Tau "The Pipe" phaser and Jürgen Haible's redesign;
not affiliated with or endorsed by Random\*Source or Tau Systems.

A 20-stage transistor-ladder all-pass phaser with 1 V/oct pitch control, resonance up to and through self-oscillation,
a triangle LFO with HARD/SMOOTH and slow modes, complementary NORMAL / ALTERNATIVE outputs for pseudo-stereo, COLOR
(wet inversion) and a Phase/Vibrato mode. Built to the engineering spec in `docs/SPEC_Tau_Pipe_Phaser_Emulation.pdf`;
fidelity is defined by the measurements in its Section 8, run by the scripts in `measure/`.

Formats: AU, VST3, CLAP, standalone app (JUCE 8), plus a JUCE-free `pipe20_render` WAV→WAV renderer for the
measurement scripts. Licence: MIT for the code and data (see `LICENSE`; it does not cover the hardware designs).

## Install (no build needed)

Grab the latest files from the **[Releases page](../../releases/latest)**:

* **macOS** (11 or later, Apple Silicon and Intel): download `Pipe20Phaser-<version>-macOS.pkg` and open it. If
  Gatekeeper objects (the installer is not notarized), right-click the file and choose *Open*. Tick the formats you
  want (AU, VST3, CLAP, standalone app); they go to `/Library/Audio/Plug-Ins` and `/Applications`. Rescan plug-ins in
  your DAW. Prefer not to run an installer? `Pipe20Phaser-<version>-macOS-bundles.zip` has the raw bundles and a
  manual-install note.
* **Windows** (x64): download `Pipe20Phaser-<version>-Windows-x64.zip`, unzip, copy `Pipe-20 Phaser.vst3` to
  `C:\Program Files\Common Files\VST3\` and `Pipe-20 Phaser.clap` to `C:\Program Files\Common Files\CLAP\`.

The plug-in shows up under **Renky Denk › Pipe-20 Phaser**. Every tagged release is built by
`.github/workflows/release.yml`; `packaging/make_installer.sh` makes the same `.pkg` locally.

## Status (6 Nov 2026)

* DSP core complete, 65 unit checks pass (`tests/UnitTests.cpp`), AU/VST3 pass `auval` and pluginval strictness 8.
* **No hardware has been measured yet.** Every behavioural constant is a nominal default in
  `resources/tunables.json` (derived from the spec and published build notes), and `docs/results.md` reports what the
  *model* does. The Section 8 acceptance items read "not measured" until the captures in `docs/measurement_protocol.md`
  exist; `measure/compare.py` then fills the table and the tunables are re-fitted from the scripts' output.
* Random\*Source has not yet been asked about the "Haible" / "Tau Pipe" names, hence the descriptive name. Their reply
  will be recorded here.

## Build from source

```
# core only (renderer + tests, no JUCE): Linux/macOS/Windows
cmake -B build-core -DPIPE20_BUILD_PLUGINS=OFF && cmake --build build-core -j && ./build-core/pipe20_tests

# plug-ins (JUCE 8.0.6 from ~/JUCE if present, else fetched; CLAP via clap-juce-extensions)
cmake -B build -DCMAKE_BUILD_TYPE=Release          # add -DPIPE20_ARCHS=arm64 for a faster native-only macOS build
cmake --build build -j
# bundles land in build/Pipe20Phaser_artefacts/Release/{AU,VST3,CLAP,Standalone}
```

Measurements (Python 3.10+, `pip install -r measure/requirements.txt`):

```
python measure/run_all.py                 # renders every model measurement -> docs/results/ + docs/results.md
python measure/run_all.py --check-baseline  # CI regression check against measure/baseline/model_baseline.json
python measure/notch_map.py --in sweep.wav --out captured.wav   # any script also takes a WAV pair
```

Tunables can be overridden per machine without rebuilding: copy `resources/tunables.json` to
`~/Library/Application Support/Pipe20Phaser/tunables.json` (macOS; `%APPDATA%/Pipe20Phaser/` on Windows) and press
*Reload tunables* in the plug-in, or pass `--tunables file.json` / `--set key=value` to the renderer.

## Parameters (real units, as the host sees them)

| Parameter | Range | Hardware |
|---|---|---|
| Pitch | 20 Hz – 20 kHz (f_c of the stages, exact at static settings) | PITCH |
| Resonance | 0 – 100 % of loop gain; self-oscillation above 100 / `feedback.gain_max` (85 % nominal) | FEEDBACK / RESONANCE |
| LFO Rate | 0.1 – 25 Hz log, L\* slow ÷ 10 (nominal) | RATE, L\* |
| LFO Depth | 0 – 100 % crossfade PITCH+CV ↔ LFO | DEPTH |
| LFO Smooth | HPF on the LFO CV (0.5 Hz nominal) | HARD/SMOOTH |
| Color | inverts the wet signal on NORMAL | NORMAL/COLOR |
| Output | Normal / Alternative / Stereo (L = Normal, R = Alternative) | both jacks |
| Mode | Phase / Vibrato (dry path removed) | Phase/Vibe |
| Stages | 12 / 16 / 18 / 20 (experiments; 20 default) | PD-10 count |
| CV Attenuverter | −100 … +100 % on the second CV input | CV ATT |
| Input | −∞ … +6 dB | INPUT |
| Drive / Matching | 0 = ideal matched linear stages … 1 = modelled mismatch + junction nonlinearity | array choice |
| Oversampling | Auto / 1× / 2× / 4× / 8× engine rate | — |

CV: the plug-in has a second stereo input bus ("CV In"): channel 1 = 1 V/oct, channel 2 = attenuverted; 1.0 = 10 V
(`cv.fullscale_volts`). Audio input is summed to mono (the hardware is mono in). Latency reported: 0 samples.

## Measured fidelity

Model-only numbers (hardware columns pending) are in **[docs/results.md](docs/results.md)**. Summary of the model with
its nominal tunables, 96 kHz:

| Item (spec 8) | Target vs hardware | Model (self-consistency) |
|---|---|---|
| 8.1 notch map | ≤ 2 % per notch, count exact | notches on the f_c·tan((2k+1)π/2N) law within 0.6 % at 4× engine rate; 10 notches for N = 20 |
| 8.2 control law | ≤ 3 % | the scripts recover the built-in law (1.002 V/oct, 1/x knee 61 kHz vs 60 kHz set) |
| 8.3 self-oscillation | onset 5 %, ±25 cents, harmonics 3 dB | onset at 93 %; mode 1 (0.158 f_c) oscillates deterministically; tracks 1 V/oct within 37 cents over CV −1…+1 V, then flattens with the nominal 1/x law (−125 cents at +3 V) |
| 8.4 level dependence | 20 % of the measured shift | first notch moves +2.4 % from −40 to 0 dBFS (nominal a1/a2) |
| 8.5 outputs | complementary / exact inversion | ALTERNATIVE peaks at NORMAL notches; COLOR == −wet sample-exact (unit test) |
| 8.6 LFO | rate 5 %, corner 20 % | rate recovered to 0.1 % at 0.1 / 1.5 / 25 Hz; triangle residual 1.3 % |
| THD+N | plot vs hardware | 0.03 % at −40 dBFS … 3.7 % at 0 dBFS (1 kHz, nominal nonlinearity) |
| aliasing | none above −80 dBFS | none at 10 / 15 kHz, 0 dBFS, static, 4× engine |
| CPU | report | ≈ 4 % of one Apple-silicon core at 48 kHz / 64 samples (4×), 0.9 % at 1× |
| null test | < −30 dB static | block-size independence −400 dB; hardware: not measured |

How the engine rate was chosen (spec 6) and every other modelling decision: `docs/DEVIATIONS.md`.
Block diagram: `docs/block_diagram.md`. Protocol for the hardware captures: `docs/measurement_protocol.md`.

## Repository layout

```
src/dsp/        JUCE-free core: Phaser.h (engine), Tunables.h, OversampleFilters.h, WavFile.h, TinyJson.h
src/plugin/     JUCE processor + plain editor
tools/render.cpp   pipe20_render WAV->WAV (all parameters as flags, --cv file, --bench)
tests/          unit tests (stage phase, notch map, feedback stability, LFO, oversampler, tunables, outputs)
measure/        Python: notch_map, control_law_fit, thd_n, osc_pitch, lfo_measure, null_test, aliasing, cpu_bench,
                compare (acceptance table), run_all (+ baseline regression), design_os_filters
docs/           spec PDF, block diagram, measurement protocol, DEVIATIONS, results.md + results/
resources/      tunables.json (nominal values + notes)
.github/        ci.yml (core + measurements vs baseline), release.yml (tagged builds)
```

## Credits, lineage and licensing

* **Jürgen Haible (1961–2011)** — designer of the 2007 redesign on which every modern Tau Pipe is based:
  <http://jhaible.com/legacy/tau/jh_tau.html>.
* **Dennis Colin and Ken McNeill / Tau Systems (Newton, NH)** — designers and makers of the original Tau "The Pipe"
  (developed 1975, released ≈ 1980).
* **Random\*Source** (serge-modular.com, randomsource.net) holds the rights to Haible's electronic designs and is the only
  legitimate source for them. This repository contains no Random\*Source or Haible schematic, BOM, PCB file, panel
  artwork or documentation; it cites and links them only. Their documentation is for personal/educational use and is
  not mirrored here.
* **ARP / Aries lineage**: Haible cited the ARP Quadra phaser as related; US patent 3,969,682 "Circuit for dynamic
  control of phase shift" (1974, expired) describes the current-controlled all-pass stage:
  <https://patents.google.com/patent/US3969682>.
* **Community documentation**: Bill and Will Hall (Dragonfly Alley) for the construction notes and US BOM
  (<https://www.dragonflyalley.com/synth/constructionJHTauPipe.htm>); the electro-music.com DIY thread (2007 onward,
  <https://electro-music.com/forum/post-121980.html>); Scott Stites for the historical and PD-10 notes;
  keyboardmuseum.org for preserving the "Pipe Phase Demo Record"; the PedalPCB build thread for the stompbox variant
  (<https://forum.pedalpcb.com/posts/377038/>); DSL-man's MOTM build log (<https://www.dsl-man.de/x/oQB4>);
  Matrixsynth's 2024 and 2026 posts.
* **Prior DSP work**: Zavalishin, *The Art of VA Filter Design* (TPT one-pole sections, zero-delay feedback);
  Huovilainen (2004) and Pirkle on nonlinear ladder models; Farina (2000) exponential-sweep deconvolution used by the
  measurement scripts.
* Code: MIT (Mark Renker, 2026). The licence covers this code and its measurement data only, not the hardware designs
  the model was measured against. Measurement data under `measure/data/` is CC-BY 4.0.
