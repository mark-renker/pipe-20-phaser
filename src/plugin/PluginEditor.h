// Pipe-20 Phaser — editor.
//
// Original rack-panel design (spec 6 / 9: no Haible or Random*Source panel artwork or trade dress). Generic
// hardware cues only — rack ears, pointer knobs with scale rings, toggle switches, LEDs, engraved section
// legends — with a matte-black rack look of a 1980 rack unit in general, but a layout, typography and legend set of its own. Drawn at a base
// size and scaled, so the window is freely resizable with a fixed aspect ratio.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace pipe20ui
{
// ---- palette ---------------------------------------------------------------------------------
static const juce::Colour kPanel      (0xff202225);   // matte black rack panel (1980 rack-unit character)
static const juce::Colour kPanelDark  (0xff17191b);
static const juce::Colour kPanelLight (0xff2b2e32);
static const juce::Colour kEar        (0xff0f1012);
static const juce::Colour kInk        (0xfff3f0e8);   // white silkscreen legends
static const juce::Colour kInk2       (0xff9da1a7);
static const juce::Colour kRule       (0xff3f4349);
static const juce::Colour kAccent     (0xffe3a64c);   // amber
static const juce::Colour kKnob       (0xff2c2f34);   // black knob cap
static const juce::Colour kKnobEdge   (0xffb9b3a7);   // silver skirt
static const juce::Colour kPointer    (0xfff4f1ea);   // white pointer line
static const juce::Colour kRecess     (0xff0d0e10);
static const juce::Colour kLcd        (0xfff2ead4);
static const juce::Colour kAmber      (0xfff2a33a);
static const juce::Colour kGreen      (0xff3cc46c);
static const juce::Colour kRed        (0xffe4473c);

inline juce::Font font (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}
inline juce::Font legend (float size) { return font (size, true).withExtraKerningFactor (0.08f); }

// ---- knob look -------------------------------------------------------------------------------
struct ModeKnob : juce::Slider { using Slider::Slider; };

class Look : public juce::LookAndFeel_V4
{
public:
    Look()
    {
        setColour (juce::Slider::textBoxTextColourId, kLcd);
        setColour (juce::Slider::textBoxBackgroundColourId, kRecess);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff454b54));
        setColour (juce::Slider::textBoxHighlightColourId, kAccent.withAlpha (0.5f));
        setColour (juce::Label::textColourId, kInk);
        setColour (juce::ComboBox::backgroundColourId, kRecess);
        setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff454b54));
        setColour (juce::ComboBox::textColourId, kLcd);
        setColour (juce::ComboBox::arrowColourId, kAccent);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1d2025));
        setColour (juce::PopupMenu::textColourId, kLcd);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, kAccent);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::TextEditor::textColourId, kLcd);
        setColour (juce::TextEditor::highlightColourId, kAccent.withAlpha (0.5f));
        setColour (juce::CaretComponent::caretColourId, kLcd);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        const bool mode = dynamic_cast<ModeKnob*> (&s) != nullptr;
        auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        const float d = juce::jmin (r.getWidth(), r.getHeight());
        const float cx = r.getCentreX(), cy = r.getCentreY();
        const float rOuter = d * 0.5f;
        const float rBody  = d * 0.33f;
        const float ang = a0 + pos * (a1 - a0);

        // scale ring: ticks (+ numerals for large knobs)
        const int nTicks = mode ? (int) std::lround (s.getMaximum() - s.getMinimum()) + 1 : 11;
        g.setColour (kInk);
        for (int i = 0; i < nTicks; ++i)
        {
            const float a = a0 + (a1 - a0) * (float) i / (float) (nTicks - 1);
            const bool major = mode || (i % 5 == 0);
            const float len = major ? d * 0.05f : d * 0.03f;
            const float r1 = rBody + d * 0.045f;
            juce::Point<float> p1 (cx + std::sin (a) * r1, cy - std::cos (a) * r1);
            juce::Point<float> p2 (cx + std::sin (a) * (r1 + len), cy - std::cos (a) * (r1 + len));
            g.drawLine ({ p1, p2 }, major ? 1.6f : 1.0f);
            if (! mode && d >= 64 && major)
            {
                g.setFont (font (d * 0.085f, true));
                juce::Point<float> pt (cx + std::sin (a) * (rOuter - d * 0.04f), cy - std::cos (a) * (rOuter - d * 0.04f));
                g.drawText (juce::String (i), juce::Rectangle<float> (20, 12).withCentre (pt), juce::Justification::centred);
            }
        }
        // body: shadow ring + cream cap with a soft highlight
        g.setColour (kRecess.withAlpha (0.6f));
        g.fillEllipse (cx - rBody - 1.5f, cy - rBody + 1.5f, 2 * rBody + 3.0f, 2 * rBody + 3.0f);
        g.setColour (kKnobEdge);
        g.fillEllipse (cx - rBody, cy - rBody, 2 * rBody, 2 * rBody);
        juce::ColourGradient grad (kKnob.brighter (0.45f), cx - rBody * 0.5f, cy - rBody * 0.6f, kKnob.darker (0.35f), cx + rBody * 0.6f, cy + rBody * 0.8f, true);
        g.setGradientFill (grad);
        g.fillEllipse (cx - rBody * 0.93f, cy - rBody * 0.93f, 2 * rBody * 0.93f, 2 * rBody * 0.93f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawEllipse (cx - rBody * 0.80f, cy - rBody * 0.80f, 2 * rBody * 0.80f, 2 * rBody * 0.80f, 1.0f);
        // pointer
        juce::Path ptr;
        ptr.addRoundedRectangle (-d * 0.018f, -rBody * 0.92f, d * 0.036f, rBody * 0.55f, d * 0.012f);
        g.setColour (kPointer);
        g.fillPath (ptr, juce::AffineTransform::rotation (ang).translated (cx, cy));
        // active arc (copper) for continuous knobs
        if (! mode)
        {
            juce::Path arc;
            arc.addCentredArc (cx, cy, rBody + d * 0.025f, rBody + d * 0.025f, 0.0f, a0, ang, true);
            g.setColour (kAccent);
            g.strokePath (arc, juce::PathStrokeType (d * 0.022f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        l->setFont (font (11.5f));
        l->setJustificationType (juce::Justification::centred);
        return l;
    }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (11.5f); }
    juce::Font getPopupMenuFont() override { return font (12.5f); }
    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (box.findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
        juce::Path tri;
        const float ax = (float) w - 11.0f, ay = (float) h * 0.5f;
        tri.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
        g.setColour (box.findColour (juce::ComboBox::arrowColourId));
        g.fillPath (tri);
    }
    void positionComboBoxText (juce::ComboBox& box, juce::Label& l) override
    {
        l.setBounds (6, 0, box.getWidth() - 24, box.getHeight());
        l.setFont (getComboBoxFont (box));
    }
};

// ---- toggle switch bound to a 2-state parameter ------------------------------------------------
class ToggleSwitch : public juce::Component
{
public:
    ToggleSwitch (juce::RangedAudioParameter& p, juce::String top, juce::String bottom)
        : legendTop (std::move (top)), legendBottom (std::move (bottom)),
          att (p, [this] (float v) { state = v > 0.5f; repaint(); })
    {
        att.sendInitialUpdate();
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }
    void mouseDown (const juce::MouseEvent&) override { att.setValueAsCompleteGesture (state ? 0.0f : 1.0f); }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        const float cx = r.getCentreX();
        g.setColour (kInk);
        g.setFont (font (9.5f, true));
        g.drawText (legendTop, r.removeFromTop (12.0f), juce::Justification::centred);
        g.drawText (legendBottom, r.removeFromBottom (12.0f), juce::Justification::centred);
        auto bez = juce::Rectangle<float> (22.0f, r.getHeight() - 4.0f).withCentre (r.getCentre());
        g.setColour (kPanelLight);
        g.fillRoundedRectangle (bez, 4.0f);
        g.setColour (kRule);
        g.drawRoundedRectangle (bez, 4.0f, 1.0f);
        auto slot = juce::Rectangle<float> (8.0f, bez.getHeight() - 14.0f).withCentre (bez.getCentre());
        g.setColour (kRecess);
        g.fillRoundedRectangle (slot, 4.0f);
        const float leverLen = slot.getHeight() * 0.62f;
        const float y0 = slot.getCentreY();
        const float y1 = state ? y0 + leverLen : y0 - leverLen;   // lever points to the active legend (top = state 0)
        g.setColour (kKnobEdge);
        g.drawLine (cx, y0, cx, y1, 5.0f);
        g.setColour (kAccent);
        g.fillEllipse (cx - 6.0f, y1 - 6.0f, 12.0f, 12.0f);
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.fillEllipse (cx - 3.0f, y1 - 4.5f, 4.0f, 3.0f);
    }
private:
    juce::String legendTop, legendBottom;
    bool state = false;
    juce::ParameterAttachment att;
};

