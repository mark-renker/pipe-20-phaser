#!/usr/bin/env python3
"""LFO measurement (spec 5, 8.6).

Excitation for the shape: periodic multisine (p20.periodic_noise, period 4096 samples) - sounds like noise, but
every 4096-sample frame has an exactly flat spectrum, so the notch set is read per frame without averaging.

Rate: a fixed sine probe (2 kHz) through the device with Depth = 100 %; the notches sweep across the probe and
      modulate its level periodically. The LFO period is the first strong peak of the envelope autocorrelation.
Shape: white-noise excitation; per STFT frame the notch set is fitted to f_c, converted to control volts
      (1 V/oct), and compared with an ideal triangle (and, for SMOOTH, a one-pole high-passed triangle whose
      corner is fitted).

  lfo_measure.py --rate-wav probe_response.wav [--probe-hz 2000]            -> rate
  lfo_measure.py --shape-wav noise_response.wav [--fc-ref 440] [--smooth]   -> swing, triangle residual, corner
  lfo_measure.py --model [--rates 0.1 1.5 25]
"""
import argparse, json
from pathlib import Path
import numpy as np
from scipy import signal
import p20

def rate_from_probe(y, fs, probe_hz, rate_min=0.05, rate_max=40.0):
    sos = signal.butter(4, [probe_hz * 0.9, probe_hz * 1.1], btype="band", fs=fs, output="sos")
    env = np.abs(signal.hilbert(signal.sosfiltfilt(sos, y)))
    dec = max(1, int(fs / 2000)); env = env[::dec]; fe = fs / dec          # 2 kHz envelope rate
    env = env - env.mean(); n = len(env)
    ac = signal.correlate(env, env, mode="full")[n - 1:] / (n - np.arange(n))   # unbiased
    ac /= ac[0]
    lo, hi = int(fe / rate_max), min(n - 1, int(fe / rate_min))
    pk, _ = signal.find_peaks(ac[:hi + 1], distance=lo)
    pk = [p for p in pk if p >= lo and ac[p] > 0.8]
    if not pk: return float("nan")
    return float(fe / pk[0])

def shape_from_noise(y, fs, fc_ref=440.0, stages=20, period=4096, min_inliers=4):
    """f_c per excitation period (periodic multisine, rectangular frames = exact flat excitation), robust notch fit."""
    n = len(y) // period
    frames = y[: n * period].reshape(n, period)
    db = 20 * np.log10(np.abs(np.fft.rfft(frames, axis=1)) + 1e-12)
    f = np.fft.rfftfreq(period, 1 / fs)
    t = (np.arange(n) + 0.5) * period / fs
    v = []
    for col in db:
        notches = p20.find_notches(f, col, fmin=40.0, prominence=5.0, smooth_oct=1.0 / 24)
        fc, inl, tot = p20.fit_fc_robust([x[0] for x in notches], stages) if len(notches) >= 3 else (np.nan, 0, 0)
        v.append(np.log2(fc / fc_ref) if np.isfinite(fc) and fc > 0 and inl >= min_inliers else np.nan)
    v = np.array(v); ok = np.isfinite(v)
    return t[ok], v[ok]

