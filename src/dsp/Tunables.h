// Pipe-20 Phaser — every behavioural constant that must come from measurement lives here.
// Defaults are NOMINAL (derived from the spec, published build notes and the control-law
// topology), not measured. Each is loaded from resources/tunables.json (or the user override
// file) so that re-tuning after a hardware capture is a JSON edit, not a recompile.
// JSON key: "<block>.<quantity>"; the plugin and the renderer both read the same file.
#pragma once
#include "TinyJson.h"
#include <array>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cmath>

namespace pipe20 {

constexpr int kMaxStages = 20;

// X(field, "json.key", default, "note")
#define PIPE20_TUNABLES(X) \
 X(stages,              "ladder.stages",               20,      "Stage count N (build constant; 12/16/18/20 selectable). Confirm by counting notches below 20 kHz (8.1).") \
 X(mismatch_scale,      "ladder.mismatch_scale",       1.0,     "Multiplier on the per-stage pole offset vector (ladder.stage_offset_cents) at Drive/matching = 1.") \
 X(nl_a1,               "ladder.nl_a1",                0.10,    "Junction-resistance term: omega_i = omega0 * (1 + a1*u + a2*u^2), u = stage input / nl_vref. Fit to the THD sweep (8.4).") \
 X(nl_a2,               "ladder.nl_a2",               -0.03,    "Second-order junction term; sets sign and size of the level-dependent first-notch shift (8.4).") \
 X(nl_vref,             "ladder.nl_vref",              1.0,     "Sample value at which u = 1 (full scale = io.volts_at_fullscale).") \
 X(wet_level,           "mix.wet_level",               1.0,     "Wet gain into the NORMAL / ALTERNATIVE sums (ideal complementary sum = 1).") \
 X(alt_mode,            "mix.alt_mode",                0,       "ALTERNATIVE derivation: 0 = dry - wet (sign flip), 1 = dry + output of stage mix.alt_tap_stage. Open question 10.3.") \
 X(alt_tap_stage,       "mix.alt_tap_stage",           18,      "Stage whose output feeds ALTERNATIVE when mix.alt_mode = 1.") \
 X(vibrato_gain,        "mix.vibrato_gain",            1.0,     "Wet-only output gain in Phase/Vibrato mode.") \
 X(fc_ref_hz,           "control.fc_ref_hz",           440.0,   "Expo converter reference: f_expo = fc_ref * 2^(V / volts_per_octave).") \
 X(volts_per_octave,    "control.volts_per_octave",    1.0,     "Tempco-compensated 1 V/oct scale (8.2).") \
 X(fc_sat_hz,           "control.fc_sat_hz",           60000.0, "1/x generator compression: f_c = fc_floor + f_expo * fc_sat / (f_expo + fc_sat). Fit from the CV sweep (8.2).") \
 X(fc_floor_hz,         "control.fc_floor_hz",         10.0,    "Lower asymptote of f_c (control-current floor).") \
 X(ctrl_smooth_ms,      "control.smooth_ms",           2.0,     "One-pole smoothing time constant on the summed control voltage.") \
 X(coef_block,          "control.coef_block",          1,       "Engine samples between coefficient recomputes (1 or 8, spec 6).") \
 X(cv_fullscale_v,      "cv.fullscale_volts",          10.0,    "Sample value 1.0 on the CV bus = this many volts (ES-8/ES-9 class DC-coupled interface).") \
 X(cv_att_max,          "cv.attenuverter_max",         1.0,     "Gain of the attenuverted CV input at +100 %.") \
 X(lfo_rate_min_hz,     "lfo.rate_min_hz",             0.1,     "RATE knob minimum (measure, 8.6).") \
 X(lfo_rate_max_hz,     "lfo.rate_max_hz",             25.0,    "RATE knob maximum (measure, 8.6).") \
 X(lfo_slow_div,        "lfo.slow_divisor",            10.0,    "Rate divisor in L* slow mode (measure).") \
 X(lfo_swing_v,         "lfo.swing_volts",             3.0,     "Peak LFO control voltage at Depth = 100 % (sweep = 2 * swing / volts_per_octave octaves).") \
 X(lfo_centre_v,        "lfo.centre_volts",            0.0,     "Control voltage at the LFO midpoint (Depth = 100 % removes PITCH+CV, spec 3).") \
 X(lfo_smooth_hp_hz,    "lfo.smooth_hp_hz",            0.5,     "SMOOTH: one-pole high-pass corner on the LFO CV (8.6, within 20 %).") \
 X(fb_sign,             "feedback.sign",               1.0,     "+1 = positive feedback (peaks resonate), -1 = negative (notches resonate). Open question 10.2.") \
 X(fb_gain_max,         "feedback.gain_max",           1.18,    "Loop gain at Resonance = 100 %. Oscillation onset is at loop gain 1 => onset % = 100 / gain_max (8.3).") \
 X(fb_limit_level,      "feedback.limit_level",        0.5,     "Soft limiter (tanh) level in the loop, full-scale units; sets the oscillation amplitude (8.3).") \
 X(fb_hp_hz,            "feedback.hp_hz",              8.0,     "AC-coupling corner in the loop (floor; see feedback.hp_ratio).") \
 X(fb_lp_hz,             "feedback.lp_hz",              14000.0, "Loop low-pass corner in Hz when feedback.lp_ratio = 0 (op-amp roll-off / nice-screaming trim).") \
 X(fb_lp_ratio,          "feedback.lp_ratio",           0.7,     "If > 0 the loop low-pass corner tracks f_c (corner = ratio * f_c): fixes the oscillating mode across CV so the pitch tracks 1 V/oct (8.3). 0 = fixed feedback.lp_hz.") \
 X(fb_hp_ratio,          "feedback.hp_ratio",           0.05,    "If > 0 the loop high-pass corner tracks f_c (corner = max(feedback.hp_hz, ratio * f_c)), suppressing the near-DC mode the AC coupling would otherwise let through at high f_c. With lp_ratio this selects which loop mode f_c*tan(m*pi/N) oscillates (default: mode 1).") \
 X(io_volts_fullscale,  "io.volts_at_fullscale",       10.0,    "Peak volts represented by sample value 1.0 (Euro: 10 Vpk = +19.2 dBu). Maps dBu sweeps to dBFS.") \
 X(input_max_db,         "io.input_max_db",             6.0,     "INPUT attenuator maximum (dB).") \
 X(os_res_threshold,    "oversample.resonance_pct",    50.0,    "Auto: switch to the nonlinear-rate factor above this Resonance %.") \
 X(os_input_threshold_db,"oversample.input_db",        0.0,     "Auto: switch to the nonlinear-rate factor above this INPUT setting (dB).") \
 X(os_linear_factor,    "oversample.linear_factor",    4,       "Engine rate factor when the nonlinearity is not engaged (1, 2, 4, 8). Bilinear warp of the top notch: see docs/DEVIATIONS.md.") \
 X(os_nonlinear_factor, "oversample.nonlinear_factor", 4,       "Engine rate factor when Resonance / INPUT exceed the thresholds (1, 2, 4, 8).")

struct Tunables
{
#define X(field, key, def, note) double field = def;
    PIPE20_TUNABLES(X)
#undef X
    // Per-stage pole offsets in cents (THAT300 / BCM847-class matching; the measured vector replaces this).
    std::array<double, kMaxStages> stageOffsetCents {
        { 4, -7, 2, -3, 9, -5, 1, -8, 6, -2, 3, -6, 7, -1, 5, -9, 2, -4, 8, -3 } };

