#pragma once
#include "PluginProcessor.h"
#include <array>

// Look & feel celeste
class CelesteLookAndFeel : public juce::LookAndFeel_V4
{
public:
    static juce::Colour celeste() { return juce::Colour (0xff7fd6ff); }

    CelesteLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float sliderPos,
                           float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
};

class CelesteChoirEditor : public juce::AudioProcessorEditor
{
public:
    explicit CelesteChoirEditor (CelesteChoirProcessor&);
    ~CelesteChoirEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAtt> att;
    };

    CelesteChoirProcessor& proc;
    CelesteLookAndFeel lnf;

    static constexpr int kKnobs = 12;
    std::array<Knob, kKnobs> knobs;

    juce::Slider vowelSlider;
    std::unique_ptr<SliderAtt> vowelAtt;
    std::array<juce::TextButton, 5> vowelButtons;

    juce::ComboBox voiceBox;
    std::unique_ptr<ComboAtt> voiceAtt;
    juce::Label voiceLabel, vowelLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CelesteChoirEditor)
};
