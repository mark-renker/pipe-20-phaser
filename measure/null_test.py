#!/usr/bin/env python3
"""Null test (spec 8): align two recordings, subtract, report residual in dB RMS (re the reference) and plot the
residual spectrum.   null_test.py reference.wav candidate.wav [--channel 0] [--gain-match] [--plot x.png] [--json x.json]
"""
import argparse, json
from pathlib import Path
import numpy as np
import p20

def null(a, b, fs, gain_match=False, max_lag=4800):
    lag = p20.align(a, b, max_lag)
    if lag > 0: b = b[lag:]; a = a[:len(b)]
    elif lag < 0: a = a[-lag:]; b = b[:len(a)]
    n = min(len(a), len(b)); a = a[:n]; b = b[:n]
    g = float(np.dot(a, b) / (np.dot(b, b) + 1e-20)) if gain_match else 1.0
    r = a - g * b
    res_db = 20 * np.log10(np.sqrt(np.mean(r ** 2)) / (np.sqrt(np.mean(a ** 2)) + 1e-20) + 1e-20)
    f, rdb = p20.spectrum(r[: min(n, 1 << 18)], fs); _, adb = p20.spectrum(a[: min(n, 1 << 18)], fs)
    return dict(lag_samples=lag, gain=round(g, 6), residual_db_rms=round(float(res_db), 2), frames=n), (f, adb, rdb)

if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("reference"); ap.add_argument("candidate"); ap.add_argument("--channel", type=int, default=0)
    ap.add_argument("--gain-match", action="store_true"); ap.add_argument("--plot"); ap.add_argument("--json")
    a = ap.parse_args()
    fs, x = p20.read_wav(a.reference); fs2, y = p20.read_wav(a.candidate)
    assert fs == fs2, "sample rates differ"
    r, (f, adb, rdb) = null(x[a.channel], y[min(a.channel, y.shape[0] - 1)], fs, a.gain_match)
    if a.plot:
        fig, ax = p20.fig_ax(f"Null test residual: {r['residual_db_rms']} dB RMS", "Frequency (Hz)", "level (dB)", logx=True)
        m = (f > 20) & (f < 20000); ax.plot(f[m], adb[m], color=p20.PALETTE[0], linewidth=1, label="reference"); ax.plot(f[m], rdb[m], color=p20.PALETTE[1], linewidth=1, label="residual")
        p20.save(fig, a.plot)
    if a.json: p20.dump(r, a.json)
    print(json.dumps(r, indent=2))