// ---- small panel push button -----------------------------------------------------------------
class PanelButton : public juce::Component
{
public:
    PanelButton (juce::String text, std::function<void()> fn) : label (std::move (text)), onClick (std::move (fn))
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }
    void mouseDown (const juce::MouseEvent&) override { down = true; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override { down = false; repaint(); if (contains (e.getPosition()) && onClick) onClick(); }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (kRecess);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (down ? kPanelDark : kPanelLight);
        g.fillRoundedRectangle (r.reduced (1.0f), 3.0f);
        g.setColour (kInk);
        g.setFont (font (10.0f, true).withExtraKerningFactor (0.06f));
        g.drawText (label, r, juce::Justification::centred);
    }
private:
    juce::String label; std::function<void()> onClick; bool down = false;
};

// ---- the panel (base-size content; scaled by the editor) ---------------------------------------
class Panel : public juce::Component, private juce::Timer
{
public:
    static constexpr int W = 1180, H = 340;
    static constexpr int kEarW = 24;

    explicit Panel (Pipe20Processor& p) : proc (p)
    {
        setLookAndFeel (&look);
        auto& ap = proc.apvts;
        auto param = [&] (const char* id) -> juce::RangedAudioParameter& { return *ap.getParameter (id); };

        auto knob = [&] (juce::Slider& s, const char* id, std::unique_ptr<SA>& att, int tbW)
        {
            s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
            s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, tbW, 16);
            s.setLookAndFeel (&look);
            addAndMakeVisible (s);
            att = std::make_unique<SA> (ap, id, s);
        };
        knob (pitch,     "pitch",     aPitch, 78);
        knob (resonance, "resonance", aResonance, 60);
        knob (rate,      "lfo_rate",  aRate, 66);
        knob (depth,     "lfo_depth", aDepth, 60);
        knob (cvAtt,     "cv_att",    aCvAtt, 56);
        knob (input,     "input",     aInput, 62);
        knob (drive,     "drive",     aDrive, 56);

