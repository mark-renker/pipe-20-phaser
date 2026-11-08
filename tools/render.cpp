// Pipe-20 Phaser — standalone WAV -> WAV renderer for the measurement scripts (no JUCE).
//
//   pipe20_render in.wav out.wav [options]
//     --pitch Hz  --resonance %  --rate Hz  --slow 0|1  --depth %  --smooth 0|1  --color 0|1
//     --output normal|alternative|stereo   --mode phase|vibrato   --stages 12|16|18|20
//     --cvatt %   --input dB   --drive 0..1   --os auto|1|2|4|8
//     --cv cv.wav          channel 0 = 1 V/oct CV, channel 1 = attenuverted CV (1.0 = cv.fullscale_volts)
//     --tunables file.json --set key=value (repeatable)   --dump-tunables   --tail seconds   --block n
//     --bench              CPU benchmark: 10 s of noise at 48 kHz in 64-sample blocks, prints % of one core
// Output: 2-channel float WAV at the input rate (L/R per --output; default stereo = NORMAL / ALTERNATIVE).
#include "../src/dsp/Phaser.h"
#include "../src/dsp/WavFile.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace pipe20;

static void usage() { std::fprintf (stderr, "usage: pipe20_render in.wav out.wav [--pitch Hz] [--resonance %%] ... (see source header)\n"); }

int main (int argc, char** argv)
{
    Params p; p.outputMode = 2; Tunables tun; std::string in, out, cvPath; double tail = 0.0; int block = 512; bool bench = false, dump = false;
    std::vector<std::pair<std::string, double>> sets;
    for (int i = 1; i < argc; ++i)
    {
        std::string a = argv[i];
        auto next = [&] () -> std::string { if (i + 1 >= argc) { usage(); std::exit (2); } return argv[++i]; };
        if (a == "--pitch") p.pitchHz = (float) std::atof (next().c_str());
        else if (a == "--resonance") p.resonancePct = (float) std::atof (next().c_str());
        else if (a == "--rate") p.lfoRateHz = (float) std::atof (next().c_str());
        else if (a == "--slow") p.lfoSlow = std::atoi (next().c_str()) != 0;
        else if (a == "--depth") p.lfoDepthPct = (float) std::atof (next().c_str());
        else if (a == "--smooth") p.lfoSmooth = std::atoi (next().c_str()) != 0;
        else if (a == "--color") p.color = std::atoi (next().c_str()) != 0;
        else if (a == "--output") { std::string v = next(); p.outputMode = v == "normal" ? 0 : v == "alternative" ? 1 : 2; }
        else if (a == "--mode") p.mode = next() == "vibrato" ? 1 : 0;
        else if (a == "--stages") p.stages = std::atoi (next().c_str());
        else if (a == "--cvatt") p.cvAttPct = (float) std::atof (next().c_str());
        else if (a == "--input") p.inputDb = (float) std::atof (next().c_str());
        else if (a == "--drive") p.driveMatching = (float) std::atof (next().c_str());
        else if (a == "--os") { std::string v = next(); p.oversampling = v == "auto" ? 0 : v == "1" ? 1 : v == "2" ? 2 : v == "4" ? 3 : 4; }
        else if (a == "--cv") cvPath = next();
        else if (a == "--tunables") { std::string err; if (! tun.loadJsonFile (next(), &err)) { std::fprintf (stderr, "tunables: %s\n", err.c_str()); return 2; } }
        else if (a == "--set") { std::string kv = next(); auto eq = kv.find ('='); if (eq == std::string::npos) { usage(); return 2; } sets.emplace_back (kv.substr (0, eq), std::atof (kv.c_str() + eq + 1)); }
        else if (a == "--dump-tunables") dump = true;
        else if (a == "--tail") tail = std::atof (next().c_str());
        else if (a == "--block") block = std::max (1, std::atoi (next().c_str()));
        else if (a == "--bench") bench = true;
        else if (a.rfind ("--", 0) == 0) { std::fprintf (stderr, "unknown option %s\n", a.c_str()); usage(); return 2; }
        else if (in.empty()) in = a; else if (out.empty()) out = a; else { usage(); return 2; }
    }
    for (auto& s : sets) if (! tun.set (s.first, s.second)) { std::fprintf (stderr, "unknown tunable %s\n", s.first.c_str()); return 2; }
    if (dump) { std::printf ("%s", tun.toJson().c_str()); return 0; }

    Engine e; e.tun = tun; e.p = p;
    if (bench)
    {
        e.prepare (48000.0);
        const int n = 48000 * 10; std::vector<float> x ((size_t) n), l ((size_t) n), r ((size_t) n);
        unsigned s = 1; for (auto& v : x) { s = s * 1664525u + 1013904223u; v = ((s >> 8) / 8388608.0f - 1.0f) * 0.3f; }
        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < n; i += 64) e.process (x.data() + i, nullptr, nullptr, l.data() + i, r.data() + i, 64);
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("{ \"engine_factor\": %d, \"seconds_audio\": 10, \"seconds_cpu\": %.4f, \"cpu_percent_one_core\": %.3f }\n", e.currentFactor(), sec, sec / 10.0 * 100.0);
        return 0;
    }
    if (in.empty() || out.empty()) { usage(); return 2; }
    Wav w; std::string err;
    if (! readWav (in, w, &err)) { std::fprintf (stderr, "%s\n", err.c_str()); return 1; }
    Wav cv; const bool hasCv = ! cvPath.empty();
    if (hasCv && ! readWav (cvPath, cv, &err)) { std::fprintf (stderr, "%s\n", err.c_str()); return 1; }
    const size_t frames = w.frames() + (size_t) (tail * w.sampleRate);
    std::vector<float> mono (frames, 0.0f), cv1 (frames, 0.0f), cv2 (frames, 0.0f), l (frames), r (frames);
    for (size_t i = 0; i < w.frames(); ++i) { double s = 0; for (auto& c : w.ch) s += c[i]; mono[i] = (float) (s / (double) w.ch.size()); }
    if (hasCv) for (size_t i = 0; i < frames; ++i) { const size_t j = std::min (i, cv.frames() - 1); if (! cv.ch.empty()) cv1[i] = cv.ch[0][j]; if (cv.ch.size() > 1) cv2[i] = cv.ch[1][j]; }
    e.prepare (w.sampleRate);
    for (size_t i = 0; i < frames; i += (size_t) block)
    {
        const int n = (int) std::min ((size_t) block, frames - i);
        e.process (mono.data() + i, hasCv ? cv1.data() + i : nullptr, hasCv ? cv2.data() + i : nullptr, l.data() + i, r.data() + i, n);
    }
    Wav o; o.sampleRate = w.sampleRate; o.ch = { l, r };
    if (! writeWav (out, o, &err)) { std::fprintf (stderr, "%s\n", err.c_str()); return 1; }
    std::fprintf (stderr, "rendered %zu frames at %.0f Hz, engine %dx (%.0f Hz), f_c %.1f Hz\n", frames, w.sampleRate, e.currentFactor(), e.engineRate(), e.currentFc());
    return 0;
}
