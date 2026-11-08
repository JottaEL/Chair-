#include "PluginEditor.h"

using juce::Colour;

//==============================================================================
CelesteLookAndFeel::CelesteLookAndFeel()
{
    const auto c = celeste();
    setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, c);
    setColour (juce::ComboBox::backgroundColourId, Colour (0xff12365a));
    setColour (juce::ComboBox::textColourId, juce::Colours::white);
    setColour (juce::ComboBox::outlineColourId, c);
    setColour (juce::ComboBox::arrowColourId, c);
    setColour (juce::PopupMenu::backgroundColourId, Colour (0xff0d2a47));
    setColour (juce::PopupMenu::textColourId, juce::Colours::white);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, c);
    setColour (juce::PopupMenu::highlightedTextColourId, Colour (0xff06182b));
}

void CelesteLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                           float startAngle, float endAngle, juce::Slider&)
{
    auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.f);
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float ang = startAngle + pos * (endAngle - startAngle);

    juce::Path track;
    track.addCentredArc (c.x, c.y, r, r, 0.f, startAngle, endAngle, true);
    g.setColour (Colour (0xff1c3d5c));
    g.strokePath (track, juce::PathStrokeType (5.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path val;
    val.addCentredArc (c.x, c.y, r, r, 0.f, startAngle, ang, true);
    g.setColour (celeste());
    g.strokePath (val, juce::PathStrokeType (5.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float kr = r - 11.f;
    g.setGradientFill (juce::ColourGradient (Colour (0xff2f6a99), c.x, c.y - kr,
                                             Colour (0xff0e2a45), c.x, c.y + kr, false));
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2.f, kr * 2.f);
    g.setColour (celeste().withAlpha (0.55f));
    g.drawEllipse (c.x - kr, c.y - kr, kr * 2.f, kr * 2.f, 1.5f);

    juce::Path p;
    p.addRoundedRectangle (-1.8f, -kr + 3.f, 3.6f, kr * 0.5f, 1.8f);
    g.setColour (juce::Colours::white);
    g.fillPath (p, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
}

//==============================================================================
CelesteChoirEditor::CelesteChoirEditor (CelesteChoirProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    setSize (900, 540);

    struct Def { const char* id; const char* name; const char* suffix; int decimals; };
    const Def defs[kKnobs] =
    {
        { "voices", "VOCES",     "",     0 },
        { "detune", "DESAFINE",  " ct",  1 },
        { "timing", "RETARDO",   " ms",  1 },
        { "human",  "HUMANIZAR", "",     2 },
        { "tone",   "TONO",      " Hz",  0 },
        { "spread", "ABRIR",     "",     2 },
        { "mix",    "MEZCLA",    "",     2 },
        { "rvSize", "SALA",      "",     2 },
        { "rvMix",  "REVERB",    "",     2 },
        { "gain",   "VOLUMEN",   " dB",  1 }
    };

    for (int i = 0; i < kKnobs; ++i)
    {
        auto& k = knobs[(size_t) i];
        k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 18);
        k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                      juce::MathConstants<float>::pi * 2.75f, true);
        k.slider.setTextValueSuffix (defs[i].suffix);
        k.slider.setNumDecimalPlacesToDisplay (defs[i].decimals);
        addAndMakeVisible (k.slider);

        k.label.setText (defs[i].name, juce::dontSendNotification);
        k.label.setJustificationType (juce::Justification::centred);
        k.label.setFont (juce::Font (13.f, juce::Font::bold));
        addAndMakeVisible (k.label);

        k.att = std::make_unique<SliderAtt> (proc.apvts, defs[i].id, k.slider);
    }

    harmonyBox.addItemList ({ "Unisono (multitud)", "Mayor", "Menor", "Quintas y octavas" }, 1);
    addAndMakeVisible (harmonyBox);
    harmonyAtt = std::make_unique<ComboAtt> (proc.apvts, "harmony", harmonyBox);

    harmonyLabel.setText ("ARMONIA", juce::dontSendNotification);
    harmonyLabel.setFont (juce::Font (13.f, juce::Font::bold));
    addAndMakeVisible (harmonyLabel);
}

CelesteChoirEditor::~CelesteChoirEditor()
{
    setLookAndFeel (nullptr);
}

void CelesteChoirEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (Colour (0xff0a2038), 0.f, 0.f,
                                             Colour (0xff134a78), (float) getWidth(), (float) getHeight(), false));
    g.fillAll();

    const auto c = CelesteLookAndFeel::celeste();

    g.setColour (c);
    g.setFont (juce::Font (30.f, juce::Font::bold));
    g.drawText ("CELESTE CHOIR FX", 30, 14, 500, 40, juce::Justification::centredLeft);
    g.setColour (c.withAlpha (0.6f));
    g.setFont (juce::Font (13.f));
    g.drawText ("efecto que convierte tu voz en un coro", 32, 50, 400, 20, juce::Justification::centredLeft);

    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.fillRoundedRectangle (20.f, 80.f, 860.f, 90.f, 14.f);
    g.fillRoundedRectangle (20.f, 185.f, 860.f, 165.f, 14.f);
    g.fillRoundedRectangle (20.f, 360.f, 860.f, 165.f, 14.f);
    g.setColour (c.withAlpha (0.35f));
    g.drawRoundedRectangle (20.f, 80.f, 860.f, 90.f, 14.f, 1.f);
    g.drawRoundedRectangle (20.f, 185.f, 860.f, 165.f, 14.f, 1.f);
    g.drawRoundedRectangle (20.f, 360.f, 860.f, 165.f, 14.f, 1.f);

    g.setColour (juce::Colours::white.withAlpha (0.75f));
    g.setFont (juce::Font (14.f));
    g.drawFittedText ("Habla o canta por la entrada: el plugin la multiplica en varias voces "
                      "con desafine, retardo y posicion distintos, como si fueran muchas personas.",
                      340, 92, 520, 66, juce::Justification::centredLeft, 3);
}

void CelesteChoirEditor::resized()
{
    harmonyLabel.setBounds (40, 88, 200, 18);
    harmonyBox.setBounds (40, 112, 270, 34);

    const int colW = 170;
    const int startX = 25;
    const int rowY[2] = { 195, 370 };
    for (int i = 0; i < kKnobs; ++i)
    {
        const int row = i / 5, col = i % 5;
        auto& k = knobs[(size_t) i];
        k.label.setBounds (startX + col * colW, rowY[row], colW, 18);
        k.slider.setBounds (startX + col * colW, rowY[row] + 20, colW, 130);
    }
}
