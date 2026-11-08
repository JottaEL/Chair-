#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cstdint>

class CelesteChoirProcessor : public juce::AudioProcessor
{
public:
    CelesteChoirProcessor();
    ~CelesteChoirProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Celeste Choir"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

private:
    static constexpr int kVoices = 16;
    static constexpr int kFormants = 4;
    static constexpr int kChunk = 32;

    struct BPCoef  { float g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0; };
    struct BPState { float s1 = 0, s2 = 0; };

    struct Voice
    {
        bool held = false;        // tecla presionada
        bool sustained = false;   // mantenida por el pedal
        int note = -1;
        float vel = 0.f;
        float baseFreq = 440.f;
        uint64_t age = 0;
        float vibPhase = 0.f, vibAmt = 0.f;
        float phase[2][3] {};
        BPState st[2][kFormants] {};
        juce::ADSR env;
    };

    struct Params
    {
        float vowel = 0, formantShift = 1, ensemble = 15, breath = 0.15f;
        float vibDepth = 20, vibRate = 5.2f;
        float gainLin = 0.5f;
        int voiceType = 0;
        juce::ADSR::Parameters adsr;
    } params;

    void readParams();
    void handleMidi (const juce::MidiMessage& m);
    Voice* allocateVoice();
    void render (juce::AudioBuffer<float>& buffer, int start, int end);
    void computeFormants (float vowel, BPCoef* coefs, float* gains) const;

    std::array<Voice, kVoices> voices;
    uint64_t ageCounter = 0;
    bool pedalDown = false;
    double sr = 44100.0;

    juce::SmoothedValue<float> vowelSmooth;
    juce::Random rnd;
    juce::Reverb reverb;
};
