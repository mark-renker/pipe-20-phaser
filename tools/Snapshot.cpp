// Renders the editor to a PNG without a host (docs/screenshot.png, UI checks).
//   pipe20_snapshot out.png [scale]
#include <juce_gui_extra/juce_gui_extra.h>
#include "../src/plugin/PluginProcessor.h"
#include "../src/plugin/PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::File out (argc > 1 ? juce::String (argv[1]) : juce::String ("pipe20_ui.png"));
    const float scale = argc > 2 ? (float) juce::String (argv[2]).getDoubleValue() : 2.0f;
    Pipe20Processor proc;
    proc.prepareToPlay (48000.0, 64);
    // a few parameters off their defaults so the controls show state
    proc.apvts.getParameter ("resonance")->setValueNotifyingHost (0.45f);
    proc.apvts.getParameter ("lfo_depth")->setValueNotifyingHost (0.6f);
    proc.apvts.getParameter ("lfo_smooth")->setValueNotifyingHost (1.0f);
    proc.apvts.getParameter ("output")->setValueNotifyingHost (1.0f);
    proc.inSignal.store (true); proc.lfoRising.store (true); proc.currentFc.store (500.0f); proc.engineFactor.store (4);
    pipe20ui::Panel panel (proc);
    const int w = juce::roundToInt (pipe20ui::Panel::W * scale), h = juce::roundToInt (pipe20ui::Panel::H * scale);
    juce::Image img (juce::Image::ARGB, w, h, true);
    {
        juce::Graphics g (img);
        g.addTransform (juce::AffineTransform::scale (scale));
        panel.paintEntireComponent (g, true);
    }
    juce::PNGImageFormat png;
    juce::FileOutputStream fo (out);
    if (fo.openedOk()) png.writeImageToStream (img, fo);
    std::printf ("wrote %s (%dx%d)\n", out.getFullPathName().toRawUTF8(), w, h);
    return 0;
}
