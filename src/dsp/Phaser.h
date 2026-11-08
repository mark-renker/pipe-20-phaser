// Pipe-20 Phaser — JUCE-free DSP core.
//
//   in -> INPUT attenuator -> [feedback sum] -> AP1 .. AP_N (TPT first-order all-pass, shared pole,
//   per-stage offset + junction-resistance level dependence) -> wet
//   wet -> loop HP/LP -> tanh soft limiter -> x feedback gain -> back to [feedback sum]   (zero-delay solve)
//   NORMAL = dry + wet (COLOR: dry - wet)   ALTERNATIVE = dry - wet (or dry + tap)   VIBRATO = wet only
//   control: PITCH + 1 V/oct CV + attenuverted CV  --(1 - depth)--+--(depth)-- LFO (triangle, optional HPF)
//            -> smoothing -> expo converter -> 1/x compression -> f_c -> g = tan(pi f_c / f_engine)
//
// The whole engine runs at the host rate x {1,2,4,8} (zero-latency IIR oversampling, see Oversampler).
// All constants that must come from hardware measurement are in Tunables (resources/tunables.json).
#pragma once
#include "Tunables.h"
#include "OversampleFilters.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace pipe20 {

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ---------------------------------------------------------------- parameters (plugin units)
struct Params
{
    float pitchHz       = 440.0f;  // f_c of the stages with CV = 0 and Depth = 0
    float resonancePct  = 0.0f;    // 0..100 % of loop gain (self-oscillation above 100 / feedback.gain_max)
    float lfoRateHz     = 1.0f;    // Hz
    bool  lfoSlow       = false;   // L* slow mode (rate / lfo.slow_divisor)
    float lfoDepthPct   = 0.0f;    // crossfade PITCH+CV (0) <-> LFO (100)
    bool  lfoSmooth     = false;   // HPF on the LFO CV
    bool  color         = false;   // invert the wet signal on NORMAL
    int   outputMode    = 2;       // 0 Normal, 1 Alternative, 2 Stereo (L = Normal, R = Alternative)
    int   mode          = 0;       // 0 Phase, 1 Vibrato (dry path removed)
    int   stages        = 20;      // 12 / 16 / 18 / 20
    float cvAttPct      = 0.0f;    // attenuverter for the second CV input, -100..100 %
    float inputDb       = 0.0f;    // INPUT attenuator, -60 (= off) .. +6 dB
    float driveMatching = 1.0f;    // 0 = ideal matched linear stages, 1 = measured mismatch + junction nonlinearity
    int   oversampling  = 0;       // 0 Auto (tunables), 1 = 1x, 2 = 2x, 3 = 4x, 4 = 8x
};

// ---------------------------------------------------------------- building blocks
struct OnePoleTPT
{
    double s = 0.0, G = 0.0;
    void setCutoff (double hz, double fs) { double g = std::tan (M_PI * std::min (hz, 0.49 * fs) / fs); G = g / (1.0 + g); }
    double lp (double x) { double v = (x - s) * G; double y = v + s; s = y + v; return y; }
    double hp (double x) { return x - lp (x); }
    void reset() { s = 0.0; }
};

struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    double process (double x) { double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void reset() { z1 = z2 = 0; }
};

struct SosFilter
{
    std::vector<Biquad> sec;
    void set (const double (*sos)[6], int n)
    {
        sec.assign ((size_t) n, Biquad {});
        for (int i = 0; i < n; ++i) { auto& b = sec[(size_t) i]; b.b0 = sos[i][0]; b.b1 = sos[i][1]; b.b2 = sos[i][2]; b.a1 = sos[i][4]; b.a2 = sos[i][5]; }
    }
    double process (double x) { for (auto& b : sec) x = b.process (x); return x; }
    void reset() { for (auto& b : sec) b.reset(); }
};

// Rate-doubling stages cascaded to reach 1x / 2x / 4x / 8x. Zero algorithmic latency (IIR, no look-ahead).
struct Oversampler
{
    static constexpr int kMaxFactor = 8;
    int factor = 1, numStages = 0;
    SosFilter up[3], down[2][3]; // one shared up-chain, two down-chains (NORMAL, ALTERNATIVE)

    void setFactor (int f)
    {
        factor = std::clamp (f, 1, kMaxFactor);
        numStages = factor >= 8 ? 3 : factor >= 4 ? 2 : factor >= 2 ? 1 : 0;
        factor = 1 << numStages;
        const double (*designs[3])[6] = { kHalfband2x_sos, kHalfband4x_sos, kHalfband8x_sos };
        const int sections[3] = { kHalfband2x_numSections, kHalfband4x_numSections, kHalfband8x_numSections };
        for (int s = 0; s < 3; ++s) { up[s].set (designs[s], sections[s]); down[0][s].set (designs[s], sections[s]); down[1][s].set (designs[s], sections[s]); }
        reset();
    }
    void reset() { for (int s = 0; s < 3; ++s) { up[s].reset(); down[0][s].reset(); down[1][s].reset(); } }