        auto modeKnob = [&] (ModeKnob& s, const char* id, std::unique_ptr<SA>& att, float a0, float a1)
        {
            s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.setRotaryParameters (juce::MathConstants<float>::pi * a0, juce::MathConstants<float>::pi * a1, true);
            s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            s.setLookAndFeel (&look);
            addAndMakeVisible (s);
            att = std::make_unique<SA> (ap, id, s);
        };
        modeKnob (output, "output", aOutput, 1.6f, 2.4f);   // 3 positions
        modeKnob (stages, "stages", aStages, 1.5f, 2.5f);   // 4 positions

        slow   = std::make_unique<ToggleSwitch> (param ("lfo_slow"),   "NORM", "SLOW");
        smooth = std::make_unique<ToggleSwitch> (param ("lfo_smooth"), "HARD", "SMOOTH");
        color  = std::make_unique<ToggleSwitch> (param ("color"),      "NORMAL", "COLOR");
        vibe   = std::make_unique<ToggleSwitch> (param ("mode"),       "PHASE", "VIBE");
        for (auto* t : { slow.get(), smooth.get(), color.get(), vibe.get() }) addAndMakeVisible (t);

        if (auto* pc = dynamic_cast<juce::AudioParameterChoice*> (ap.getParameter ("oversampling"))) os.addItemList (pc->choices, 1);
        os.setLookAndFeel (&look);
        addAndMakeVisible (os);
        aOs = std::make_unique<CA> (ap, "oversampling", os);

