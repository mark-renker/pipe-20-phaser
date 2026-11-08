#!/usr/bin/env python3
"""THD+N vs input level and level-dependent first-notch shift (spec 5, 8.4; verification 8 THD+N).

  thd_n.py --in sine.wav --out response.wav --f0 1000 [--channel 0]          one WAV pair -> THD+N %, harmonics
  thd_n.py --model [--levels -40 -35 ... 0] [--pitch 2000]                  renders the level series

THD uses NORMAL (channel 0) at a static setting with a 1 kHz tone. The notch shift uses a sweep at each level
(first notch = fitted f_c * tan(pi/2N) from the NORMAL response, robust to a filled-in lowest minimum) because the shift is a property of the response, not of one tone.
"""
import argparse, json
from pathlib import Path
import numpy as np
import p20

def analyse(in_wav, out_wav, f0, channel=0):
    fs, x = p20.read_wav(in_wav); _, y = p20.read_wav(out_wav)
    n = min(x.shape[1], y.shape[1]); seg = slice(n // 2, n)      # steady state
    ratio, harms = p20.thd_n(y[channel, seg], fs, f0)
    return dict(input_dbfs=round(p20.level_dbfs(x[0, seg]), 2), output_dbfs=round(p20.level_dbfs(y[channel, seg]), 2),
                thd_n_percent=round(100 * ratio, 5), thd_n_db=round(20 * np.log10(ratio + 1e-15), 2), harmonics_db=[round(h, 2) for h in harms])

if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--in", dest="inp"); ap.add_argument("--out"); ap.add_argument("--f0", type=float, default=1000); ap.add_argument("--channel", type=int, default=0)
    ap.add_argument("--json"); ap.add_argument("--model", action="store_true"); ap.add_argument("--out-dir", default=str(p20.RESULTS))
    ap.add_argument("--levels", type=float, nargs="*", default=list(range(-40, 1, 5))); ap.add_argument("--pitch", type=float, default=2000)
    ap.add_argument("--input-db", type=float, default=0.0); ap.add_argument("--resonance", type=float, default=0.0)
    a = ap.parse_args()
    if a.model:
        out_dir = Path(a.out_dir); tmp = out_dir / "tmp"; tmp.mkdir(parents=True, exist_ok=True)
        fs = 96000; rows = []
        import notch_map
        for lv in a.levels:
            p20.write_wav(tmp / "sine.wav", fs, p20.sine(fs, 2.0, a.f0, lv))
            p20.render(tmp / "sine.wav", tmp / "sine_out.wav", pitch=a.pitch, resonance=a.resonance, depth=0, input=a.input_db, os="auto", output="stereo")
            r = analyse(tmp / "sine.wav", tmp / "sine_out.wav", a.f0)
            sweep, _ = p20.ess(fs, 6.0, amp=10 ** (lv / 20)); p20.write_wav(tmp / "sweep_lv.wav", fs, sweep)
            p20.render(tmp / "sweep_lv.wav", tmp / "sweep_lv_out.wav", pitch=a.pitch, resonance=a.resonance, depth=0, input=a.input_db, os="auto", output="stereo", tail=0.5)
            nm, _ = notch_map.analyse(tmp / "sweep_lv.wav", tmp / "sweep_lv_out.wav")
            r.update(level_dbfs=lv, first_notch_hz=round(nm["fc_fit_hz"] * float(np.tan(np.pi / 40)), 3), fc_fit_hz=nm["fc_fit_hz"], lowest_minimum_hz=nm["notches_hz"][0] if nm["notches_hz"] else None)
            rows.append(r); print(f"{lv:+.0f} dBFS: THD+N {r['thd_n_percent']:.4f} %  first notch {r['first_notch_hz']} Hz")
        ref = rows[0]["first_notch_hz"]
        res = dict(f0=a.f0, pitch=a.pitch, input_db=a.input_db, rows=rows,
                   first_notch_shift_percent_at_max=round((rows[-1]["first_notch_hz"] / ref - 1) * 100, 3) if ref else None)
        p20.dump(res, out_dir / "thd_n.json")
        fig, ax = p20.fig_ax(f"THD+N vs input level ({a.f0:g} Hz, PITCH {a.pitch:g} Hz)", "input level (dBFS)", "THD+N (%)")
        ax.set_yscale("log"); ax.plot([r["level_dbfs"] for r in rows], [max(r["thd_n_percent"], 1e-5) for r in rows], color=p20.PALETTE[0], linewidth=2, marker="o", markersize=4, label="model")
        p20.save(fig, out_dir / "thd_n.png")
        fig, ax = p20.fig_ax("First-notch frequency vs input level", "input level (dBFS)", "first notch (Hz)")
        ax.plot([r["level_dbfs"] for r in rows], [r["first_notch_hz"] for r in rows], color=p20.PALETTE[1], linewidth=2, marker="o", markersize=4)
        p20.save(fig, out_dir / "notch_shift.png")
    else:
        r = analyse(a.inp, a.out, a.f0, a.channel)
        if a.json: p20.dump(r, a.json)
        print(json.dumps(r, indent=2))