def fit_triangle(t, v, rate_guess=None, smooth=False):
    v = v - np.median(v)
    swing = (np.percentile(v, 99) - np.percentile(v, 1)) / 2
    dt = float(np.min(np.diff(t))); tu = np.arange(t[0], t[-1] + dt / 2, dt)   # uniform grid (tracking may have gaps)
    rates = [rate_guess] if rate_guess else list(np.geomspace(0.05, 30, 400))
    best = (1e9, None, None, None)
    corners = [None] if not smooth else list(np.geomspace(0.05, 5.0, 40))
    for rate in rates:
        for corner in corners:
            for ph in np.linspace(0, 1, 256, endpoint=False):
                x = (tu * rate + ph) % 1.0; tri = np.where(x < 0.5, 4 * x - 1, 3 - 4 * x)
                if corner:
                    a = np.exp(-2 * np.pi * corner * dt); tri = signal.lfilter([1, -1], [1, -a], tri, zi=[-tri[0]])[0] * (1 + a) / 2
                    tri = tri - np.median(tri[len(tri) // 4:])
                tri = np.interp(t, tu, tri)
                sw = np.dot(tri, v) / (np.dot(tri, tri) + 1e-12)
                r = float(np.sqrt(np.mean((v - sw * tri) ** 2)) / max(swing, 1e-6))
                if r < best[0]: best = (r, rate, corner, sw)
    return dict(triangle_rms_residual_fraction=round(best[0], 4), rate_hz_from_shape=round(float(best[1]), 4),
                smooth_corner_hz=round(float(best[2]), 3) if best[2] else None, swing_volts=round(float(swing), 4), template_gain=round(float(best[3]), 4))

if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--rate-wav"); ap.add_argument("--probe-hz", type=float, default=2000); ap.add_argument("--shape-wav"); ap.add_argument("--smooth", action="store_true")
    ap.add_argument("--channel", type=int, default=0); ap.add_argument("--fc-ref", type=float, default=440); ap.add_argument("--json"); ap.add_argument("--plot")
    ap.add_argument("--model", action="store_true"); ap.add_argument("--out-dir", default=str(p20.RESULTS))
    ap.add_argument("--rates", type=float, nargs="*", default=[0.1, 1.5, 25.0]); ap.add_argument("--dur", type=float, default=30.0)
    a = ap.parse_args()
    if a.model:
        out_dir = Path(a.out_dir); tmp = out_dir / "tmp"; tmp.mkdir(parents=True, exist_ok=True)
        fs = 48000; rows = []; curves = []
        for rate in a.rates:
            for smooth in (0, 1):
                dur = max(a.dur, 3 / rate)
                p20.write_wav(tmp / "probe.wav", fs, p20.sine(fs, dur, a.probe_hz, -12))
                p20.render(tmp / "probe.wav", tmp / "probe_out.wav", pitch=440, resonance=0, depth=100, rate=rate, smooth=smooth, os="auto", output="stereo")
                _, y = p20.read_wav(tmp / "probe_out.wav")
                r = dict(rate_set_hz=rate, smooth=bool(smooth), rate_hz=round(rate_from_probe(y[0], fs, a.probe_hz), 4))
                if rate <= 0.2:   # shape needs a slow sweep so each 0.25 s window sees a quasi-static notch set
                    p20.write_wav(tmp / "noise.wav", fs, p20.periodic_noise(fs, dur))
                    p20.render(tmp / "noise.wav", tmp / "noise_out.wav", pitch=440, resonance=0, depth=100, rate=rate, smooth=smooth, os="auto", output="stereo")
                    _, yn = p20.read_wav(tmp / "noise_out.wav"); t, v = shape_from_noise(yn[0], fs, a.fc_ref)
                    r.update(fit_triangle(t, v, rate_guess=r["rate_hz"] if np.isfinite(r["rate_hz"]) else None, smooth=bool(smooth)))
                    if rate == min(a.rates): curves.append((f"{rate:g} Hz {'SMOOTH' if smooth else 'HARD'}", t, v - np.median(v)))
                rows.append(r); print(json.dumps(r))
        p20.dump(dict(rows=rows), out_dir / "lfo.json")
        fig, ax = p20.fig_ax("LFO waveform recovered from the notch trajectory (Depth 100 %)", "time (s)", "control (V, 1 V/oct)")
        for i, (lab, t, v) in enumerate(curves):
            m = t < 2.0 / min(a.rates); ax.plot(t[m], v[m], color=p20.PALETTE[i], linewidth=1.5, label=lab)
        p20.save(fig, out_dir / "lfo.png")
    else:
        out = {}
        if a.rate_wav:
            fs, y = p20.read_wav(a.rate_wav); out["rate_hz"] = rate_from_probe(y[a.channel], fs, a.probe_hz)
        if a.shape_wav:
            fs, y = p20.read_wav(a.shape_wav); t, v = shape_from_noise(y[a.channel], fs, a.fc_ref); out.update(fit_triangle(t, v, out.get("rate_hz"), a.smooth))
            if a.plot:
                fig, ax = p20.fig_ax("LFO waveform (notch trajectory)", "time (s)", "control (V)"); ax.plot(t, v - np.median(v), color=p20.PALETTE[0], linewidth=1.5); p20.save(fig, a.plot)
        if a.json: p20.dump(out, a.json)
        print(json.dumps(out, indent=2))
