#!/usr/bin/env python3
"""Acceptance table (spec 8.1-8.6): compare model results against hardware results of the same schema.

  compare.py --model docs/results --hardware measure/data/hardware/results [--json x.json]

Each directory holds notch_map.json, control_law.json, osc_pitch.json, thd_n.json, lfo.json produced by the scripts
from the respective WAV sets. Missing hardware files are reported as 'not measured'.
"""
import argparse, json
from pathlib import Path

def load(d, name):
    p = Path(d) / name
    return json.loads(p.read_text()) if p.exists() else None

def pct(a, b): return abs(a / b - 1) * 100

def main(model_dir, hw_dir):
    rows = []
    def row(item, target, value, ok): rows.append(dict(item=item, target=target, value=value, status="PASS" if ok else ("FAIL" if ok is False else "not measured")))
    m, h = load(model_dir, "notch_map.json"), load(hw_dir, "notch_map.json")
    if m and h:
        worst = 0.0; count_ok = True
        for mr in m:
            hr = next((x for x in h if abs(x["cv_volts"] - mr["cv_volts"]) < 1e-6), None)
            if not hr: continue
            count_ok &= mr["notch_count_below_20k"] == hr["notch_count_below_20k"]
            for a, b in zip(mr["notches_hz"], hr["notches_hz"]): worst = max(worst, pct(a, b))
        row("8.1 notch frequencies", "<= 2 % each", f"{worst:.2f} % worst", worst <= 2.0)
        row("8.1 notch count (N)", "exact", "match" if count_ok else "mismatch", count_ok)
    else: row("8.1 notch map", "<= 2 %", "-", None)
    m, h = load(model_dir, "control_law.json"), load(hw_dir, "control_law.json")
    if m and h:
        worst = max(pct(a["fc_fit_hz"], b["fc_fit_hz"]) for a in m["points"] for b in h["points"] if abs(a["cv_volts"] - b["cv_volts"]) < 1e-6)
        row("8.2 control law f_c(CV)", "<= 3 %", f"{worst:.2f} %", worst <= 3.0)
    else: row("8.2 control law", "<= 3 %", "-", None)
    m, h = load(model_dir, "osc_pitch.json"), load(hw_dir, "osc_pitch.json")
    if m and h and m.get("onset_resonance_pct") and h.get("onset_resonance_pct"):
        d = pct(m["onset_resonance_pct"], h["onset_resonance_pct"]); row("8.3 oscillation onset", "<= 5 %", f"{d:.1f} %", d <= 5)
        cents = max(abs(1200 * __import__('math').log2(a["f0_hz"] / b["f0_hz"])) for a in m["pitch_vs_cv"] for b in h["pitch_vs_cv"] if a["cv_volts"] == b["cv_volts"])
        row("8.3 oscillation pitch", "<= 25 cents", f"{cents:.1f} cents", cents <= 25)
        hd = max(abs(x - y) for a in m["pitch_vs_cv"] for b in h["pitch_vs_cv"] if a["cv_volts"] == b["cv_volts"] for x, y in zip(a["harmonics_db"], b["harmonics_db"]))
        row("8.3 oscillation harmonics 1-5", "<= 3 dB", f"{hd:.1f} dB", hd <= 3)
    else: row("8.3 self-oscillation", "onset 5 % / 25 cents / 3 dB", "-", None)
    m, h = load(model_dir, "thd_n.json"), load(hw_dir, "thd_n.json")
    if m and h and m.get("first_notch_shift_percent_at_max") is not None and h.get("first_notch_shift_percent_at_max"):
        d = abs(m["first_notch_shift_percent_at_max"] - h["first_notch_shift_percent_at_max"]) / abs(h["first_notch_shift_percent_at_max"]) * 100
        row("8.4 level-dependent notch shift", "<= 20 % of measured shift", f"{d:.0f} %", d <= 20)
    else: row("8.4 level dependence", "<= 20 % of shift", "-", None)
    m, h = load(model_dir, "lfo.json"), load(hw_dir, "lfo.json")
    if m and h:
        worst = max(pct(a["rate_hz"], b["rate_hz"]) for a in m["rows"] for b in h["rows"] if a["rate_set_hz"] == b["rate_set_hz"] and a["smooth"] == b["smooth"])
        row("8.6 LFO rate limits", "<= 5 %", f"{worst:.1f} %", worst <= 5)
    else: row("8.6 LFO", "rate 5 % / corner 20 %", "-", None)
    n = load(hw_dir, "null_test.json")
    if n: row("8 null test (static)", "< -30 dB", f"{n['residual_db_rms']} dB", n["residual_db_rms"] < -30)
    else: row("8 null test", "< -30 dB", "-", None)
    return rows

if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--model", default="docs/results"); ap.add_argument("--hardware", default="measure/data/hardware/results"); ap.add_argument("--json")
    a = ap.parse_args()
    rows = main(a.model, a.hardware)
    for r in rows: print(f"{r['status']:>13}  {r['item']:<36} target {r['target']:<28} {r['value']}")
    if a.json: Path(a.json).write_text(json.dumps(rows, indent=2))