        reload = std::make_unique<PanelButton> ("RELOAD TUNABLES", [this] { proc.tunablesStatus = proc.reloadTunables(); });
        addAndMakeVisible (*reload);

        setSize (W, H);
        startTimerHz (30);
    }
    ~Panel() override
    {
        for (auto* c : getChildren()) c->setLookAndFeel (nullptr);
        setLookAndFeel (nullptr);
    }

    // ---- layout (base coordinates) ----
    struct Section { const char* title; int x0, x1; };
    static constexpr Section kSections[] = {
        { "INPUT", 40, 150 }, { "PITCH", 160, 380 }, { "RESONANCE", 390, 510 }, { "LFO", 520, 790 },
        { "CV", 800, 900 }, { "OUTPUT", 910, 1020 }, { "LADDER", 1030, 1140 } };

    void resized() override
    {
        auto circle = [] (int cx, int cy, int d) { return juce::Rectangle<int> (d, d + 18).withCentre ({ cx, cy + 9 }); }; // + textbox
        input.setBounds (circle (95, 150, 70));
        pitch.setBounds (circle (270, 172, 132));
        resonance.setBounds (circle (450, 156, 88));
        rate.setBounds (circle (590, 156, 88));
        depth.setBounds (circle (700, 156, 88));
        cvAtt.setBounds (circle (850, 150, 70));
        drive.setBounds (circle (1085, 226, 50));
        output.setBounds (juce::Rectangle<int> (60, 60).withCentre ({ 965, 254 }));
        stages.setBounds (juce::Rectangle<int> (56, 56).withCentre ({ 1085, 150 }));
        slow->setBounds (560, 232, 44, 72);
        smooth->setBounds (716, 232, 44, 72);
        color->setBounds (926, 108, 44, 72);
        vibe->setBounds (970, 108, 44, 72);
        os.setBounds (1040, 284, 90, 20);
        reload->setBounds (W - kEarW - 124, 12, 108, 20);
    }

    void paint (juce::Graphics& g) override
    {
        // panel + brushed texture
        g.fillAll (kPanel);
        {
            juce::ColourGradient grad (kPanelLight, 0, 0, kPanelDark, 0, (float) H, false);
            g.setGradientFill (grad);
            g.fillRect (getLocalBounds());
            juce::Random rnd (11);
            for (int y = 0; y < H; y += 2)
            {
                g.setColour (juce::Colours::white.withAlpha (rnd.nextFloat() * 0.035f));
                g.drawHorizontalLine (y, (float) kEarW, (float) (W - kEarW));
            }
        }
        // rack ears
        for (int side = 0; side < 2; ++side)
        {
            auto ear = juce::Rectangle<int> (side == 0 ? 0 : W - kEarW, 0, kEarW, H);
            g.setColour (kEar);
            g.fillRect (ear);
            g.setColour (kEar.brighter (0.35f));
            g.drawVerticalLine (side == 0 ? kEarW - 1 : W - kEarW, 0.0f, (float) H);
            for (int yy : { 26, H - 26 })
            {
                auto s = juce::Rectangle<float> (10, 10).withCentre ({ (float) ear.getCentreX(), (float) yy });
                g.setColour (juce::Colour (0xff8d9199));
                g.fillEllipse (s);
                g.setColour (kEar.darker (0.4f));
                g.drawLine (s.getX() + 2, s.getCentreY(), s.getRight() - 2, s.getCentreY(), 1.4f);
            }
        }
        // header
        g.setColour (kInk);
        g.setFont (legend (17.0f));
        g.drawText ("PIPE-20 PHASER", 42, 16, 420, 24, juce::Justification::centredLeft);
        g.setColour (kInk2);
        g.setFont (font (10.5f));
        g.drawText ("twenty-stage all-pass phaser  /  1 V/oct  /  self-oscillating", 44, 38, 420, 14, juce::Justification::centredLeft);
        g.setColour (kAccent);
        g.fillRoundedRectangle (42.0f, 58.0f, (float) (W - 2 * kEarW - 36), 2.0f, 1.0f);
        g.setColour (kInk2);
        g.setFont (font (10.0f));
        g.drawText (status, W - kEarW - 560, 36, 540, 14, juce::Justification::centredRight);

        // sections
        for (auto& s : kSections)
        {
            g.setColour (kInk);
            g.setFont (legend (9.5f));
            g.drawText (s.title, s.x0, 70, s.x1 - s.x0, 14, juce::Justification::centred);
            g.setColour (kRule);
            g.drawRoundedRectangle ((float) s.x0, 88.0f, (float) (s.x1 - s.x0), 222.0f, 6.0f, 1.0f);
            g.setColour (kPanelDark.withAlpha (0.6f));
            g.drawRoundedRectangle ((float) s.x0 + 1, 89.0f, (float) (s.x1 - s.x0) - 2, 220.0f, 5.0f, 1.0f);
        }
        auto cap = [&] (const juce::String& t, int cx, int y) { g.setColour (kInk); g.setFont (legend (9.0f)); g.drawText (t, cx - 60, y, 120, 12, juce::Justification::centred); };
        auto note = [&] (const juce::String& t, int cx, int y, int w = 120) { g.setColour (kInk2); g.setFont (font (8.5f)); g.drawText (t, cx - w / 2, y, w, 11, juce::Justification::centred); };
        cap ("LEVEL", 95, 98);
        cap ("PITCH", 270, 90);      note ("centre frequency of the 20 stages", 270, 292, 200);
        cap ("RESONANCE", 450, 96);  note ("self-oscillates past the mark", 450, 292, 118);
        cap ("RATE", 590, 96);       cap ("DEPTH", 700, 96);
        cap ("L*", 582, 220);        cap ("SHAPE", 738, 220);
        cap ("ATT", 850, 98);        note ("bus 2: 1 V/oct + att", 850, 232, 96); note ("10 V = full scale", 850, 246, 96);
        cap ("WET", 948, 94);        cap ("MODE", 992, 94);   cap ("OUTPUT", 965, 190);
        cap ("STAGES", 1085, 96);    cap ("DRIVE / MATCH", 1085, 190); cap ("ENGINE", 1085, 272);

        // mode legends around the selectors
        auto ring = [&] (const char* const* names, int n, float a0, float a1, float cx, float cy, float rx, float ry)
        {
            g.setColour (kInk);
            g.setFont (font (9.0f, true));
            for (int i = 0; i < n; ++i)
            {
                const float a = a0 + (a1 - a0) * (float) i / (float) (n - 1);
                juce::Point<float> pt (cx + std::sin (a) * rx, cy + 2.0f - std::cos (a) * ry);
                g.drawText (names[i], juce::Rectangle<float> (48, 12).withCentre (pt), juce::Justification::centred);
            }
        };
        static const char* outs[] = { "NORMAL", "ALT", "STEREO" };
        ring (outs, 3, juce::MathConstants<float>::pi * 1.6f, juce::MathConstants<float>::pi * 2.4f, 965.0f, 254.0f, 54.0f, 44.0f);
        static const char* stg[] = { "12", "16", "18", "20" };
        ring (stg, 4, juce::MathConstants<float>::pi * 1.5f, juce::MathConstants<float>::pi * 2.5f, 1085.0f, 150.0f, 44.0f, 38.0f);

        // LEDs
        drawLed (g, 112, 236, proc.inSignal.load(), kGreen);  g.setColour (kInk2); g.setFont (font (8.5f, true)); g.drawText ("SIG", 98, 246, 28, 10, juce::Justification::centred);
        drawLed (g, 112, 272, proc.inClip.load(), kRed);      g.setColour (kInk2); g.drawText ("CLIP", 98, 282, 28, 10, juce::Justification::centred);
        drawLed (g, 450, 250, proc.oscillating.load(), kAmber); g.setColour (kInk2); g.drawText ("OSC", 436, 260, 28, 10, juce::Justification::centred);
        const bool up = proc.lfoRising.load(), run = proc.depthActive.load();
        drawLed (g, 630, 104, run && ! up, kAmber);  g.setColour (kInk2); g.setFont (font (8.5f, true)); g.drawText ("DOWN", 612, 112, 36, 10, juce::Justification::centred);
        drawLed (g, 662, 104, run && up, kAmber);    g.setColour (kInk2); g.drawText ("UP", 648, 112, 28, 10, juce::Justification::centred);
    }