    struct Entry { const char* key; double* ptr; double def; const char* note; };
    std::vector<Entry> entries()
    {
        return {
#define X(field, key, def, note) Entry { key, &field, def, note },
            PIPE20_TUNABLES(X)
#undef X
        };
    }

    // Accepts "key": number or "key": { "value": number, ... }. Returns the number of keys applied, -1 on parse error.
    int loadJsonText (const std::string& text, std::string* err = nullptr, std::vector<std::string>* unknown = nullptr)
    {
        JsonValue root;
        if (! JsonParser::parse (text, root, err)) return -1;
        if (! root.isObject()) { if (err) *err = "root is not an object"; return -1; }
        auto es = entries();
        int applied = 0;
        for (auto& kv : root.obj)
        {
            if (! kv.first.empty() && kv.first[0] == '_') continue;
            const JsonValue* v = &kv.second;
            if (v->isObject()) { if (auto* inner = v->get ("value")) v = inner; }
            if (kv.first == "ladder.stage_offset_cents")
            {
                if (v->type == JsonValue::Array)
                    for (size_t i = 0; i < v->arr.size() && i < (size_t) kMaxStages; ++i)
                        if (v->arr[i].isNumber()) { stageOffsetCents[i] = v->arr[i].num; }
                ++applied; continue;
            }
            bool found = false;
            for (auto& e : es)
                if (kv.first == e.key) { if (v->isNumber()) { *e.ptr = v->num; ++applied; } found = true; break; }
            if (! found && unknown) unknown->push_back (kv.first);
        }
        return applied;
    }
    bool loadJsonFile (const std::string& path, std::string* err = nullptr)
    {
        std::ifstream f (path);
        if (! f) { if (err) *err = "cannot open " + path; return false; }
        std::stringstream ss; ss << f.rdbuf();
        return loadJsonText (ss.str(), err) >= 0;
    }
    std::string toJson (bool withNotes = true)
    {
        std::string o = "{\n  \"_comment\": \"Pipe-20 Phaser behavioural tunables. Values are NOMINAL until replaced by hardware measurements (docs/measurement_protocol.md).\",\n";
        for (auto& e : entries())
        {
            o += "  \"" + std::string (e.key) + "\": ";
            if (withNotes)
                o += "{ \"value\": " + jsonNumber (*e.ptr) + ", \"default\": " + jsonNumber (e.def) + ", \"note\": \"" + jsonEscape (e.note) + "\" }";
            else
                o += jsonNumber (*e.ptr);
            o += ",\n";
        }
        o += "  \"ladder.stage_offset_cents\": [";
        for (int i = 0; i < kMaxStages; ++i) o += (i ? ", " : "") + jsonNumber (stageOffsetCents[(size_t) i]);
        o += "]\n}\n";
        return o;
    }
    bool set (const std::string& key, double value)
    {
        for (auto& e : entries()) if (key == e.key) { *e.ptr = value; return true; }
        return false;
    }
};

} // namespace pipe20
