// Pipe-20 Phaser — DSP core unit tests (no JUCE). Spec 7: single all-pass stage phase response,
// chain notch positions for ideal poles, feedback stability, LFO shape. Plus oversampler, outputs, tunables.
#include "../src/dsp/Phaser.h"
#include <complex>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

using namespace pipe20;
static int failures = 0, checks = 0;
#define CHECK(cond, ...) do { ++checks; if (! (cond)) { ++failures; std::printf ("  FAIL %s:%d: ", __FILE__, __LINE__); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

static void fft (std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) { size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap (a[i], a[j]); }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * M_PI / (double) len; const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len) { std::complex<double> w (1.0); for (size_t j = 0; j < len / 2; ++j) { auto u = a[i + j], v = a[i + j + len / 2] * w; a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= wl; } }
    }
}

static Params staticParams (float fc, float res = 0.0f, int os = 1)
{
    Params p; p.pitchHz = fc; p.resonancePct = res; p.lfoDepthPct = 0; p.driveMatching = 0; p.oversampling = os; p.outputMode = 2; return p;
}
static Tunables idealTunables()
{
    Tunables t; t.fc_floor_hz = 0.0; t.fc_sat_hz = 1e12; t.mismatch_scale = 0.0; return t; // pure expo law, matched stages
}

// Magnitude response (dB) of NORMAL and ALTERNATIVE at the host rate from the impulse response.
static void response (Engine& e, int nfft, std::vector<double>& nDb, std::vector<double>& aDb)
{
    std::vector<float> in ((size_t) nfft, 0.0f), l ((size_t) nfft), r ((size_t) nfft); in[0] = 1.0f;
    e.process (in.data(), nullptr, nullptr, l.data(), r.data(), nfft);
    std::vector<std::complex<double>> L (l.begin(), l.end()), R (r.begin(), r.end());
    fft (L); fft (R);
    nDb.resize ((size_t) nfft / 2); aDb.resize ((size_t) nfft / 2);
    for (int i = 0; i < nfft / 2; ++i) { nDb[(size_t) i] = 20.0 * std::log10 (std::abs (L[(size_t) i]) + 1e-12); aDb[(size_t) i] = 20.0 * std::log10 (std::abs (R[(size_t) i]) + 1e-12); }
}
static std::vector<int> localMinima (const std::vector<double>& db, double depthDb, int lo, int hi)
{
    std::vector<int> out;
    for (int i = std::max (lo, 2); i < std::min (hi, (int) db.size() - 2); ++i)
        if (db[(size_t) i] < db[(size_t) i - 1] && db[(size_t) i] <= db[(size_t) i + 1] && db[(size_t) i] < -depthDb) out.push_back (i);
    return out;
}

// ---- T1: single stage phase vs the bilinear-prewarped analytic phase, exact -90 deg at f_c
static void testSingleStagePhase()
{
    std::printf ("T1 single all-pass stage phase response\n");
    const double fe = 96000.0, fc = 1000.0; const double g = std::tan (M_PI * fc / fe);
    for (double f : { 250.0, 1000.0, 4000.0, 12000.0 })
    {
        Ladder lad; lad.N = 1; lad.reset(); lad.offMul.fill (1.0); lad.setCoefficients (g);
        const int n = 48000; double sr = 0, si = 0; double tap;
        for (int i = 0; i < n; ++i)
        {
            const double x = std::sin (2.0 * M_PI * f * i / fe); const double y = lad.process (x, 0, tap);
            if (i > n / 2) { sr += y * std::sin (2.0 * M_PI * f * i / fe); si += y * std::cos (2.0 * M_PI * f * i / fe); }
        }
        const double phase = std::atan2 (si, sr) * 180.0 / M_PI;            // measured
        const double expect = -2.0 * std::atan (std::tan (M_PI * f / fe) / g) * 180.0 / M_PI;
        const double mag = 2.0 * std::sqrt (sr * sr + si * si) / (n / 2);
        CHECK (std::fabs (phase - expect) < 0.5, "f=%g phase %.2f expected %.2f", f, phase, expect);
        CHECK (std::fabs (mag - 1.0) < 0.01, "f=%g magnitude %.4f (all-pass)", f, mag);
        if (f == fc) CHECK (std::fabs (phase + 90.0) < 0.5, "phase at f_c = %.2f (expect -90)", phase);
    }
}

