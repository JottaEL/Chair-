#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>

// Efecto: convierte una voz en un coro / multitud de voces.
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

    const juce::String getName() const override { return "Celeste Choir FX"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

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
    static constexpr int kMaxVoices = 8;

    struct Voice
    {
        float pan = 0.f, gain = 1.f;
        float detuneRand = 0.f, timingRand = 1.f, toneRand = 0.5f;
        float d[2] { 0.f, 0.f };        // posicion de las 2 cabezas del pitch shifter
        float T = 0.f;                   // retardo base actual (muestras)
        float drift = 0.f, driftTarget = 0.f;
        int   driftCount = 0;
        float lp = 0.f, lpCoef = 0.3f;
        float gl = 0.7f, gr = 0.7f;
    };

    std::array<Voice, kMaxVoices> voices;
    std::vector<float> buf;
    int mask = 0, writePos = 0;
    float W = 1440.f;                    // tamano de grano (muestras)
    double sr = 44100.0;

    juce::AudioBuffer<float> wetBuf;
    juce::Random rnd;
    juce::Reverb reverb;
    juce::SmoothedValue<float> mixS, gainS;
};