    // x (host rate) -> out[0..factor-1] (engine rate)
    void upsample (double x, double* out)
    {
        double a[kMaxFactor], b[kMaxFactor]; a[0] = x; int n = 1;
        for (int s = 0; s < numStages; ++s)
        {
            for (int i = 0; i < n; ++i) { b[2 * i] = up[s].process (2.0 * a[i]); b[2 * i + 1] = up[s].process (0.0); }
            n *= 2; std::copy (b, b + n, a);
        }
        std::copy (a, a + n, out);
    }
    // in[0..factor-1] (engine rate) -> one host-rate sample
    double downsample (int chain, const double* in)
    {
        double a[kMaxFactor], b[kMaxFactor]; int n = factor; std::copy (in, in + n, a);
        for (int s = numStages - 1; s >= 0; --s)
        {
            for (int i = 0; i < n / 2; ++i) { down[chain][s].process (a[2 * i]); b[i] = down[chain][s].process (a[2 * i + 1]); }
            n /= 2; std::copy (b, b + n, a);
        }
        return a[0];
    }
};

// N first-order TPT all-pass sections sharing one nominal pole, with per-stage offset and a
// level-dependent pole term modelling the junction dynamic resistance r_e = V_T / I.
struct Ladder
{
    int N = kMaxStages;
    std::array<double, kMaxStages> s {}, xin {}, offMul {}, G {};
    double a1 = 0.0, a2 = 0.0, invVref = 1.0, drive = 0.0;

    void reset() { s.fill (0.0); xin.fill (0.0); }
    void setOffsets (const std::array<double, kMaxStages>& cents, double scale)
    {
        for (int i = 0; i < kMaxStages; ++i) offMul[(size_t) i] = std::exp2 (cents[(size_t) i] * scale / 1200.0);
    }
    // g = tan(pi f_c / f_engine). The nonlinearity uses the previous sample's stage inputs (one-sample delay
    // inside the nonlinearity only; the linear path is delay-free).
    void setCoefficients (double g)
    {
        for (int i = 0; i < N; ++i)
        {
            const double u = xin[(size_t) i] * invVref;
            double gi = g * offMul[(size_t) i] * (1.0 + drive * (a1 * u + a2 * u * u));
            gi = std::clamp (gi, 1.0e-5, 50.0);
            G[(size_t) i] = gi / (1.0 + gi);
        }
    }
    // Instantaneous linear form of the whole chain for the zero-delay feedback solve: y = A * u + B.
    void linearise (double& A, double& B) const
    {
        A = 1.0; B = 0.0;
        for (int i = 0; i < N; ++i) { const double ai = 2.0 * G[(size_t) i] - 1.0; B = ai * B + 2.0 * (1.0 - G[(size_t) i]) * s[(size_t) i]; A *= ai; }
    }
    double process (double u, int tapStage, double& tap)
    {
        double x = u; tap = u;
        for (int i = 0; i < N; ++i)
        {
            xin[(size_t) i] = x;
            const double v = (x - s[(size_t) i]) * G[(size_t) i];
            const double lp = v + s[(size_t) i];
            s[(size_t) i] = lp + v;
            x = 2.0 * lp - x;
            if (i + 1 == tapStage) tap = x;
        }
        return x;
    }
};

struct Lfo
{
    double phase = 0.0;
    OnePoleTPT hp;
    bool rising() const { return phase < 0.5; }
    double tick (double inc)
    {
        phase += inc; if (phase >= 1.0) phase -= std::floor (phase);
        return phase < 0.5 ? 4.0 * phase - 1.0 : 3.0 - 4.0 * phase;   // triangle in [-1, 1], rising first
    }
    void reset() { phase = 0.0; hp.reset(); }
};

// ---------------------------------------------------------------- the engine
class Engine
{
public:
    Tunables tun;
    Params   p;

