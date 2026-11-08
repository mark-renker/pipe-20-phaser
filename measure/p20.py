"""Shared helpers for the Pipe-20 Phaser measurement scripts (numpy/scipy only).

Every script takes WAV files (hardware captures or model renders) and emits numbers (JSON)
and plots. The model path drives tools/render.cpp (pipe20_render) through `render()`.
"""
from __future__ import annotations
import json, os, subprocess, sys
from pathlib import Path
import numpy as np
from scipy import signal
from scipy.io import wavfile

ROOT = Path(__file__).resolve().parents[1]
RESULTS = ROOT / "docs" / "results"
PALETTE = ["#2a78d6", "#eb6834", "#1baf7a", "#eda100", "#e87ba4"]   # validated categorical slots 1-5 (light surface)
TEXT = "#1f1f1f"; MUTED = "#6b6b66"; GRID = "#e4e4df"

# ----------------------------------------------------------------------------- renderer
def find_renderer() -> str:
    env = os.environ.get("PIPE20_RENDER")
    if env and Path(env).exists():
        return env
    for d in ("build-core", "build", "build-universal", "build-ci"):
        for cand in (ROOT / d / "pipe20_render", ROOT / d / "Release" / "pipe20_render.exe", ROOT / d / "pipe20_render.exe"):
            if cand.exists():
                return str(cand)
    sys.exit("pipe20_render not found: build it (cmake -B build-core -DPIPE20_BUILD_PLUGINS=OFF && cmake --build build-core) or set PIPE20_RENDER")

