// Pipe-20 Phaser — minimal RIFF/WAVE reader (PCM 16/24/32, float 32/64, incl. WAVE_FORMAT_EXTENSIBLE)
// and float-32 writer for the headless renderer and the unit tests. No external dependencies.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace pipe20 {

struct Wav
{
    double sampleRate = 48000.0;
    std::vector<std::vector<float>> ch; // ch[channel][frame]
    size_t frames() const { return ch.empty() ? 0 : ch[0].size(); }
};

inline bool readWav (const std::string& path, Wav& out, std::string* err = nullptr)
{
    FILE* f = std::fopen (path.c_str(), "rb");
    if (! f) { if (err) *err = "cannot open " + path; return false; }
    std::vector<uint8_t> d;
    { uint8_t buf[65536]; size_t n; while ((n = std::fread (buf, 1, sizeof buf, f)) > 0) d.insert (d.end(), buf, buf + n); }
    std::fclose (f);
    if (d.size() < 12 || std::memcmp (d.data(), "RIFF", 4) != 0 || std::memcmp (d.data() + 8, "WAVE", 4) != 0)
    { if (err) *err = "not a RIFF/WAVE file: " + path; return false; }
    auto u16 = [&] (size_t p) { return (uint32_t) d[p] | ((uint32_t) d[p + 1] << 8); };
    auto u32 = [&] (size_t p) { return u16 (p) | (u16 (p + 2) << 16); };
    int fmtTag = 0, channels = 0, bits = 0; uint32_t rate = 0;
    size_t dataPos = 0, dataLen = 0;
    for (size_t p = 12; p + 8 <= d.size();)
    {
        uint32_t len = u32 (p + 4);
        const char* id = (const char*) d.data() + p;
        size_t body = p + 8;
        if (std::memcmp (id, "fmt ", 4) == 0 && body + 16 <= d.size())
        {
            fmtTag = (int) u16 (body); channels = (int) u16 (body + 2); rate = u32 (body + 4); bits = (int) u16 (body + 14);
            if (fmtTag == 0xFFFE && len >= 40) fmtTag = (int) u16 (body + 24); // extensible: sub-format GUID first 2 bytes
        }
        else if (std::memcmp (id, "data", 4) == 0) { dataPos = body; dataLen = std::min<size_t> (len, d.size() - body); }
        p = body + len + (len & 1);
    }
    if (channels <= 0 || dataPos == 0) { if (err) *err = "missing fmt/data chunk: " + path; return false; }
    const int bytes = bits / 8;
    const size_t frames = dataLen / (size_t) (bytes * channels);
    out.sampleRate = rate; out.ch.assign ((size_t) channels, std::vector<float> (frames));
    const uint8_t* s = d.data() + dataPos;
    for (size_t i = 0; i < frames; ++i)
        for (int c = 0; c < channels; ++c, s += bytes)
        {
            float v = 0.0f;
            if (fmtTag == 3 && bits == 32) { float x; std::memcpy (&x, s, 4); v = x; }
            else if (fmtTag == 3 && bits == 64) { double x; std::memcpy (&x, s, 8); v = (float) x; }
            else if (bits == 16) { int16_t x = (int16_t) (s[0] | (s[1] << 8)); v = x / 32768.0f; }
            else if (bits == 24) { int32_t x = (int32_t) ((uint32_t) s[0] << 8 | (uint32_t) s[1] << 16 | (uint32_t) s[2] << 24) >> 8; v = x / 8388608.0f; }
            else if (bits == 32) { int32_t x; std::memcpy (&x, s, 4); v = (float) (x / 2147483648.0); }
            else if (bits == 8) { v = (s[0] - 128) / 128.0f; }
            out.ch[(size_t) c][i] = v;
        }
    return true;
}

inline bool writeWav (const std::string& path, const Wav& w, std::string* err = nullptr)
{
    const uint32_t channels = (uint32_t) w.ch.size(), frames = (uint32_t) w.frames();
    const uint32_t dataLen = frames * channels * 4, rate = (uint32_t) (w.sampleRate + 0.5);
    std::vector<uint8_t> d; d.reserve (58 + dataLen);
    auto p16 = [&] (uint32_t v) { d.push_back ((uint8_t) v); d.push_back ((uint8_t) (v >> 8)); };
    auto p32 = [&] (uint32_t v) { p16 (v & 0xFFFF); p16 (v >> 16); };
    auto tag = [&] (const char* t) { d.insert (d.end(), t, t + 4); };
    tag ("RIFF"); p32 (4 + 26 + 12 + 8 + dataLen); tag ("WAVE");
    tag ("fmt "); p32 (18); p16 (3); p16 (channels); p32 (rate); p32 (rate * channels * 4); p16 ((uint16_t) (channels * 4)); p16 (32); p16 (0);
    tag ("fact"); p32 (4); p32 (frames);
    tag ("data"); p32 (dataLen);
    for (uint32_t i = 0; i < frames; ++i)
        for (uint32_t c = 0; c < channels; ++c) { float v = w.ch[c][i]; uint32_t u; std::memcpy (&u, &v, 4); p32 (u); }
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) { if (err) *err = "cannot write " + path; return false; }
    std::fwrite (d.data(), 1, d.size(), f); std::fclose (f);
    return true;
}

} // namespace pipe20
