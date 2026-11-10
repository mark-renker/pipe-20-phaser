#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace pipe20;

juce::AudioProcessorValueTreeState::ParameterLayout Pipe20Processor::makeLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;
    auto hz = [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 2) + " kHz" : String (v, v < 100.0f ? 1 : 0) + " Hz"; };
    NormalisableRange<float> pitchRange (20.0f, 20000.0f, 0.0f); pitchRange.setSkewForCentre (500.0f);
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID ("pitch", 1), "Pitch", pitchRange, 500.0f,
        AudioParameterFloatAttributes().withLabel ("Hz").withStringFromValueFunction (hz)));
    auto pct = [] (float v, int) { return String (v, 1) + " %"; };
    auto fromText = [] (const String& t) { return t.retainCharacters ("0123456789.-").getFloatValue(); };
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID ("resonance", 1), "Resonance", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (pct).withValueFromStringFunction (fromText)));
    NormalisableRange<float> rateRange (0.1f, 25.0f, 0.0f); rateRange.setSkewForCentre (1.5f);
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID ("lfo_rate", 1), "LFO Rate", rateRange, 1.0f,
        AudioParameterFloatAttributes().withLabel ("Hz").withStringFromValueFunction ([] (float v, int) { return String (v, 2) + " Hz"; })));
    ps.push_back (std::make_unique<AudioParameterBool> (ParameterID ("lfo_slow", 1), "LFO Slow (L*)", false));
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID ("lfo_depth", 1), "LFO Depth", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (pct).withValueFromStringFunction (fromText)));
    ps.push_back (std::make_unique<AudioParameterBool> (ParameterID ("lfo_smooth", 1), "LFO Smooth", false));
    ps.push_back (std::make_unique<AudioParameterBool> (ParameterID ("color", 1), "Color (invert wet)", false));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID ("output", 1), "Output", StringArray { "Normal", "Alternative", "Stereo" }, 2));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID ("mode", 1), "Mode", StringArray { "Phase", "Vibrato" }, 0));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID ("stages", 1), "Stages", StringArray { "12", "16", "18", "20" }, 3));
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID ("cv_att", 1), "CV Attenuverter", NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction ([] (float v, int) { const int i = (int) std::lround (v); return (i >= 0 ? "+" : "") + String (i) + " %"; }).withValueFromStringFunction (fromText)));
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID ("input", 1), "Input", NormalisableRange<float> (-60.0f, 6.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction ([] (float v, int) { return v <= -59.9f ? String ("-inf dB") : (v >= 0 ? "+" : "") + String (v, 1) + " dB"; }).withValueFromStringFunction ([] (const String& t) { return t.containsIgnoreCase ("inf") ? -60.0f : t.retainCharacters ("0123456789.-").getFloatValue(); })));
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID ("drive", 1), "Drive / Matching", NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v <= 0.0f ? String ("ideal") : String (v * 100.0f, 0) + " %"; }).withValueFromStringFunction ([] (const String& t) { return t.containsIgnoreCase ("ideal") ? 0.0f : t.retainCharacters ("0123456789.").getFloatValue() / 100.0f; })));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID ("oversampling", 1), "Oversampling", StringArray { "Auto", "1x", "2x", "4x", "8x" }, 0));
    return { ps.begin(), ps.end() };
}

Pipe20Processor::Pipe20Processor()
    : AudioProcessor (BusesProperties().withInput ("Audio In", juce::AudioChannelSet::stereo(), true)
                                       .withInput ("CV In", juce::AudioChannelSet::stereo(), false)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PIPE20", makeLayout())
{
    prm.pitch = apvts.getRawParameterValue ("pitch");   prm.res = apvts.getRawParameterValue ("resonance");
    prm.rate = apvts.getRawParameterValue ("lfo_rate"); prm.slow = apvts.getRawParameterValue ("lfo_slow");
    prm.depth = apvts.getRawParameterValue ("lfo_depth"); prm.smooth = apvts.getRawParameterValue ("lfo_smooth");
    prm.color = apvts.getRawParameterValue ("color");   prm.output = apvts.getRawParameterValue ("output");
    prm.mode = apvts.getRawParameterValue ("mode");     prm.stages = apvts.getRawParameterValue ("stages");
    prm.cvatt = apvts.getRawParameterValue ("cv_att");  prm.input = apvts.getRawParameterValue ("input");
    prm.drive = apvts.getRawParameterValue ("drive");   prm.os = apvts.getRawParameterValue ("oversampling");
    tunablesStatus = reloadTunables();
}

juce::File Pipe20Processor::userTunablesFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Pipe20Phaser").getChildFile ("tunables.json");
}

