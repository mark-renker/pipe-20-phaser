// Pipe-20 Phaser — plain generic-style editor (no hardware artwork, per spec 6).
#pragma once
#include "PluginProcessor.h"

class Pipe20Editor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit Pipe20Editor (Pipe20Processor&);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    Pipe20Processor& proc;
    struct Knob { juce::Slider slider; juce::Label label; std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att; };
    struct Toggle { juce::ToggleButton button; std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att; };
    struct Choice { juce::ComboBox box; juce::Label label; std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att; };
    std::vector<std::unique_ptr<Knob>> knobs; std::vector<std::unique_ptr<Toggle>> toggles; std::vector<std::unique_ptr<Choice>> choices;
    juce::Label status, title; juce::TextButton reload { "Reload tunables" };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Pipe20Editor)
};
