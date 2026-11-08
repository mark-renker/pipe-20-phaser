#!/usr/bin/env python3
"""Aliasing check (spec 8): 10 kHz and 15 kHz sines at 0 dBFS with the nonlinearity engaged; any component above
-80 dBFS that is not a harmonic of the input (or the LFO sidebands of one) is reported as a defect.

  aliasing.py --in sine.wav --out response.wav --f0 10000 [--json x.json]
  aliasing.py --model [--os auto|1|2|4|8]
"""
import argparse, json
from pathlib import Path
import numpy as np
import p20

def analyse(out_wav, f0, fs_in=None, channel=0, floor_db=-80.0, tol_hz=30.0, lfo_rate=None):
    fs, y = p20.read_wav(out_wav); x = y[channel][len(y[channel]) // 2:]
    f, db = p20.spectrum(x, fs)
    from scipy import signal
    pk, _ = signal.find_peaks(db, height=floor_db, prominence=20)
    comps = []
    for i in pk:
        fr = f[i]; k = round(fr / f0)
        harmonic = abs(fr - k * f0) < tol_hz and k >= 1
        if lfo_rate: harmonic = harmonic or any(abs(fr - (k * f0 + m * lfo_rate)) < tol_hz for m in range(-40, 41))
        comps.append(dict(freq_hz=round(float(fr), 1), level_dbfs=round(float(db[i]), 1), harmonic_order=int(k) if harmonic else None))
    defects = [c for c in comps if c["harmonic_order"] is None and c["freq_hz"] > 20]
    return dict(f0=f0, components=comps, alias_defects_above_floor=defects, worst_alias_dbfs=max([c["level_dbfs"] for c in defects], default=None))

if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--in", dest="inp"); ap.add_argument("--out"); ap.add_argument("--f0", type=float, default=10000); ap.add_argument("--json")
    ap.add_argument("--model", action="store_true"); ap.add_argument("--out-dir", default=str(p20.RESULTS)); ap.add_argument("--os", default="auto")
    a = ap.parse_args()
    if a.model:
        out_dir = Path(a.out_dir); tmp = out_dir / "tmp"; tmp.mkdir(parents=True, exist_ok=True)
        fs = 48000; results = {}
        for f0 in (10000.0, 15000.0):
            p20.write_wav(tmp / "tone.wav", fs, p20.sine(fs, 2.0, f0, 0.0))
            # static, nonlinearity engaged (INPUT +6 dB, Resonance 80 %)
            p20.render(tmp / "tone.wav", tmp / "tone_out.wav", pitch=1000, resonance=80, input=6, depth=0, os=a.os, output="stereo")
            r = analyse(tmp / "tone_out.wav", f0)
            # maximum modulation depth and rate
            p20.render(tmp / "tone.wav", tmp / "tone_mod.wav", pitch=1000, resonance=80, input=6, depth=100, rate=25, os=a.os, output="stereo")
            rm = analyse(tmp / "tone_mod.wav", f0, lfo_rate=25.0)
            results[f"{int(f0)}"] = dict(static=r, modulated=rm)
            print(f"{f0:.0f} Hz static: worst alias {r['worst_alias_dbfs']} dBFS ({len(r['alias_defects_above_floor'])} above -80); modulated: worst {rm['worst_alias_dbfs']} dBFS")
        results["os"] = a.os
        p20.dump(results, out_dir / f"aliasing_{a.os}.json")
    else:
        r = analyse(a.out, a.f0)
        if a.json: p20.dump(r, a.json)
        print(json.dumps(r, indent=2))