// ---- T2: notch map of the ideal 20-stage chain (NORMAL) and complementary peaks (ALTERNATIVE)
static void testNotchPositions()
{
    std::printf ("T2 chain notch positions (ideal poles)\n");
    for (int N : { 20, 12 })
    {
        Engine e; e.tun = idealTunables(); Params p = staticParams (1000.0f, 0.0f, 3); p.stages = N; e.p = p; e.prepare (48000.0); e.setParams (p);
        const int nfft = 1 << 16; std::vector<double> nDb, aDb; response (e, nfft, nDb, aDb);
        const double fe = e.engineRate(), g = std::tan (M_PI * 1000.0 / fe);
        auto mins = localMinima (nDb, 20.0, 1, nfft * 20000 / 48000);
        std::vector<double> expect;
        for (int k = 0; k < N / 2; ++k) { const double fk = fe / M_PI * std::atan (g * std::tan ((2 * k + 1) * M_PI / (2.0 * N))); if (fk < 20000.0) expect.push_back (fk); }
        CHECK (mins.size() == expect.size(), "N=%d: %zu notches below 20 kHz, expected %zu", N, mins.size(), expect.size());
        for (size_t k = 0; k < std::min (mins.size(), expect.size()); ++k)
        {
            const double f = mins[k] * 48000.0 / nfft, err = (f - expect[k]) / expect[k] * 100.0;
            CHECK (std::fabs (err) < 0.5, "N=%d notch %zu at %.1f Hz, expected %.1f (%.2f %%)", N, k, f, expect[k], err);
            // ALTERNATIVE peaks where NORMAL has notches (complementary)
            CHECK (aDb[(size_t) mins[k]] > 5.0, "N=%d ALTERNATIVE at notch %zu is %.1f dB (expect a peak)", N, k, aDb[(size_t) mins[k]]);
        }
        if (N == 20) { // analog (unwarped) comparison for the docs
            for (size_t k = 0; k < expect.size(); ++k) std::printf ("  notch %zu: model %.1f Hz, analog %.1f Hz\n", k, expect[k], 1000.0 * std::tan ((2 * k + 1) * M_PI / 40.0));
        }
    }
    // COLOR inverts the wet path exactly: NORMAL(color) == ALTERNATIVE (alt_mode 0), sample for sample
    Engine e; e.tun = idealTunables(); Params p = staticParams (700.0f, 20.0f, 2); p.color = true; e.p = p; e.prepare (48000.0); e.setParams (p);
    std::vector<float> in (4096), l (4096), r (4096); for (size_t i = 0; i < in.size(); ++i) in[i] = (float) std::sin (0.37 * i) * 0.5f;
    e.process (in.data(), nullptr, nullptr, l.data(), r.data(), 4096);
    double maxd = 0; for (size_t i = 2048; i < 4096; ++i) maxd = std::max (maxd, (double) std::fabs (l[i] - r[i]));
    CHECK (maxd < 1e-6, "COLOR NORMAL vs ALTERNATIVE max difference %.3g", maxd);
}

