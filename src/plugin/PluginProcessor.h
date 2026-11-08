// Pipe-20 Phaser — JUCE processor: parameter binding (real units), CV sidechain bus, tunables loading.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../dsp/Phaser.h"

class Pipe20Processor : public juce::AudioProcessor
{
public:
    Pipe20Processor();
    ~Pipe20Processor() override = default;

    void prepareToPlay (double sr, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& l) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Pipe-20 Phaser"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::File userTunablesFile();
    juce::String reloadTunables();      // returns a status line for the UI
    juce::String tunablesStatus;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> cpuLoad { 0.0f }, currentFc { 0.0f };
    std::atomic<int> engineFactor { 1 };
    std::atomic<bool> lfoRising { true };

private:
    juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    pipe20::Params readParams() const;

    pipe20::Engine engine;
    juce::SpinLock engineLock;
    std::vector<float> mono, cv1, cv2, outL, outR;
    struct P { std::atomic<float>* pitch, *res, *rate, *slow, *depth, *smooth, *color, *output, *mode, *stages, *cvatt, *input, *drive, *os; } prm {};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Pipe20Processor)
};
