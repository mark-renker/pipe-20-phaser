#include "PluginEditor.h"

Pipe20Editor::Pipe20Editor (Pipe20Processor& p) : AudioProcessorEditor (p), proc (p)
{
    auto knob = [&] (const char* id, const char* name)
    {
        auto k = std::make_unique<Knob>();
        k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 92, 18);
        k->label.setText (name, juce::dontSendNotification); k->label.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (k->slider); addAndMakeVisible (k->label);
        k->att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k->slider);
        knobs.push_back (std::move (k));
    };
    auto toggle = [&] (const char* id, const char* name)
    {
        auto t = std::make_unique<Toggle>(); t->button.setButtonText (name); addAndMakeVisible (t->button);
        t->att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, t->button);
        toggles.push_back (std::move (t));
    };
    auto choice = [&] (const char* id, const char* name)
    {
        auto c = std::make_unique<Choice>();
        if (auto* par = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id))) c->box.addItemList (par->choices, 1);
        c->label.setText (name, juce::dontSendNotification);
        addAndMakeVisible (c->box); addAndMakeVisible (c->label);
        c->att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, c->box);
        choices.push_back (std::move (c));
    };
    knob ("pitch", "PITCH"); knob ("resonance", "RESONANCE"); knob ("lfo_rate", "RATE"); knob ("lfo_depth", "DEPTH");
    knob ("cv_att", "CV ATT"); knob ("input", "INPUT"); knob ("drive", "DRIVE / MATCH");
    toggle ("lfo_slow", "L* slow"); toggle ("lfo_smooth", "SMOOTH"); toggle ("color", "COLOR");
    choice ("output", "Output"); choice ("mode", "Mode"); choice ("stages", "Stages"); choice ("oversampling", "Oversampling");

    title.setText ("Pipe-20 Phaser", juce::dontSendNotification); title.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    addAndMakeVisible (title);
    status.setFont (juce::FontOptions (12.0f)); status.setJustificationType (juce::Justification::centredLeft); addAndMakeVisible (status);
    reload.onClick = [this] { proc.tunablesStatus = proc.reloadTunables(); };
    addAndMakeVisible (reload);
    setSize (760, 360);
    startTimerHz (10);
}

void Pipe20Editor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff23262b));
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawRect (getLocalBounds(), 1);
}

void Pipe20Editor::resized()
{
    auto r = getLocalBounds().reduced (12);
    auto top = r.removeFromTop (26); title.setBounds (top.removeFromLeft (220));
    reload.setBounds (top.removeFromRight (130)); top.removeFromRight (8); status.setBounds (top);
    r.removeFromTop (8);
    auto row1 = r.removeFromTop (150);
    const int kw = row1.getWidth() / (int) knobs.size();
    for (auto& k : knobs) { auto c = row1.removeFromLeft (kw); k->label.setBounds (c.removeFromTop (18)); k->slider.setBounds (c); }
    r.removeFromTop (12);
    auto row2 = r.removeFromTop (28);
    const int tw = 110;
    for (auto& t : toggles) t->button.setBounds (row2.removeFromLeft (tw));
    row2.removeFromLeft (10);
    const int cw = (row2.getWidth()) / (int) choices.size();
    for (auto& c : choices) { auto cell = row2.removeFromLeft (cw); c->label.setBounds (cell.removeFromLeft (84)); c->box.setBounds (cell.reduced (2, 0)); }
}

void Pipe20Editor::timerCallback()
{
    status.setText (juce::String::formatted ("f_c %.0f Hz | engine %dx | LFO %s | CPU %.1f %% | ", proc.currentFc.load(), proc.engineFactor.load(),
                                             proc.lfoRising.load() ? "UP" : "DOWN", proc.cpuLoad.load() * 100.0f) + proc.tunablesStatus,
                    juce::dontSendNotification);
}