// ---- T3: feedback stability below / above the oscillation threshold, oscillation pitch
static void testFeedback()
{
    std::printf ("T3 feedback stability and self-oscillation\n");
    auto run = [] (float res, double& earlyRms, double& lateRms, double& peak, double& f0)
    {
        Engine e; e.tun = idealTunables(); e.tun.fb_gain_max = 1.2; Params p = staticParams (1000.0f, res, 3); e.p = p; e.prepare (48000.0); e.setParams (p);
        const int n = 48000 * 3; std::vector<float> in ((size_t) n, 0.0f), l ((size_t) n), r ((size_t) n); in[100] = 0.5f;
        e.process (in.data(), nullptr, nullptr, l.data(), r.data(), n);
        auto rms = [&] (int a, int b) { double s = 0; for (int i = a; i < b; ++i) s += (double) l[(size_t) i] * l[(size_t) i]; return std::sqrt (s / (b - a)); };
        earlyRms = rms (100, 100 + 2400); lateRms = rms (n - 24000, n); peak = 0; for (auto v : l) peak = std::max (peak, (double) std::fabs (v));
        // oscillation frequency from the last second (zero crossings)
        int zc = 0; for (int i = n - 48000 + 1; i < n; ++i) if ((l[(size_t) i] >= 0) != (l[(size_t) i - 1] >= 0)) ++zc; f0 = zc / 2.0;
    };
    double e1, l1, pk1, f1, e2, l2, pk2, f2;
    run (75.0f, e1, l1, pk1, f1);   // loop gain 0.90
    run (95.0f, e2, l2, pk2, f2);   // loop gain 1.14
    CHECK (l1 < 1e-4 * std::max (e1, 1e-9) || l1 < 1e-7, "below onset: impulse response must decay (early %.3g late %.3g)", e1, l1);
    CHECK (l2 > 0.01 && pk2 < 2.0, "above onset: sustained bounded oscillation (late rms %.3g peak %.3g)", l2, pk2);
    // the loop oscillates at a mode f_c tan(m pi / N); report which
    double best = 1e9; int bm = 0; for (int m = 1; m < 10; ++m) { const double fm = 1000.0 * std::tan (m * M_PI / 20.0); if (std::fabs (fm - f2) < best) { best = std::fabs (fm - f2); bm = m; } }
    std::printf ("  oscillation at %.1f Hz (mode %d: f_c tan(%d pi/20) = %.1f Hz)\n", f2, bm, bm, 1000.0 * std::tan (bm * M_PI / 20.0));
    CHECK (best / f2 < 0.05, "oscillation pitch %.1f Hz is not on a loop mode", f2);
}

// ---- T4: LFO shape (triangle), period, smooth-mode HPF
static void testLfo()
{
    std::printf ("T4 LFO shape\n");
    const double fe = 96000.0, rate = 2.0; Lfo lfo; lfo.hp.setCutoff (0.5, fe);
    double maxErr = 0, mn = 1, mx = -1; int periodSamples = 0, lastRise = -1;
    for (int i = 0; i < (int) (fe * 3); ++i)
    {
        const double t = i / fe; const double v = lfo.tick (rate / fe);
        const double ph = std::fmod (t * rate + 1.0 / fe * rate, 1.0); const double ideal = ph < 0.5 ? 4 * ph - 1 : 3 - 4 * ph;
        maxErr = std::max (maxErr, std::fabs (v - ideal)); mn = std::min (mn, v); mx = std::max (mx, v);
        if (i > 0 && lfo.phase < rate / fe * 1.5 && lastRise >= 0 && periodSamples == 0 && i - lastRise > 10) periodSamples = i - lastRise;
        if (lfo.phase < rate / fe * 1.5 && (lastRise < 0 || i - lastRise > 10)) lastRise = i;
    }
    CHECK (maxErr < 1e-6, "triangle linearity error %.3g", maxErr);
    CHECK (std::fabs (mn + 1) < 1e-4 && std::fabs (mx - 1) < 1e-4, "triangle range [%.4f, %.4f]", mn, mx);
    CHECK (std::fabs (periodSamples - fe / rate) < 2, "period %d samples, expected %.0f", periodSamples, fe / rate);
    // SMOOTH: a 0.5 Hz HPF on a 0.2 Hz triangle removes most of it; on a 20 Hz triangle almost nothing
    auto hpAtten = [&] (double r) { Lfo l; l.hp.setCutoff (0.5, fe); double s = 0, so = 0; for (int i = 0; i < (int) (fe * 20); ++i) { const double v = l.tick (r / fe); const double o = l.hp.hp (v); if (i > fe * 10) { s += v * v; so += o * o; } } return 10 * std::log10 (so / s); };
    const double a1 = hpAtten (0.2), a2 = hpAtten (20.0);
    std::printf ("  SMOOTH attenuation: %.1f dB at 0.2 Hz, %.2f dB at 20 Hz\n", a1, a2);
    CHECK (a1 < -3.0 && a2 > -0.1, "SMOOTH HPF corner behaviour");
}

