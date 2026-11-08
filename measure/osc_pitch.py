#!/usr/bin/env python3
"""Self-oscillation (spec 8.3): onset resonance, oscillation pitch vs CV (cents), first five harmonics.

  osc_pitch.py --out osc.wav [--channel 0]                 one capture -> f0 (Hz), harmonics (dB re fundamental)
  osc_pitch.py --model [--pitch 500] [--cv -3 ... 5]       onset search + pitch tracking + waveform
"""
import argparse, json
from pathlib import Path
import numpy as np
import p20

def analyse(out_wav, channel=0):
    fs, y = p20.read_wav(out_wav); x = y[channel]
    seg = x[len(x) // 2:]
    rms = float(np.sqrt(np.mean(seg ** 2)))
    f, db = p20.spectrum(seg, fs)
    m = (f > 20) & (f < 20000)
    f0 = float(f[m][np.argmax(db[m])])
    # refine with zero crossings around the dominant component (band-limit first)
    from scipy import signal
    sos = signal.butter(4, [max(10, f0 * 0.5), min(fs / 2 - 1, f0 * 1.5)], btype="band", fs=fs, output="sos")
    z = signal.sosfiltfilt(sos, seg); zc = np.where(np.diff(np.signbit(z).astype(int)) != 0)[0]
    if len(zc) > 4: f0 = float(fs * (len(zc) - 1) / 2 / (zc[-1] - zc[0]))
    _, harms = p20.thd_n(seg, fs, f0)
    return dict(f0_hz=round(f0, 3), rms_dbfs=round(20 * np.log10(rms + 1e-15), 2), peak=round(float(np.max(np.abs(seg))), 4), harmonics_db=[round(h, 2) for h in harms])

if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out"); ap.add_argument("--channel", type=int, default=0); ap.add_argument("--json")
    ap.add_argument("--model", action="store_true"); ap.add_argument("--out-dir", default=str(p20.RESULTS))
    ap.add_argument("--pitch", type=float, default=500); ap.add_argument("--cv", type=float, nargs="*", default=[-3, -2, -1, 0, 1, 2, 3, 4, 5])
    a = ap.parse_args()
    if a.model:
        out_dir = Path(a.out_dir); tmp = out_dir / "tmp"; tmp.mkdir(parents=True, exist_ok=True)
        fs = 48000; n = 3 * fs; kick = np.zeros(n); kick[100] = 0.1
        p20.write_wav(tmp / "kick.wav", fs, kick)
        onset = None; scan = []
        for res in np.arange(70, 100.1, 1.0):
            p20.render(tmp / "kick.wav", tmp / "osc.wav", pitch=a.pitch, resonance=res, depth=0, output="stereo")
            r = analyse(tmp / "osc.wav"); scan.append(dict(resonance_pct=float(res), rms_dbfs=r["rms_dbfs"]))
            if onset is None and r["rms_dbfs"] > -40: onset = float(res)
        rows = []
        for cv in a.cv:
            p20.dc_wav(tmp / "cv.wav", fs, n, volts1=cv)
            p20.render(tmp / "kick.wav", tmp / "osc.wav", cv_wav=tmp / "cv.wav", pitch=a.pitch, resonance=100, depth=0, output="stereo")
            r = analyse(tmp / "osc.wav"); r["cv_volts"] = cv; rows.append(r)
        ref = next(r for r in rows if r["cv_volts"] == 0)["f0_hz"]
        for r in rows:
            r["cents_vs_1v_oct"] = round(1200 * np.log2(r["f0_hz"] / (ref * 2 ** r["cv_volts"])), 1)
            print(f"CV {r['cv_volts']:+.0f} V: {r['f0_hz']:.1f} Hz ({r['cents_vs_1v_oct']:+.1f} cents vs 1 V/oct), {r['rms_dbfs']:.1f} dBFS, harmonics {r['harmonics_db']}")
        osc = [r for r in rows if r["rms_dbfs"] > -30]      # rows where the loop actually oscillates
        res = dict(pitch=a.pitch, onset_resonance_pct=onset, onset_scan=scan, pitch_vs_cv=rows,
                   oscillating_cv_range=[min(r["cv_volts"] for r in osc), max(r["cv_volts"] for r in osc)] if osc else None,
                   max_abs_cents_error=max(abs(r["cents_vs_1v_oct"]) for r in osc) if osc else None,
                   max_abs_cents_error_pm1v=max(abs(r["cents_vs_1v_oct"]) for r in osc if abs(r["cv_volts"]) <= 1) if osc else None)
        print(f"onset at Resonance {onset} %")
        p20.dump(res, out_dir / "osc_pitch.json")
        fig, ax = p20.fig_ax("Self-oscillation pitch vs CV", "CV (V)", "deviation from 1 V/oct (cents)")
        ax.axhspan(-25, 25, color=p20.GRID, alpha=0.6, label="spec band ±25 cents")
        ax.plot([r["cv_volts"] for r in rows], [r["cents_vs_1v_oct"] for r in rows], color=p20.PALETTE[0], linewidth=2, marker="o", markersize=4, label="model")
        p20.save(fig, out_dir / "osc_pitch.png")
    else:
        r = analyse(a.out, a.channel)
        if a.json: p20.dump(r, a.json)
        print(json.dumps(r, indent=2))