def render(in_wav, out_wav, cv_wav=None, tunables=None, sets=None, **params) -> str:
    """params: pitch, resonance, rate, slow, depth, smooth, color, output, mode, stages, cvatt, input, drive, os, tail, block"""
    cmd = [find_renderer(), str(in_wav), str(out_wav)]
    for k, v in params.items():
        if v is None: continue
        if isinstance(v, bool): v = int(v)
        cmd += [f"--{k}", str(v)]
    if cv_wav: cmd += ["--cv", str(cv_wav)]
    if tunables: cmd += ["--tunables", str(tunables)]
    for k, v in (sets or {}).items(): cmd += ["--set", f"{k}={v}"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(f"render failed: {' '.join(cmd)}\n{r.stderr}")
    return str(out_wav)

def bench(os_mode="auto", **params) -> dict:
    cmd = [find_renderer(), "--bench", "--os", str(os_mode)]
    for k, v in params.items(): cmd += [f"--{k}", str(v)]
    r = subprocess.run(cmd, capture_output=True, text=True, check=True)
    return json.loads(r.stdout)

# ----------------------------------------------------------------------------- wav
def read_wav(path):
    fs, x = wavfile.read(str(path))
    if x.dtype == np.int16: x = x / 32768.0
    elif x.dtype == np.int32: x = x / 2147483648.0
    elif x.dtype == np.uint8: x = (x - 128) / 128.0
    x = np.asarray(x, dtype=np.float64)
    if x.ndim == 1: x = x[:, None]
    return fs, x.T          # [channel, frame]

def write_wav(path, fs, x):
    x = np.asarray(x, dtype=np.float32)
    if x.ndim == 2: x = x.T
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    wavfile.write(str(path), int(fs), x)

def dc_wav(path, fs, n, volts1=0.0, volts2=0.0, fullscale_volts=10.0):
    """Constant CV file: channel 0 = 1 V/oct input, channel 1 = attenuverted input."""
    write_wav(path, fs, np.stack([np.full(n, volts1 / fullscale_volts), np.full(n, volts2 / fullscale_volts)]))

# ----------------------------------------------------------------------------- signals / analysis
def ess(fs, dur=8.0, f1=10.0, f2=22000.0, amp=0.5, fade=0.05):
    """Exponential sine sweep (Farina). The inverse is not needed: deconvolve() divides by the played sweep."""
    n = int(dur * fs); t = np.arange(n) / fs
    R = np.log(f2 / f1)
    x = np.sin(2 * np.pi * f1 * dur / R * (np.exp(t * R / dur) - 1))
    w = np.ones(n); nf = int(fade * fs)
    w[:nf] = np.sin(np.linspace(0, np.pi / 2, nf)) ** 2; w[-nf:] = w[:nf][::-1]
    return amp * x * w, None

def deconvolve(y, x, fs, pre=0.05, post=0.5, reg_db=-60.0, band=(10.0, 22000.0)):
    """Linear impulse response of a system from (played x, recorded y): regularised frequency-domain division,
    tapered to the excitation band so the result is smoothly band-limited (a hard edge at the sweep's top
    frequency rings over all lags and corrupts the response). With an exponential sweep the harmonic-distortion
    responses land at large negative lags and are excluded by the crop (pre = 50 ms keeps the band-limiting
    ringing, which is confined to a few ms)."""
    nfft = 1 << int(np.ceil(np.log2(len(x) + len(y))))
    f = np.fft.rfftfreq(nfft, 1 / fs)
    X = np.fft.rfft(x, nfft); Y = np.fft.rfft(y, nfft)
    eps = (np.abs(X).max() * 10 ** (reg_db / 20)) ** 2
    H = Y * np.conj(X) / (np.abs(X) ** 2 + eps)
    f1, f2 = band
    w = np.ones_like(f)
    lo = (f >= f1) & (f < f1 * 1.5); w[lo] = 0.5 - 0.5 * np.cos(np.pi * (f[lo] - f1) / (0.5 * f1)); w[f < f1] = 0
    hi = (f > f2 * 0.9) & (f <= f2); w[hi] = 0.5 + 0.5 * np.cos(np.pi * (f[hi] - 0.9 * f2) / (0.1 * f2)); w[f > f2] = 0
    h = np.fft.irfft(H * w, nfft)
    peak = int(np.argmax(np.abs(h[: nfft // 2])))
    h = np.roll(h, int(pre * fs))            # bring the pre-peak lags in front of the peak
    peak += int(pre * fs)
    a = max(0, peak - int(pre * fs)); b = min(len(h), peak + int(post * fs))
    return h[a:b]

def ir_from_sweep(y, x, fs, **kw):
    return deconvolve(y, x, fs, **kw)

def freq_response(ir, fs, nfft=1 << 17):
    H = np.fft.rfft(ir, nfft)
    f = np.fft.rfftfreq(nfft, 1 / fs)
    return f, 20 * np.log10(np.abs(H) + 1e-15)

def notch_model(fc, N):
    k = np.arange(N // 2)
    return fc * np.tan((2 * k + 1) * np.pi / (2 * N))

def log_grid(f, db, fmin=20.0, fmax=24000.0, per_octave=2400):
    """Resample a linear-frequency response onto a log-frequency grid."""
    n = int(np.log2(fmax / fmin) * per_octave)
    fl = np.geomspace(fmin, fmax, n)
    return fl, np.interp(fl, f, db), per_octave

def find_notches(f, db, fmin=30.0, fmax=20000.0, prominence=6.0, smooth_oct=1.0 / 36, min_spacing_oct=0.3):
    """Notches = local minima of the fractional-octave-smoothed magnitude response (dB) at least `prominence` dB
    below their surroundings. Smoothing removes the intermodulation ripple a level-dependent (time-variant)
    device leaves in a deconvolved sweep response."""
    from scipy.ndimage import uniform_filter1d
    fl, dl, per_oct = log_grid(f, db)
    k = max(3, int(per_oct * smooth_oct))
    ds = uniform_filter1d(dl, k, mode="nearest")
    m = (fl >= fmin) & (fl <= fmax)
    idx = np.where(m)[0]
    peaks, _ = signal.find_peaks(-ds[m], prominence=prominence, distance=max(1, int(per_oct * min_spacing_oct)))
    floor = np.median(ds[m]) - 3.0          # a notch must sit below the response's median level
    return [(float(fl[idx[p]]), float(ds[idx[p]])) for p in peaks if ds[idx[p]] < floor]

def fit_fc(notches_hz, N, max_offset=3):
    """Least-squares f_c from a measured notch set assuming consecutive notch indices starting at k0 (0..max_offset).
    Returns (fc, k0, max_abs_err_percent)."""
    fk = np.asarray(sorted(notches_hz))
    if len(fk) > N // 2: fk = fk[-(N // 2):]          # more minima than the chain can make: keep the top set
    best = None
    for k0 in range(max_offset + 1):
        if k0 + len(fk) > N // 2: continue
        T = np.tan((2 * (np.arange(len(fk)) + k0) + 1) * np.pi / (2 * N))
        fc = float(np.sum(fk * T) / np.sum(T * T))
        err = np.max(np.abs(fk / (fc * T) - 1)) * 100
        if best is None or err < best[2]:
            best = (fc, k0, float(err))
    return best if best else (float("nan"), 0, float("nan"))

def periodic_noise(fs, dur, period=4096, amp=0.25, seed=7):
    """Flat-magnitude random-phase multisine repeated every `period` samples: a frame of `period` samples with a
    rectangular window has an exactly flat excitation spectrum, so a quasi-static response is read directly."""
    rng = np.random.default_rng(seed)
    X = np.exp(1j * rng.uniform(0, 2 * np.pi, period // 2 + 1)); X[0] = 0; X[-1] = 1
    x = np.fft.irfft(X, period); x *= amp / np.max(np.abs(x))
    reps = int(np.ceil(dur * fs / period))
    return np.tile(x, reps)[: int(dur * fs)]

def fit_fc_robust(notches_hz, N, tol=0.05):
    """f_c from an incomplete / partly spurious notch set: every (notch, index) hypothesis proposes an f_c; the
    hypothesis explaining the most notches within `tol` wins, then least squares on its inliers.
    Returns (fc, inliers, total)."""
    fk = np.asarray(sorted(notches_hz)); T = np.tan((2 * np.arange(N // 2) + 1) * np.pi / (2 * N))
    best = (0, np.nan, None)
    for f in fk:
        for k in range(N // 2):
            fc = f / T[k]; pred = fc * T
            d = np.min(np.abs(fk[:, None] / pred[None, :] - 1), axis=1)
            inl = d < tol
            if inl.sum() > best[0] or (inl.sum() == best[0] and inl.sum() and np.mean(d[inl]) < np.mean(best[2]) if best[2] is not None else False):
                best = (int(inl.sum()), fc, d[inl])
    n_in, fc, _ = best
    if n_in == 0: return float("nan"), 0, len(fk)
    pred = fc * T; idx = np.argmin(np.abs(fk[:, None] / pred[None, :] - 1), axis=1); d = np.abs(fk / pred[idx] - 1)
    m = d < tol
    fc = float(np.sum(fk[m] * T[idx[m]]) / np.sum(T[idx[m]] ** 2))
    return fc, int(m.sum()), len(fk)

def sine(fs, dur, f0, dbfs=0.0):
    t = np.arange(int(dur * fs)) / fs
    return 10 ** (dbfs / 20) * np.sin(2 * np.pi * f0 * t)

def spectrum(x, fs, nfft=None):
    n = len(x); nfft = nfft or n
    w = signal.windows.blackmanharris(n)
    X = np.fft.rfft(x * w, nfft) / (w.sum() / 2)
    return np.fft.rfftfreq(nfft, 1 / fs), 20 * np.log10(np.abs(X) + 1e-15)

def thd_n(y, fs, f0, nharm=5, bw_bins=6):
    """THD+N (ratio) and the first `nharm` harmonic levels (dB re fundamental) of a steady sine response."""
    n = len(y); w = signal.windows.blackmanharris(n)
    X = np.abs(np.fft.rfft(y * w)) ** 2
    f = np.fft.rfftfreq(n, 1 / fs)
    def band_power(fc):
        i = int(round(fc * n / fs)); return X[max(0, i - bw_bins): i + bw_bins + 1].sum()
    fund = band_power(f0)
    lo = int(round(20 * n / fs)); hi = int(round(min(20000, fs / 2 - 1) * n / fs))
    total = X[lo:hi].sum()
    rest = max(total - fund, 0.0)
    harms = [10 * np.log10(band_power(k * f0) / fund + 1e-30) if k * f0 < fs / 2 else float("nan") for k in range(1, nharm + 1)]
    return np.sqrt(rest / fund), harms

def level_dbfs(x):
    return 20 * np.log10(np.sqrt(np.mean(x ** 2)) * np.sqrt(2) + 1e-15)

def align(a, b, max_lag=4800):
    """Integer-sample lag that best aligns b to a (positive = b lags a)."""
    n = min(len(a), len(b))
    c = signal.correlate(b[:n], a[:n], mode="full", method="fft")
    lags = np.arange(-n + 1, n)
    m = np.abs(lags) <= max_lag
    return int(lags[m][np.argmax(c[m])])

# ----------------------------------------------------------------------------- plotting
def fig_ax(title, xlabel, ylabel, logx=False, size=(8, 4.2)):
    import matplotlib; matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    fig, ax = plt.subplots(figsize=size, dpi=120)
    ax.set_title(title, loc="left", color=TEXT, fontsize=11)
    ax.set_xlabel(xlabel, color=MUTED); ax.set_ylabel(ylabel, color=MUTED)
    ax.grid(True, color=GRID, linewidth=0.6); ax.set_axisbelow(True)
    for s in ("top", "right"): ax.spines[s].set_visible(False)
    for s in ("left", "bottom"): ax.spines[s].set_color(GRID)
    ax.tick_params(colors=MUTED, labelsize=8)
    if logx: ax.set_xscale("log")
    return fig, ax

def save(fig, path):
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    if len(fig.axes[0].lines) + len(fig.axes[0].collections) > 1 and fig.axes[0].get_legend_handles_labels()[1]:
        fig.axes[0].legend(frameon=False, fontsize=8, labelcolor=TEXT)
    fig.tight_layout(); fig.savefig(path); 
    import matplotlib.pyplot as plt; plt.close(fig)

def dump(obj, path):
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    Path(path).write_text(json.dumps(obj, indent=2, default=float))
