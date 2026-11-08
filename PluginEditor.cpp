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
    setColour (juce::TextButton::buttonColourId, Colour (0xff12365a));
    setColour (juce::TextButton::buttonOnColourId, c);
    setColour (juce::TextButton::textColourOffId, c);
    setColour (juce::TextButton::textColourOnId, Colour (0xff06182b));
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

void CelesteLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos,
                                           float, float, juce::Slider::SliderStyle, juce::Slider&)
{
    const float cy = (float) y + (float) h * 0.5f;
    g.setColour (Colour (0xff1c3d5c));
    g.fillRoundedRectangle ((float) x, cy - 3.f, (float) w, 6.f, 3.f);
    g.setColour (celeste());
    g.fillRoundedRectangle ((float) x, cy - 3.f, sliderPos - (float) x, 6.f, 3.f);
    g.setColour (juce::Colours::white);
    g.fillEllipse (sliderPos - 9.f, cy - 9.f, 18.f, 18.f);
    g.setColour (celeste());
    g.drawEllipse (sliderPos - 9.f, cy - 9.f, 18.f, 18.f, 2.f);
}

//==============================================================================
CelesteChoirEditor::CelesteChoirEditor (CelesteChoirProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    setSize (900, 540);

    struct Def { const char* id; const char* name; const char* suffix; };
    const Def defs[kKnobs] =
    {
        { "formant",  "FORMANTE",   "" },
        { "ensemble", "ENSEMBLE",   " ct" },
        { "breath",   "AIRE",       "" },
        { "vibDepth", "VIBRATO",    " ct" },
        { "vibRate",  "VEL. VIB.",  " Hz" },
        { "gain",     "VOLUMEN",    " dB" },
        { "attack",   "ATAQUE",     " s" },
        { "decay",    "CAIDA",      " s" },
        { "sustain",  "SOSTENIDO",  "" },
        { "release",  "LIBERACION", " s" },
        { "rvSize",   "REVERB TAM.", "" },
        { "rvMix",    "REVERB MEZCLA", "" }
    };

    for (int i = 0; i < kKnobs; ++i)
    {
        auto& k = knobs[(size_t) i];
        k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 18);
        k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                      juce::MathConstants<float>::pi * 2.75f, true);
        k.slider.setTextValueSuffix (defs[i].suffix);
        k.slider.setNumDecimalPlacesToDisplay (2);
        addAndMakeVisible (k.slider);

        k.label.setText (defs[i].name, juce::dontSendNotification);
        k.label.setJustificationType (juce::Justification::centred);
        k.label.setFont (juce::Font (13.f, juce::Font::bold));
        addAndMakeVisible (k.label);

        k.att = std::make_unique<SliderAtt> (proc.apvts, defs[i].id, k.slider);
    }

    // Vocal
    vowelSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    vowelSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (vowelSlider);
    vowelAtt = std::make_unique<SliderAtt> (proc.apvts, "vowel", vowelSlider);

    const char* names[5] = { "A", "E", "I", "O", "U" };
    for (int i = 0; i < 5; ++i)
    {
        auto& b = vowelButtons[(size_t) i];
        b.setButtonText (names[i]);
        b.onClick = [this, i] { vowelSlider.setValue ((double) i, juce::sendNotificationSync); };
        addAndMakeVisible (b);
    }

    vowelSlider.onValueChange = [this]
    {
        const int nearest = juce::roundToInt (vowelSlider.getValue());
        for (int j = 0; j < 5; ++j)
            vowelButtons[(size_t) j].setToggleState (j == nearest, juce::dontSendNotification);
    };
    vowelSlider.onValueChange();

    // Tipo de voz
    voiceBox.addItemList ({ "Soprano", "Alto", "Tenor", "Bajo" }, 1);
    addAndMakeVisible (voiceBox);
    voiceAtt = std::make_unique<ComboAtt> (proc.apvts, "voice", voiceBox);

    voiceLabel.setText ("VOZ", juce::dontSendNotification);
    voiceLabel.setFont (juce::Font (13.f, juce::Font::bold));
    addAndMakeVisible (voiceLabel);
    vowelLabel.setText ("VOCAL  (arrastra para mezclar entre vocales)", juce::dontSendNotification);
    vowelLabel.setFont (juce::Font (13.f, juce::Font::bold));
    addAndMakeVisible (vowelLabel);
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

    // Titulo
    g.setColour (c);
    g.setFont (juce::Font (30.f, juce::Font::bold));
    g.drawText ("CELESTE CHOIR", 30, 14, 400, 40, juce::Justification::centredLeft);
    g.setColour (c.withAlpha (0.6f));
    g.setFont (juce::Font (13.f));
    g.drawText ("sintetizador de coro", 32, 50, 300, 20, juce::Justification::centredLeft);

    // Paneles
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.fillRoundedRectangle (20.f, 80.f, 860.f, 90.f, 14.f);
    g.fillRoundedRectangle (20.f, 185.f, 860.f, 165.f, 14.f);
    g.fillRoundedRectangle (20.f, 360.f, 860.f, 165.f, 14.f);
    g.setColour (c.withAlpha (0.35f));
    g.drawRoundedRectangle (20.f, 80.f, 860.f, 90.f, 14.f, 1.f);
    g.drawRoundedRectangle (20.f, 185.f, 860.f, 165.f, 14.f, 1.f);
    g.drawRoundedRectangle (20.f, 360.f, 860.f, 165.f, 14.f, 1.f);
}

void CelesteChoirEditor::resized()
{
    // Fila de vocal / voz
    voiceLabel.setBounds (40, 88, 100, 18);
    voiceBox.setBounds (40, 112, 150, 32);

    for (int i = 0; i < 5; ++i)
        vowelButtons[(size_t) i].setBounds (230 + i * 58, 100, 50, 50);

    vowelLabel.setBounds (540, 88, 330, 18);
    vowelSlider.setBounds (540, 112, 320, 32);

    // Perillas: 2 filas de 6
    const int colW = 140;
    const int startX = 30;
    const int rowY[2] = { 195, 370 };
    for (int i = 0; i < kKnobs; ++i)
    {
        const int row = i / 6, col = i % 6;
        auto& k = knobs[(size_t) i];
        k.label.setBounds (startX + col * colW, rowY[row], colW, 18);
        k.slider.setBounds (startX + col * colW, rowY[row] + 20, colW, 130);
    }
}