private:
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    static void drawLed (juce::Graphics& g, int cx, int cy, bool on, juce::Colour c)
    {
        auto r = juce::Rectangle<float> (10, 10).withCentre ({ (float) cx, (float) cy });
        g.setColour (kRecess);
        g.fillEllipse (r.expanded (2.0f));
        g.setColour (on ? c : c.withAlpha (0.2f));
        g.fillEllipse (r);
        if (on) { g.setColour (c.withAlpha (0.3f)); g.fillEllipse (r.expanded (4.0f)); }
    }

    void timerCallback() override
    {
        const float fc = proc.currentFc.load();
        status = juce::String::formatted ("f_c %s   engine %dx   cpu %.1f %%   ",
                                          (fc >= 1000.0f ? juce::String (fc / 1000.0f, 2) + " kHz" : juce::String (fc, 0) + " Hz").toRawUTF8(),
                                          proc.engineFactor.load(), 100.0f * proc.cpuLoad.load())
               + proc.tunablesStatus.upToFirstOccurrenceOf (" (", false, false).replace ("tunables: ", "");
        repaint (600, 94, 90, 30);
        repaint (90, 226, 50, 70);
        repaint (430, 240, 40, 32);
        repaint (W - kEarW - 560, 36, 540, 14);
    }

    Pipe20Processor& proc;
    Look look;
    juce::Slider pitch, resonance, rate, depth, cvAtt, input, drive;
    ModeKnob output, stages;
    juce::ComboBox os;
    std::unique_ptr<ToggleSwitch> slow, smooth, color, vibe;
    std::unique_ptr<PanelButton> reload;
    std::unique_ptr<SA> aPitch, aResonance, aRate, aDepth, aCvAtt, aInput, aDrive, aOutput, aStages;
    std::unique_ptr<CA> aOs;
    juce::String status;
};
} // namespace pipe20ui

class Pipe20Editor : public juce::AudioProcessorEditor
{
public:
    explicit Pipe20Editor (Pipe20Processor& p) : AudioProcessorEditor (p), panel (p)
    {
        addAndMakeVisible (panel);
        setResizable (true, true);
        getConstrainer()->setFixedAspectRatio ((double) pipe20ui::Panel::W / pipe20ui::Panel::H);
        setResizeLimits (pipe20ui::Panel::W * 0.6, pipe20ui::Panel::H * 0.6, pipe20ui::Panel::W * 2, pipe20ui::Panel::H * 2);
        setSize (pipe20ui::Panel::W, pipe20ui::Panel::H);
    }
    void resized() override
    {
        const float sc = (float) getWidth() / (float) pipe20ui::Panel::W;
        panel.setTransform (juce::AffineTransform::scale (sc));
        panel.setBounds (0, 0, pipe20ui::Panel::W, pipe20ui::Panel::H);
    }
    void paint (juce::Graphics& g) override { g.fillAll (pipe20ui::kEar); }
    pipe20ui::Panel panel;
};
