#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

// Formantes (Hz) por tipo de voz [soprano, alto, tenor, bajo] x vocal [a e i o u] x 4 formantes
static const float kFormantTable[4][5][4] =
{
    { {800,1150,2900,3900}, {350,2000,2800,3600}, {270,2140,2950,3900}, {450,800,2830,3800}, {325,700,2700,3800} },
    { {800,1150,2800,3500}, {400,1600,2700,3300}, {350,1700,2700,3700}, {450,800,2830,3500}, {325,700,2530,3500} },
    { {650,1080,2650,2900}, {400,1700,2600,3200}, {290,1870,2800,3250}, {400,800,2600,2800}, {350,600,2700,2900} },
    { {600,1040,2250,2450}, {400,1620,2400,2800}, {250,1750,2600,3050}, {400,750,2400,2600}, {350,600,2400,2675} }
};
static const float kBandwidth[4] = { 70.f, 90.f, 120.f, 130.f };
static const float kFormantGain[4] = { 1.0f, 0.56f, 0.25f, 0.15f };

// Detune relativo de cada oscilador del "ensemble" (canal L / canal R)
static const float kDetune[2][3] = { { -1.0f, 0.35f, 0.9f }, { -0.8f, -0.3f, 1.0f } };

static inline float polyBlep (float t, float dt)
{
    if (t < dt)         { t /= dt;           return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt)  { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

static inline float bandpass (const float g, const float k, const float a1, const float a2, const float a3,
                              float& s1, float& s2, float v0)
{
    const float v3 = v0 - s2;
    const float v1 = a1 * s1 + a2 * v3;
    const float v2 = s2 + a2 * s1 + a3 * v3;
    s1 = 2.0f * v1 - s1;
    s2 = 2.0f * v2 - s2;
    (void) g;
    return v1 * k;
}

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

    f ("vowel",    "Vocal",         NR (0.f, 4.f, 0.001f), 0.f);
    f ("formant",  "Formante",      NR (0.7f, 1.4f, 0.001f), 1.f);
    f ("ensemble", "Ensemble",      NR (0.f, 50.f, 0.1f), 15.f);
    f ("breath",   "Aire",          NR (0.f, 1.f, 0.001f), 0.15f);
    f ("vibDepth", "Vibrato",       NR (0.f, 100.f, 0.1f), 20.f);
    f ("vibRate",  "Vel. Vibrato",  NR (0.5f, 9.f, 0.01f), 5.2f);
    f ("gain",     "Volumen",       NR (-24.f, 6.f, 0.1f), -6.f);
    f ("attack",   "Ataque",        NR (0.01f, 4.f, 0.001f, 0.4f), 0.6f);
    f ("decay",    "Caida",         NR (0.01f, 2.f, 0.001f, 0.5f), 0.3f);
    f ("sustain",  "Sostenido",     NR (0.f, 1.f, 0.001f), 0.8f);
    f ("release",  "Liberacion",    NR (0.05f, 6.f, 0.001f, 0.4f), 1.2f);
    f ("rvSize",   "Reverb Tam.",   NR (0.f, 1.f, 0.001f), 0.6f);
    f ("rvMix",    "Reverb Mezcla", NR (0.f, 1.f, 0.001f), 0.3f);

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "voice", 1 }, "Voz",
                 StringArray { "Soprano", "Alto", "Tenor", "Bajo" }, 1));

    return { p.begin(), p.end() };
}

//==============================================================================
CelesteChoirProcessor::CelesteChoirProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

bool CelesteChoirProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void CelesteChoirProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    for (auto& v : voices)
    {
        v = Voice();
        v.env.setSampleRate (sr);
    }
    vowelSmooth.reset (sr, 0.05);
    vowelSmooth.setCurrentAndTargetValue (apvts.getRawParameterValue ("vowel")->load());
    reverb.setSampleRate (sr);
    reverb.reset();
    pedalDown = false;
}

void CelesteChoirProcessor::readParams()
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    params.vowel        = get ("vowel");
    params.formantShift = get ("formant");
    params.ensemble     = get ("ensemble");
    params.breath       = get ("breath");
    params.vibDepth     = get ("vibDepth");
    params.vibRate      = get ("vibRate");
    params.gainLin      = juce::Decibels::decibelsToGain (get ("gain"));
    params.voiceType    = juce::jlimit (0, 3, (int) std::lround (get ("voice")));
    params.adsr.attack  = get ("attack");
    params.adsr.decay   = get ("decay");
    params.adsr.sustain = get ("sustain");
    params.adsr.release = get ("release");

    juce::Reverb::Parameters rp;
    const float mix = get ("rvMix");
    rp.roomSize   = get ("rvSize");
    rp.damping    = 0.45f;
    rp.wetLevel   = mix * 0.6f;
    rp.dryLevel   = 1.0f - mix * 0.4f;
    rp.width      = 1.0f;
    rp.freezeMode = 0.f;
    reverb.setParameters (rp);
}

CelesteChoirProcessor::Voice* CelesteChoirProcessor::allocateVoice()
{
    for (auto& v : voices)
        if (! v.env.isActive())
            return &v;

    Voice* oldest = &voices[0];
    for (auto& v : voices)
        if (v.age < oldest->age)
            oldest = &v;
    return oldest;
}

void CelesteChoirProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
    {
        auto* v = allocateVoice();
        v->note = m.getNoteNumber();
        v->baseFreq = (float) juce::MidiMessage::getMidiNoteInHertz (v->note);
        v->vel = 0.25f + 0.75f * m.getFloatVelocity();
        v->held = true;
        v->sustained = false;
        v->age = ++ageCounter;
        v->vibPhase = 0.f;
        v->vibAmt = 0.f;
        for (int ch = 0; ch < 2; ++ch)
            for (int k = 0; k < 3; ++k)
                v->phase[ch][k] = rnd.nextFloat();
        v->env.setSampleRate (sr);
        v->env.setParameters (params.adsr);
        v->env.noteOn();
    }
    else if (m.isNoteOff())
    {
        for (auto& v : voices)
            if (v.held && v.note == m.getNoteNumber())
            {
                v.held = false;
                if (pedalDown) v.sustained = true;
                else           v.env.noteOff();
            }
    }
    else if (m.isController() && m.getControllerNumber() == 64)
    {
        pedalDown = m.getControllerValue() >= 64;
        if (! pedalDown)
            for (auto& v : voices)
                if (v.sustained) { v.sustained = false; v.env.noteOff(); }
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        for (auto& v : voices) { v.held = false; v.sustained = false; v.env.noteOff(); }
    }
}

void CelesteChoirProcessor::computeFormants (float vowel, BPCoef* coefs, float* gains) const
{
    vowel = juce::jlimit (0.f, 4.f, vowel);
    int i0 = (int) std::floor (vowel);
    if (i0 > 3) i0 = 3;
    const float frac = vowel - (float) i0;
    const auto& tab = kFormantTable[params.voiceType];

    for (int k = 0; k < kFormants; ++k)
    {
        float f = tab[i0][k] + frac * (tab[i0 + 1][k] - tab[i0][k]);
        f *= params.formantShift;
        f = juce::jlimit (80.f, (float) sr * 0.45f, f);

        const float Q = f / kBandwidth[k];
        auto& c = coefs[k];
        c.g  = std::tan (juce::MathConstants<float>::pi * f / (float) sr);
        c.k  = 1.0f / Q;
        c.a1 = 1.0f / (1.0f + c.g * (c.g + c.k));
        c.a2 = c.g * c.a1;
        c.a3 = c.g * c.a2;
        gains[k] = kFormantGain[k];
    }
}

void CelesteChoirProcessor::render (juce::AudioBuffer<float>& buffer, int start, int end)
{
    auto* outL = buffer.getWritePointer (0);
    auto* outR = buffer.getWritePointer (1);
    const float makeup = 5.0f;

    while (start < end)
    {
        const int n = juce::jmin (kChunk, end - start);
        const float vowel = vowelSmooth.skip (n);

        BPCoef coefs[kFormants];
        float gains[kFormants];
        computeFormants (vowel, coefs, gains);

        const float vibInc = params.vibRate / (float) sr;
        const float vibOnset = 1.0f / ((float) sr * 0.8f);
        float ratio[2][3];
        for (int ch = 0; ch < 2; ++ch)
            for (int k = 0; k < 3; ++k)
                ratio[ch][k] = std::exp2 (params.ensemble * kDetune[ch][k] / 1200.0f);

        for (auto& v : voices)
        {
            if (! v.env.isActive())
                continue;

            v.env.setParameters (params.adsr);

            for (int i = 0; i < n; ++i)
            {
                v.vibPhase += vibInc;
                if (v.vibPhase >= 1.f) v.vibPhase -= 1.f;
                v.vibAmt = juce::jmin (1.0f, v.vibAmt + vibOnset);

                const float lfo = std::sin (juce::MathConstants<float>::twoPi * v.vibPhase);
                const float f0 = v.baseFreq * std::exp2 (params.vibDepth * v.vibAmt * lfo / 1200.0f);
                const float e = v.env.getNextSample() * v.vel;

                float sample[2];
                for (int ch = 0; ch < 2; ++ch)
                {
                    float s = 0.f;
                    for (int k = 0; k < 3; ++k)
                    {
                        const float dt = juce::jmin (0.45f, f0 * ratio[ch][k] / (float) sr);
                        float ph = v.phase[ch][k] + dt;
                        if (ph >= 1.f) ph -= 1.f;
                        v.phase[ch][k] = ph;
                        s += (2.f * ph - 1.f) - polyBlep (ph, dt);
                    }
                    s *= (1.0f / 3.0f);
                    const float noise = rnd.nextFloat() * 2.f - 1.f;
                    const float x = s + params.breath * 0.6f * noise;

                    float y = 0.f;
                    for (int k = 0; k < kFormants; ++k)
                    {
                        const auto& c = coefs[k];
                        auto& st = v.st[ch][k];
                        y += gains[k] * bandpass (c.g, c.k, c.a1, c.a2, c.a3, st.s1, st.s2, x);
                    }
                    sample[ch] = y * makeup;
                }

                outL[start + i] += sample[0] * e;
                outR[start + i] += sample[1] * e;
            }
        }
        start += n;
    }
}

void CelesteChoirProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    readParams();
    vowelSmooth.setTargetValue (params.vowel);

    int pos = 0;
    for (const auto meta : midi)
    {
        const int t = juce::jlimit (pos, numSamples, meta.samplePosition);
        render (buffer, pos, t);
        pos = t;
        handleMidi (meta.getMessage());
    }
    render (buffer, pos, numSamples);

    reverb.processStereo (buffer.getWritePointer (0), buffer.getWritePointer (1), numSamples);

    buffer.applyGain (params.gainLin);

    for (int ch = 0; ch < 2; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
            d[i] = std::tanh (d[i]);
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
