#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

// Intervalos (semitonos) por voz para cada modo de armonia
static const int kInterval[4][8] =
{
    { 0, 0, 0, 0, 0, 0, 0, 0 },          // Unisono (multitud)
    { 0, 4, 7, -5, 12, 0, -12, 7 },      // Mayor
    { 0, 3, 7, -5, 12, 0, -12, 7 },      // Menor
    { 0, 7, 12, -12, 0, 7, -5, 12 }      // Quintas y octavas
};

static const float kPan[8] = { -0.9f, 0.9f, -0.5f, 0.5f, -0.2f, 0.2f, -0.75f, 0.75f };

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout CelesteChoirProcessor::createLayout()
{
    using namespace juce;
    using NR = NormalisableRange<float>;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto f = [&p] (const char* id, const char* name, NR range, float def)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, def));
    };

    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "voices", 1 }, "Voces", 2, 8, 6));
    f ("detune", "Desafine",  NR (0.f, 50.f, 0.1f), 14.f);
    f ("timing", "Retardo",   NR (0.f, 50.f, 0.1f), 22.f);
    f ("human",  "Humanizar", NR (0.f, 1.f, 0.001f), 0.5f);
    f ("tone",   "Tono",      NR (1500.f, 16000.f, 1.f, 0.4f), 9000.f);
    f ("spread", "Abrir",     NR (0.f, 1.f, 0.001f), 0.8f);
    f ("mix",    "Mezcla",    NR (0.f, 1.f, 0.001f), 0.5f);
    f ("rvSize", "Sala",      NR (0.f, 1.f, 0.001f), 0.5f);
    f ("rvMix",  "Reverb",    NR (0.f, 1.f, 0.001f), 0.25f);
    f ("gain",   "Volumen",   NR (-18.f, 9.f, 0.1f), 0.f);

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "harmony", 1 }, "Armonia",
                 StringArray { "Unisono (multitud)", "Mayor", "Menor", "Quintas y octavas" }, 0));

    return { p.begin(), p.end() };
}

//==============================================================================
CelesteChoirProcessor::CelesteChoirProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

bool CelesteChoirProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo();
}

void CelesteChoirProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    const int size = juce::nextPowerOfTwo ((int) (sr * 0.3));
    buf.assign ((size_t) size, 0.f);
    mask = size - 1;
    writePos = 0;
    W = (float) (0.030 * sr);

    juce::Random r (42);
    for (int i = 0; i < kMaxVoices; ++i)
    {
        Voice v;
        v.pan = kPan[i];
        v.gain = 0.7f + 0.3f * r.nextFloat();
        v.detuneRand = ((i % 2 == 0) ? -1.f : 1.f) * (0.3f + 0.7f * r.nextFloat());
        v.timingRand = 0.2f + 0.8f * r.nextFloat();
        v.toneRand = r.nextFloat();
        v.d[0] = W * ((float) i / (float) kMaxVoices);
        v.d[1] = std::fmod (v.d[0] + W * 0.5f, W);
        v.driftTarget = r.nextFloat() * 2.f - 1.f;
        v.drift = v.driftTarget;
        v.driftCount = 1 + (int) (r.nextFloat() * (float) sr * 0.5f);
        voices[(size_t) i] = v;
    }

    wetBuf.setSize (2, juce::jmax (samplesPerBlock, 512));
    reverb.setSampleRate (sr);
    reverb.reset();

    mixS.reset (sr, 0.03);
    gainS.reset (sr, 0.03);
    mixS.setCurrentAndTargetValue (apvts.getRawParameterValue ("mix")->load());
    gainS.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("gain")->load()));
}

void CelesteChoirProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || n == 0)
        return;

    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const int   nv        = juce::jlimit (2, kMaxVoices, (int) std::lround (get ("voices")));
    const float detune    = get ("detune");
    const float timingMs  = get ("timing");
    const float human     = get ("human");
    const float toneHz    = get ("tone");
    const float spread    = get ("spread");
    const int   harmony   = juce::jlimit (0, 3, (int) std::lround (get ("harmony")));

    mixS.setTargetValue (get ("mix"));
    gainS.setTargetValue (juce::Decibels::decibelsToGain (get ("gain")));

    juce::Reverb::Parameters rp;
    rp.roomSize   = get ("rvSize");
    rp.damping    = 0.5f;
    rp.wetLevel   = get ("rvMix") * 0.6f;
    rp.dryLevel   = 1.0f;
    rp.width      = 1.0f;
    rp.freezeMode = 0.f;
    reverb.setParameters (rp);

    if (wetBuf.getNumSamples() < n)
        wetBuf.setSize (2, n, false, false, true);

    // Parametros por voz que cambian por bloque (pan y tono)
    for (int i = 0; i < nv; ++i)
    {
        auto& v = voices[(size_t) i];
        const float ang = (juce::jlimit (-1.f, 1.f, v.pan * spread) + 1.f) * juce::MathConstants<float>::pi * 0.25f;
        v.gl = std::cos (ang);
        v.gr = std::sin (ang);
        const float fc = juce::jmin (toneHz * (0.6f + 0.4f * v.toneRand), (float) sr * 0.45f);
        v.lpCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * fc / (float) sr);
    }

    const float norm = 1.2f / std::sqrt ((float) nv);
    const float driftCoef = 1.0f / (float) (sr * 0.35);
    const float* inL = buffer.getReadPointer (0);
    const float* inR = buffer.getReadPointer (1);
    float* wl = wetBuf.getWritePointer (0);
    float* wr = wetBuf.getWritePointer (1);

    for (int s = 0; s < n; ++s)
    {
        buf[(size_t) writePos] = 0.5f * (inL[s] + inR[s]);

        float sumL = 0.f, sumR = 0.f;

        for (int i = 0; i < nv; ++i)
        {
            auto& v = voices[(size_t) i];

            if (--v.driftCount <= 0)
            {
                v.driftTarget = rnd.nextFloat() * 2.f - 1.f;
                v.driftCount = 1 + (int) ((0.25f + 0.9f * rnd.nextFloat()) * (float) sr);
            }
            v.drift += (v.driftTarget - v.drift) * driftCoef;

            const float cents = detune * v.detuneRand
                              + 100.f * (float) kInterval[harmony][i]
                              + human * 20.f * v.drift;
            const float ratio = std::exp2 (cents / 1200.f);
            const float step = 1.0f - ratio;

            const float Ttarget = timingMs * 0.001f * (float) sr * v.timingRand;
            v.T += (Ttarget - v.T) * 0.002f;

            float out = 0.f;
            for (int h = 0; h < 2; ++h)
            {
                v.d[h] += step;
                if (v.d[h] < 0.f)        v.d[h] += W;
                else if (v.d[h] >= W)    v.d[h] -= W;

                const float sn = std::sin (juce::MathConstants<float>::pi * v.d[h] / W);
                const float w = sn * sn;

                const float delay = v.T + v.d[h] + 2.0f;
                const float rpos = (float) writePos - delay;
                const int i0 = (int) std::floor (rpos);
                const float fr = rpos - (float) i0;
                const float a = buf[(size_t) (i0 & mask)];
                const float b = buf[(size_t) ((i0 + 1) & mask)];
                out += (a + fr * (b - a)) * w;
            }

            v.lp += v.lpCoef * (out - v.lp);
            out = v.lp * v.gain;

            sumL += out * v.gl;
            sumR += out * v.gr;
        }

        writePos = (writePos + 1) & mask;
        wl[s] = sumL * norm;
        wr[s] = sumR * norm;
    }

    reverb.processStereo (wl, wr, n);

    float* outL = buffer.getWritePointer (0);
    float* outR = buffer.getWritePointer (1);
    for (int s = 0; s < n; ++s)
    {
        const float mix = mixS.getNextValue();
        const float g = gainS.getNextValue();
        const float dryG = std::cos (mix * juce::MathConstants<float>::halfPi);
        const float wetG = std::sin (mix * juce::MathConstants<float>::halfPi);
        outL[s] = (outL[s] * dryG + wl[s] * wetG) * g;
        outR[s] = (outR[s] * dryG + wr[s] * wetG) * g;
    }
}

juce::AudioProcessorEditor* CelesteChoirProcessor::createEditor()
{
    return new CelesteChoirEditor (*this);
}

void CelesteChoirProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void CelesteChoirProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CelesteChoirProcessor();
}
