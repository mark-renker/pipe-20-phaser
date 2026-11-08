#!/usr/bin/env python3
"""Notch extraction (spec 5 / 8.1): swept sine in, NORMAL out -> notch frequencies, count below 20 kHz (= N), fitted f_c.

  notch_map.py --in sweep.wav --out response.wav [--stages 20] [--channel 0] [--plot x.png] [--json x.json]
  notch_map.py --model --out-dir docs/results [--cv -3 -2 ... 5] [--pitch 500] [--drive 1] [--os auto]

The sweep file must be the exponential sine sweep written by `--make-sweep sweep.wav` (same length/range), so the
inverse filter can be regenerated from its header.
"""
import argparse, json
from pathlib import Path
import numpy as np
import p20

def analyse(in_wav, out_wav, stages=20, channel=0, label=""):
    fs, x = p20.read_wav(in_wav); _, y = p20.read_wav(out_wav)
    ir = p20.deconvolve(y[channel], x[0], fs)
    f, db = p20.freq_response(ir, fs)
    notches = p20.find_notches(f, db)
    fc, inliers, total = p20.fit_fc_robust([n[0] for n in notches], stages)
    T = np.tan((2 * np.arange(stages // 2) + 1) * np.pi / (2 * stages)); pred = fc * T
    errs = [float(np.min(np.abs(n[0] / pred - 1)) * 100) for n in notches] if np.isfinite(fc) else []
    return dict(label=label, fs=fs, stages=stages, notch_count_below_20k=len(notches),
                notches_hz=[round(n[0], 2) for n in notches], notch_depth_db=[round(n[1], 1) for n in notches],
                fc_fit_hz=round(fc, 3), fit_inliers=inliers, fit_max_error_percent=round(max(errs), 3) if errs else None,
                notch_errors_percent=[round(e, 2) for e in errs],
                model_notches_hz=[round(v, 2) for v in p20.notch_model(fc, stages) if v < 20000]), (f, db)

def model_sweeps(out_dir, cvs, pitch, drive, os_mode, stages=20, fs=96000, dur=8.0, tunables=None, sets=None, amp=0.25):
    out_dir = Path(out_dir); tmp = out_dir / "tmp"; tmp.mkdir(parents=True, exist_ok=True)
    sweep, inv = p20.ess(fs, dur, amp=amp)   # -12 dBFS: the protocol level (2.5 Vpk Euro)
    p20.write_wav(tmp / "sweep.wav", fs, sweep)
    rows = []; curves = []
    for cv in cvs:
        p20.dc_wav(tmp / f"cv_{cv:+.1f}.wav", fs, len(sweep), volts1=cv)
        out = tmp / f"sweep_cv{cv:+.1f}_out.wav"
        p20.render(tmp / "sweep.wav", out, cv_wav=tmp / f"cv_{cv:+.1f}.wav", pitch=pitch, resonance=0, depth=0, drive=drive,
                   os=os_mode, output="stereo", stages=stages, tail=0.5, tunables=tunables, sets=sets)
        r, (f, db) = analyse(tmp / "sweep.wav", out, stages, 0, label=f"cv {cv:+.1f} V")
        r["cv_volts"] = cv; rows.append(r); curves.append((cv, f, db))
    return rows, curves

def plot(curves, path, title):
    fig, ax = p20.fig_ax(title, "Frequency (Hz)", "NORMAL magnitude (dB)", logx=True)
    for i, (cv, f, db) in enumerate(curves):
        m = (f >= 20) & (f <= 24000)
        ax.plot(f[m], db[m], color=p20.PALETTE[i % len(p20.PALETTE)], linewidth=1.2, label=f"CV {cv:+.1f} V")
    ax.set_ylim(-60, 8); ax.set_xlim(20, 24000)
    p20.save(fig, path)

if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--in", dest="inp"); ap.add_argument("--out"); ap.add_argument("--stages", type=int, default=20)
    ap.add_argument("--channel", type=int, default=0); ap.add_argument("--plot"); ap.add_argument("--json")
    ap.add_argument("--make-sweep", help="write the reference 8 s 20 Hz-20 kHz sweep at --fs"); ap.add_argument("--fs", type=int, default=96000)
    ap.add_argument("--model", action="store_true"); ap.add_argument("--out-dir", default=str(p20.RESULTS))
    ap.add_argument("--cv", type=float, nargs="*", default=[-3, -1, 0, 1, 3]); ap.add_argument("--pitch", type=float, default=500)
    ap.add_argument("--drive", type=float, default=1.0); ap.add_argument("--os", default="auto")
    a = ap.parse_args()
    if a.make_sweep:
        sweep, _ = p20.ess(a.fs, amp=0.25); p20.write_wav(a.make_sweep, a.fs, sweep); print("wrote", a.make_sweep)
    elif a.model:
        rows, curves = model_sweeps(a.out_dir, a.cv, a.pitch, a.drive, a.os, a.stages)
        plot(curves, Path(a.out_dir) / "notch_map.png", f"Model notch map, PITCH {a.pitch:g} Hz, Depth 0, Resonance 0")
        p20.dump(rows, Path(a.out_dir) / "notch_map.json")
        for r in rows: print(f"CV {r['cv_volts']:+.1f} V: {r['notch_count_below_20k']} notches ({r['fit_inliers']} on the tan law), f_c {r['fc_fit_hz']:.1f} Hz, worst notch error {r['fit_max_error_percent']} %")
    else:
        r, (f, db) = analyse(a.inp, a.out, a.stages, a.channel, label=a.out)
        if a.plot: plot([(0.0, f, db)], a.plot, Path(a.out).name)
        if a.json: p20.dump(r, a.json)
        print(json.dumps(r, indent=2))