    void prepare (double hostRate) { fs = hostRate; applyTunables(); applyFactor (chooseFactor(), true); reset(); }
    void reset()
    {
        ladder.reset(); lfo.reset(); ctrlSmooth.reset(); fbHp.reset(); fbLp.reset(); os.reset();
        zPrev = 0.0; coefCounter = 0; gCur = 0.0;
        inGainS = targetInGain(); fbGainS = targetFbGain(); depthS = p.lfoDepthPct * 0.01; wetSignS = p.color ? -1.0 : 1.0;
        smoothFirst = true;
    }
    void setParams (const Params& np)
    {
        const bool stagesChanged = np.stages != p.stages;
        p = np;
        if (stagesChanged) { ladder.N = std::clamp (p.stages, 1, kMaxStages); ladder.reset(); }
        updateDerived();
    }
    // Re-read the tunables (after a JSON reload).
    void applyTunables()
    {
        ladder.N = std::clamp (p.stages, 1, kMaxStages);
        ladder.a1 = tun.nl_a1; ladder.a2 = tun.nl_a2; ladder.invVref = 1.0 / std::max (1e-6, tun.nl_vref);
        updateDerived();
    }

    int    currentFactor() const { return os.factor; }
    double engineRate()    const { return fe; }
    double currentFc()     const { return fcCur; }     // for UI / diagnostics
    bool   lfoRising()     const { return lfo.rising(); }
    int    latencySamples() const { return 0; }

    // Host-rate block. in: mono audio; cv1 / cv2 (may be null): sample values, 1.0 = cv.fullscale_volts.
    void process (const float* in, const float* cv1, const float* cv2, float* outL, float* outR, int n)
    {
        const int wanted = chooseFactor();
        if (wanted != os.factor) applyFactor (wanted, false);
        const int F = os.factor;
        double upBuf[Oversampler::kMaxFactor], nBuf[Oversampler::kMaxFactor], aBuf[Oversampler::kMaxFactor];
        for (int i = 0; i < n; ++i)
        {
            const double x = (double) in[i];
            const double v1 = cv1 ? cv1[i] * tun.cv_fullscale_v : 0.0;
            const double v2 = cv2 ? cv2[i] * tun.cv_fullscale_v : 0.0;
            if (F == 1) { upBuf[0] = x; } else os.upsample (x, upBuf);
            for (int k = 0; k < F; ++k) tick (upBuf[k], v1, v2, nBuf[k], aBuf[k]);
            const double normal = F == 1 ? nBuf[0] : os.downsample (0, nBuf);
            const double alt    = F == 1 ? aBuf[0] : os.downsample (1, aBuf);
            switch (p.outputMode)
            {
                case 0:  outL[i] = (float) normal; outR[i] = (float) normal; break;
                case 1:  outL[i] = (float) alt;    outR[i] = (float) alt;    break;
                default: outL[i] = (float) normal; outR[i] = (float) alt;    break;
            }
        }
    }