// ---- T5: oversampler is transparent (unity, flat) and has no bulk latency. The zero-latency IIR filters
//          do have a small minimum-phase group delay at low frequencies; it is measured and bounded here.
static void testOversampler()
{
    std::printf ("T5 oversampler transparency / latency\n");
    for (int f : { 2, 4, 8 })
    {
        Oversampler os; os.setFactor (f);
        const double fs = 48000.0; const int n = 48000; double buf[8];
        std::vector<double> in ((size_t) n), out ((size_t) n);
        for (int i = 0; i < n; ++i) { in[(size_t) i] = std::sin (2 * M_PI * 1000.0 * i / fs); os.upsample (in[(size_t) i], buf); out[(size_t) i] = os.downsample (0, buf); }
        // phase delay at 1 kHz from the projection onto sin/cos
        double sr = 0, si = 0; for (int i = n / 2; i < n; ++i) { sr += out[(size_t) i] * std::sin (2 * M_PI * 1000.0 * i / fs); si += out[(size_t) i] * std::cos (2 * M_PI * 1000.0 * i / fs); }
        const double mag = 2.0 * std::sqrt (sr * sr + si * si) / (n / 2), ph = std::atan2 (si, sr);
        const double delaySamples = -ph / (2 * M_PI * 1000.0) * fs;
        // and a DC step must settle to exactly 1 (no gain error) quickly
        Oversampler os2; os2.setFactor (f); double y = 0; for (int i = 0; i < 400; ++i) { os2.upsample (1.0, buf); y = os2.downsample (0, buf); }
        std::printf ("  %dx: 1 kHz gain %.4f dB, phase delay %.2f samples @ 48 kHz; DC settles to %.6f\n", f, 20 * std::log10 (mag), delaySamples, y);
        CHECK (std::fabs (20 * std::log10 (mag)) < 0.02, "%dx: 1 kHz gain error", f);
        CHECK (delaySamples > 0 && delaySamples < 8.0, "%dx: phase delay %.2f samples", f, delaySamples);
        CHECK (std::fabs (y - 1.0) < 1e-4, "%dx: DC gain %.6f", f, y);
    }
}

// ---- T6: tunables JSON round trip
static void testTunables()
{
    std::printf ("T6 tunables JSON\n");
    Tunables t; t.fb_gain_max = 1.333; t.stageOffsetCents[3] = 42; const std::string js = t.toJson();
    Tunables u; std::string err; const int n = u.loadJsonText (js, &err);
    CHECK (n > 30, "loaded %d keys (%s)", n, err.c_str());
    CHECK (std::fabs (u.fb_gain_max - 1.333) < 1e-9 && u.stageOffsetCents[3] == 42, "round trip values");
    CHECK (u.loadJsonText ("{ \"feedback.sign\": -1 }") == 1 && u.fb_sign == -1, "flat value form");
}

// ---- T7: vibrato mode removes the dry path; output modes route correctly; latency = 0
static void testOutputs()
{
    std::printf ("T7 outputs / modes\n");
    Engine e; e.tun = idealTunables(); Params p = staticParams (1000.0f, 0.0f, 2); p.mode = 1; p.outputMode = 0; e.p = p; e.prepare (48000.0); e.setParams (p);
    const int nfft = 1 << 14; std::vector<double> nDb, aDb; response (e, nfft, nDb, aDb);
    double mn = 1e9, mx = -1e9; for (int i = 10; i < nfft * 18000 / 48000; ++i) { mn = std::min (mn, nDb[(size_t) i]); mx = std::max (mx, nDb[(size_t) i]); }
    CHECK (mx < 0.2 && mn > -0.6, "vibrato mode is all-pass (%.2f .. %.2f dB)", mn, mx);
    CHECK (e.latencySamples() == 0, "latency");
}

int main()
{
    testSingleStagePhase(); testNotchPositions(); testFeedback(); testLfo(); testOversampler(); testTunables(); testOutputs();
    std::printf ("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
