#!/usr/bin/env python3
"""CPU budget (spec 8): per-instance CPU at 48 kHz / 64-sample buffers for each engine rate factor."""
import json, platform, sys
from pathlib import Path
import p20
out_dir = Path(sys.argv[sys.argv.index("--out-dir") + 1]) if "--out-dir" in sys.argv else p20.RESULTS
rows = []
for os_mode, label in (("1", "1x"), ("2", "2x"), ("4", "4x"), ("8", "8x"), ("auto", "auto (res 0)")):
    r = p20.bench(os_mode); r["mode"] = label; rows.append(r); print(f"{label:>12}: {r['cpu_percent_one_core']:.2f} % of one core (engine {r['engine_factor']}x)")
res = dict(machine=platform.machine(), system=platform.platform(), rows=rows)
p20.dump(res, out_dir / "cpu.json")