juce::String Pipe20Processor::reloadTunables()
{
    const juce::SpinLock::ScopedLockType sl (engineLock);
    Tunables t; auto f = userTunablesFile(); juce::String status;
    if (f.existsAsFile())
    {
        std::string err; std::vector<std::string> unknown;
        const int n = t.loadJsonText (f.loadFileAsString().toStdString(), &err, &unknown);
        if (n < 0) { status = "tunables: parse error (" + juce::String (err) + ") - using built-in defaults"; t = Tunables(); }
        else status = "tunables: " + juce::String (n) + " keys from " + f.getFullPathName() + (unknown.empty() ? "" : " (" + juce::String ((int) unknown.size()) + " unknown)");
    }
    else status = "tunables: built-in nominal defaults (override: " + f.getFullPathName() + ")";
    engine.tun = t; engine.applyTunables(); engine.reset();
    return status;
}

bool Pipe20Processor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet(), in = l.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    if (in != juce::AudioChannelSet::stereo() && in != juce::AudioChannelSet::mono()) return false;
    if (l.inputBuses.size() > 1)
    {
        const auto sc = l.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::stereo() && sc != juce::AudioChannelSet::mono()) return false;
    }
    return true;
}

Params Pipe20Processor::readParams() const
{
    Params p;
    p.pitchHz = prm.pitch->load(); p.resonancePct = prm.res->load(); p.lfoRateHz = prm.rate->load(); p.lfoSlow = prm.slow->load() > 0.5f;
    p.lfoDepthPct = prm.depth->load(); p.lfoSmooth = prm.smooth->load() > 0.5f; p.color = prm.color->load() > 0.5f;
    p.outputMode = (int) prm.output->load(); p.mode = (int) prm.mode->load();
    static const int stageTable[] = { 12, 16, 18, 20 }; p.stages = stageTable[juce::jlimit (0, 3, (int) prm.stages->load())];
    p.cvAttPct = prm.cvatt->load(); p.inputDb = prm.input->load(); p.driveMatching = prm.drive->load(); p.oversampling = (int) prm.os->load();
    return p;
}

void Pipe20Processor::prepareToPlay (double sr, int samplesPerBlock)
{
    const juce::SpinLock::ScopedLockType sl (engineLock);
    engine.p = readParams(); engine.prepare (sr);
    const size_t n = (size_t) juce::jmax (1, samplesPerBlock);
    mono.assign (n, 0.0f); cv1.assign (n, 0.0f); cv2.assign (n, 0.0f); outL.assign (n, 0.0f); outR.assign (n, 0.0f);
    setLatencySamples (engine.latencySamples()); // 0: the oversampling filters are minimum-phase IIR (no look-ahead)
}

void Pipe20Processor::processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int n = buf.getNumSamples();
    const juce::SpinLock::ScopedTryLockType sl (engineLock);
    if (! sl.isLocked()) { buf.clear(); return; }
    if ((int) mono.size() < n) { mono.resize ((size_t) n); cv1.resize ((size_t) n); cv2.resize ((size_t) n); outL.resize ((size_t) n); outR.resize ((size_t) n); }
    const auto t0 = juce::Time::getHighResolutionTicks();

    engine.setParams (readParams());
    auto in = getBusBuffer (buf, true, 0);
    const int inCh = in.getNumChannels();
    for (int i = 0; i < n; ++i) { float s = 0; for (int c = 0; c < inCh; ++c) s += in.getReadPointer (c)[i]; mono[(size_t) i] = inCh > 0 ? s / (float) inCh : 0.0f; }
    const float* c1 = nullptr; const float* c2 = nullptr;
    if (getBusCount (true) > 1)
    {
        auto sc = getBusBuffer (buf, true, 1);
        if (sc.getNumChannels() >= 1) c1 = sc.getReadPointer (0);
        if (sc.getNumChannels() >= 2) c2 = sc.getReadPointer (1);
    }
    float peak = 0.0f; for (int i = 0; i < n; ++i) peak = juce::jmax (peak, std::abs (mono[(size_t) i]));
    sigHold = peak > 0.002f ? 8 : juce::jmax (0, sigHold - 1); clipHold = peak > 0.98f ? 12 : juce::jmax (0, clipHold - 1);
    inSignal.store (sigHold > 0); inClip.store (clipHold > 0);
    oscillating.store (engine.p.resonancePct * 0.01 * engine.tun.fb_gain_max >= 1.0);
    engine.process (mono.data(), c1, c2, outL.data(), outR.data(), n);
    auto out = getBusBuffer (buf, false, 0);
    if (out.getNumChannels() >= 2) { out.copyFrom (0, 0, outL.data(), n); out.copyFrom (1, 0, outR.data(), n); }
    else if (out.getNumChannels() == 1) { out.copyFrom (0, 0, outL.data(), n); }

    const double sec = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
    const float load = (float) (sec / (n / getSampleRate()));
    cpuLoad.store (cpuLoad.load() * 0.9f + load * 0.1f);
    currentFc.store ((float) engine.currentFc()); engineFactor.store (engine.currentFactor()); lfoRising.store (engine.lfoRising()); depthActive.store (engine.p.lfoDepthPct > 0.5f);
}

void Pipe20Processor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}
void Pipe20Processor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size)) if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* Pipe20Processor::createEditor() { return new Pipe20Editor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Pipe20Processor(); }
