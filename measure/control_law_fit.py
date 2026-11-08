#!/usr/bin/env python3
"""Control-law fit (spec 8.2): f_c vs CV -> fit  f_c = floor + fref * 2^(V/vpo) * fsat / (fref * 2^(V/vpo) + fsat).

  control_law_fit.py --points notch_map.json [--hardware hw_notch_map.json] [--plot x.png] [--json x.json]
  control_law_fit.py --model [--cv -3 -2.5 ... 5]      (renders sweeps at each CV and fits)

`--points` is the JSON written by notch_map.py (list of {cv_volts, fc_fit_hz}). The hardware file has the same schema.
"""
import argparse, json
from pathlib import Path
import numpy as np
from scipy.optimize import curve_fit
import p20

def law(v, fref, vpo, fsat, floor):
    fe = fref * 2.0 ** (v / vpo)
    return floor + fe * fsat / (fe + fsat)

def fit(cv, fc):
    cv = np.asarray(cv, float); fc = np.asarray(fc, float)
    p0 = [fc[np.argmin(np.abs(cv))], 1.0, 5e4, 5.0]
    popt, _ = curve_fit(lambda v, a, b, c, d: np.log(law(v, a, b, c, d)), cv, np.log(fc), p0=p0,
                        bounds=([1, 0.5, 1e3, 0], [1e5, 2.0, 1e7, 200]), maxfev=20000)
    pred = law(cv, *popt)
    err = (pred / fc - 1) * 100
    return dict(fc_ref_hz=popt[0], volts_per_octave=popt[1], fc_sat_hz=popt[2], fc_floor_hz=popt[3],
                max_error_percent=float(np.max(np.abs(err))), errors_percent=[round(e, 3) for e in err])

if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--points"); ap.add_argument("--hardware"); ap.add_argument("--plot"); ap.add_argument("--json")
    ap.add_argument("--model", action="store_true"); ap.add_argument("--out-dir", default=str(p20.RESULTS))
    ap.add_argument("--cv", type=float, nargs="*", default=list(np.arange(-3, 5.01, 0.5))); ap.add_argument("--pitch", type=float, default=500)
    a = ap.parse_args()
    if a.model:
        import notch_map
        rows, _ = notch_map.model_sweeps(a.out_dir, a.cv, a.pitch, 1.0, "auto")
        pts = [dict(cv_volts=r["cv_volts"], fc_fit_hz=r["fc_fit_hz"]) for r in rows]
        p20.dump(pts, Path(a.out_dir) / "control_law_points.json")
        a.points = str(Path(a.out_dir) / "control_law_points.json"); a.plot = a.plot or str(Path(a.out_dir) / "control_law.png"); a.json = a.json or str(Path(a.out_dir) / "control_law.json")
    pts = json.loads(Path(a.points).read_text())
    cv = [p["cv_volts"] for p in pts]; fc = [p["fc_fit_hz"] for p in pts]
    res = dict(points=pts, fit=fit(cv, fc))
    if a.hardware:
        hw = json.loads(Path(a.hardware).read_text()); hcv = [p["cv_volts"] for p in hw]; hfc = [p["fc_fit_hz"] for p in hw]
        res["hardware_fit"] = fit(hcv, hfc)
        common = [(v, f, next(h["fc_fit_hz"] for h in hw if abs(h["cv_volts"] - v) < 1e-6)) for v, f in zip(cv, fc) if any(abs(h["cv_volts"] - v) < 1e-6 for h in hw)]
        res["model_vs_hardware_max_error_percent"] = float(max(abs(f / h - 1) * 100 for _, f, h in common)) if common else None
    if a.plot:
        fig, ax = p20.fig_ax("Control law: f_c vs CV", "CV (V)", "f_c (Hz)")
        ax.set_yscale("log")
        vv = np.linspace(min(cv), max(cv), 200)
        ax.plot(vv, law(vv, *[res["fit"][k] for k in ("fc_ref_hz", "volts_per_octave", "fc_sat_hz", "fc_floor_hz")]), color=p20.PALETTE[0], linewidth=2, label="model fit")
        ax.scatter(cv, fc, s=28, color=p20.PALETTE[0], zorder=3, label="model measured")
        if a.hardware: ax.scatter(hcv, hfc, s=28, color=p20.PALETTE[1], zorder=3, label="hardware")
        p20.save(fig, a.plot)
    if a.json: p20.dump(res, a.json)
    f = res["fit"]
    print(f"fit: fc_ref {f['fc_ref_hz']:.2f} Hz, {f['volts_per_octave']:.4f} V/oct, fc_sat {f['fc_sat_hz']:.0f} Hz, floor {f['fc_floor_hz']:.2f} Hz, max error {f['max_error_percent']:.3f} %")