    // One engine-rate sample. Exposed for the unit tests.
    void tick (double xRaw, double cv1V, double cv2V, double& normal, double& alt)
    {
        // ---- slow parameter smoothing (zipper-free gains)
        inGainS += (targetInGain() - inGainS) * pSmooth;
        fbGainS += (targetFbGain() - fbGainS) * pSmooth;
        depthS  += (p.lfoDepthPct * 0.01 - depthS) * pSmooth;
        wetSignS += ((p.color ? -1.0 : 1.0) - wetSignS) * pSmooth;

        // ---- control voltage
        const double lfoRaw = lfo.tick (lfoInc);
        const double lfoCv  = p.lfoSmooth ? lfo.hp.hp (lfoRaw) : (lfo.hp.s = 0.0, lfoRaw);
        const double vStatic = vPitch + cv1V + cv2V * cvAtt;
        const double vLfo    = tun.lfo_centre_v + tun.lfo_swing_v * lfoCv;
        double v = (1.0 - depthS) * vStatic + depthS * vLfo;
        if (smoothFirst) { ctrlSmooth.s = v; smoothFirst = false; }
        v = ctrlSmooth.lp (v);
        if (coefCounter <= 0)
        {
            coefCounter = std::max (1, (int) tun.coef_block);
            const double fExpo = tun.fc_ref_hz * std::exp2 (v / tun.volts_per_octave);
            double fc = tun.fc_floor_hz + fExpo * tun.fc_sat_hz / (fExpo + tun.fc_sat_hz);
            fc = std::clamp (fc, 1.0, 0.45 * fe);
            fcCur = fc;
            gCur = std::tan (M_PI * fc / fe);
            if (tun.fb_lp_ratio > 0.0) fbLp.setCutoff (fc * tun.fb_lp_ratio, fe);
            if (tun.fb_hp_ratio > 0.0) fbHp.setCutoff (std::max (tun.fb_hp_hz, fc * tun.fb_hp_ratio), fe);
        }
        --coefCounter;
        ladder.setCoefficients (gCur);

        // ---- zero-delay feedback solve: y = A u + B,  u = x + k sat(z),  z = cz y + dz (loop HP -> LP),
        //      sat linearised around the previous sample's z.
        const double x = xRaw * inGainS;
        double A, B; ladder.linearise (A, B);
        const double cz = fbLp.G * (1.0 - fbHp.G);
        const double dz = -fbLp.G * (1.0 - fbHp.G) * fbHp.s + (1.0 - fbLp.G) * fbLp.s;
        const double L  = std::max (1e-3, tun.fb_limit_level);
        const double th = std::tanh (zPrev / L);
        const double satPrev = L * th, sp = 1.0 - th * th;
        const double k = tun.fb_sign * fbGainS;
        double denom = 1.0 - A * k * sp * cz;
        if (denom < 0.1) denom = 0.1;
        const double y  = (A * x + A * k * (satPrev + sp * (dz - zPrev)) + B) / denom;
        const double z  = cz * y + dz;
        const double u  = x + k * L * std::tanh (z / L);
        double tap = 0.0;
        const double yReal = ladder.process (u, altTap, tap);
        zPrev = fbLp.lp (fbHp.hp (yReal));

        // ---- outputs
        const double wet = tun.wet_level * yReal;
        const double altWet = tun.alt_mode < 0.5 ? -wet : tun.wet_level * tap;
        if (p.mode == 1) { normal = tun.vibrato_gain * wet; alt = tun.vibrato_gain * altWet; }
        else             { normal = x + wetSignS * wet;     alt = x + altWet; }
    }

private:
    double fs = 48000.0, fe = 48000.0;
    Ladder ladder; Lfo lfo; OnePoleTPT ctrlSmooth, fbHp, fbLp; Oversampler os;
    double zPrev = 0.0, gCur = 0.0, fcCur = 0.0;
    int coefCounter = 0, altTap = 18;
    double vPitch = 0.0, cvAtt = 0.0, lfoInc = 0.0, pSmooth = 0.001;
    double inGainS = 1.0, fbGainS = 0.0, depthS = 0.0, wetSignS = 1.0;
    bool smoothFirst = true;

    double targetInGain() const { return p.inputDb <= -59.9f ? 0.0 : std::pow (10.0, std::min ((double) p.inputDb, tun.input_max_db) / 20.0); }
    double targetFbGain() const { return std::clamp ((double) p.resonancePct, 0.0, 100.0) * 0.01 * tun.fb_gain_max; }

    bool nonlinearEngaged() const { return p.resonancePct > tun.os_res_threshold || p.inputDb > tun.os_input_threshold_db; }
    int chooseFactor() const
    {
        if (p.oversampling >= 1) return 1 << (p.oversampling - 1);
        return (int) (nonlinearEngaged() ? tun.os_nonlinear_factor : tun.os_linear_factor);
    }
    void applyFactor (int f, bool force)
    {
        if (f == os.factor && ! force) return;
        os.setFactor (f);
        fe = fs * os.factor;
        updateDerived();
    }
    void updateDerived()
    {
        // PITCH (Hz) -> control volts by inverting the expo + 1/x law so the displayed Hz is exact at static settings.
        const double ft = std::clamp ((double) p.pitchHz - tun.fc_floor_hz, 1e-3, tun.fc_sat_hz * 0.999);
        const double fExpo = ft * tun.fc_sat_hz / (tun.fc_sat_hz - ft);
        vPitch = tun.volts_per_octave * std::log2 (fExpo / tun.fc_ref_hz);
        cvAtt  = p.cvAttPct * 0.01 * tun.cv_att_max;
        double rate = std::clamp ((double) p.lfoRateHz, tun.lfo_rate_min_hz, tun.lfo_rate_max_hz);
        if (p.lfoSlow) rate /= std::max (1.0, tun.lfo_slow_div);
        lfoInc = rate / fe;
        lfo.hp.setCutoff (tun.lfo_smooth_hp_hz, fe);
        ctrlSmooth.G = 1.0 - std::exp (-1.0 / (std::max (0.01, tun.ctrl_smooth_ms) * 1e-3 * fe));
        fbHp.setCutoff (tun.fb_hp_hz, fe); fbLp.setCutoff (tun.fb_lp_hz, fe);
        pSmooth = 1.0 - std::exp (-1.0 / (0.005 * fe));
        ladder.drive = std::clamp ((double) p.driveMatching, 0.0, 1.0);
        ladder.setOffsets (tun.stageOffsetCents, ladder.drive * tun.mismatch_scale);
        altTap = std::clamp ((int) tun.alt_tap_stage, 1, ladder.N);
    }
};

} // namespace pipe20
