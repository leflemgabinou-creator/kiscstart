#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Shapes.h"

using APVTS = juce::AudioProcessorValueTreeState;

APVTS::ParameterLayout KickProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix",
        NormalisableRange<float> (0.f, 1.f, 0.001f), 0.8f));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "div", 1 }, "Division",
        StringArray { "1/8", "1/4", "1/2", "1/1" }, 1));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Trigger",
        StringArray { "Sync", "Midi", "Audio" }, 0));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "loop", 1 }, "Loop", true));

    StringArray shapes;
    for (int i = 1; i <= KS::numShapes; ++i) shapes.add (String (i));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "shape", 1 }, "Shape", shapes, 2));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "len", 1 }, "Length",
        NormalisableRange<float> (0.05f, 1.f, 0.001f), 0.83f));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "band", 1 }, "Band", false));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "listen", 1 }, "Listen", false));

    NormalisableRange<float> fr (30.f, 2000.f, 0.1f);
    fr.setSkewForCentre (200.f);
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "freq", 1 }, "Band Freq", fr, 150.f,
        AudioParameterFloatAttributes().withLabel ("Hz")));

    return { p.begin(), p.end() };
}

KickProcessor::KickProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "KICKSTART", createLayout())
{
    for (int i = 0; i < kCols; ++i)
    {
        inMin[i] = 0.f; inMax[i] = 0.f; outMin[i] = 0.f; outMax[i] = 0.f;
    }
}

bool KickProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    if (l.getMainInputChannelSet() != out)
        return false;
    const auto sc = l.getChannelSet (true, 1);
    return sc.isDisabled() || sc == juce::AudioChannelSet::stereo() || sc == juce::AudioChannelSet::mono();
}

void KickProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    xover.prepare ({ sampleRate, (juce::uint32) samplesPerBlock, 2 });
    xover.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    xover.reset();
    phase = 1.f; running = false; gainSmooth = 1.f; detLevel = 0.f; armed = true; refractory = 0;
    tableShape = -1; lastMode = -1;
}

void KickProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    auto mainBuf = getBusBuffer (buffer, true, 0);
    auto scBuf   = getBusBuffer (buffer, true, getBusCount (true) > 1 ? 1 : 0);
    const int nch = juce::jmin (mainBuf.getNumChannels(), 2);
    const bool useSC = getBusCount (true) > 1 && getBus (true, 1) != nullptr
                       && getBus (true, 1)->isEnabled() && scBuf.getNumChannels() > 0;

    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    const float mix    = get ("mix");
    const int   div    = juce::jlimit (0, 3, (int) std::round (get ("div")));
    const int   mode   = juce::jlimit (0, 2, (int) std::round (get ("mode")));   // 0 sync, 1 midi, 2 audio
    const bool  loop   = get ("loop") > 0.5f;
    const int   shape  = juce::jlimit (0, KS::numShapes - 1, (int) std::round (get ("shape")));
    const float len    = juce::jmax (0.05f, get ("len"));
    const bool  band   = get ("band") > 0.5f;
    const bool  listen = band && get ("listen") > 0.5f;
    const float freq   = get ("freq");

    if (shape != tableShape)
    {
        for (int i = 0; i <= 1024; ++i)
            table[(size_t) i] = KS::shapeValue (shape, (float) i / 1024.f);
        tableShape = shape;
    }
    if (mode != lastMode)
    {
        lastMode = mode;
        if (mode != 0) { phase = 1.f; running = false; }
    }
    xover.setCutoffFrequency (juce::jmin (freq, 0.45f * (float) sr));

    // tempo et position
    double bpm = 120.0, ppq = 0.0;
    bool playing = false, havePpq = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            playing = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition()) { ppq = *q; havePpq = true; }
        }
    static constexpr double divBeats[4] = { 0.5, 1.0, 2.0, 4.0 };
    const double beats = divBeats[div];
    const float cyclesPerSample = (float) ((bpm / 60.0) / (sr * beats));

    if (mode == 0 && playing && havePpq)
    {
        double ph = std::fmod (ppq / beats, 1.0);
        if (ph < 0.0) ph += 1.0;
        phase = (float) ph;
    }

    // notes MIDI (positions en échantillons)
    int notePos[64]; int nNotes = 0;
    if (mode == 1)
        for (const auto meta : midi)
            if (meta.getMessage().isNoteOn() && nNotes < 64)
                notePos[nNotes++] = meta.samplePosition;
    int mi = 0;

    float* io[2] = { mainBuf.getWritePointer (0), nch > 1 ? mainBuf.getWritePointer (1) : nullptr };
    const float* det[2] = { nullptr, nullptr };
    for (int c = 0; c < nch; ++c)
        det[c] = useSC ? scBuf.getReadPointer (juce::jmin (c, scBuf.getNumChannels() - 1)) : mainBuf.getReadPointer (c);

    const float smoothCoef = 1.f - std::exp (-1.f / ((float) sr * 0.0006f));
    const float relCoef = std::exp (-1.f / ((float) sr * 0.03f));
    const float thr = 0.03f;   // seuil de déclenchement audio (~ -30 dBFS)

    for (int i = 0; i < n; ++i)
    {
        bool trig = false;
        if (mode == 1)
            while (mi < nNotes && notePos[mi] <= i) { trig = true; ++mi; }

        if (mode == 2)
        {
            float d = 0.f;
            for (int c = 0; c < nch; ++c) d = juce::jmax (d, std::abs (det[c][i]));
            detLevel = juce::jmax (d, detLevel * relCoef);
            if (refractory > 0) --refractory;
            if (armed && d > thr && refractory == 0) { trig = true; armed = false; refractory = (int) (0.05 * sr); }
            if (! armed && detLevel < thr * 0.5f) armed = true;
        }

        if (mode == 0)
        {
            phase += cyclesPerSample;
            if (phase >= 1.f) phase -= 1.f;
        }
        else if (trig)
        {
            phase = 0.f; running = true;
        }
        else if (running)
        {
            phase += cyclesPerSample;
            if (phase >= 1.f)
            {
                if (loop) phase -= 1.f; else { phase = 1.f; running = false; }
            }
        }

        const float tt = juce::jmin (1.f, phase / len);
        const float fi = tt * 1024.f;
        const int i0 = juce::jmin (1023, (int) fi);
        const float g = table[(size_t) i0] + (table[(size_t) i0 + 1] - table[(size_t) i0]) * (fi - (float) i0);
        const float target = 1.f - mix * (1.f - g);
        gainSmooth += (target - gainSmooth) * smoothCoef;

        float inSample = 0.f, outSample = 0.f;
        for (int c = 0; c < nch; ++c)
        {
            const float x = io[c][i];
            float y;
            if (band)
            {
                float lo, hi;
                xover.processSample (c, x, lo, hi);
                y = listen ? lo * gainSmooth : hi + lo * gainSmooth;
            }
            else
                y = x * gainSmooth;

            io[c][i] = y;
            if (c == 0) { inSample = x; outSample = y; }
        }

        // courbes de l'affichage (par colonne de phase)
        const int col = juce::jlimit (0, kCols - 1, (int) (phase * (float) kCols));
        if (col != lastCol)
        {
            inMin[lastCol].store (curInMin);  inMax[lastCol].store (curInMax);
            outMin[lastCol].store (curOutMin); outMax[lastCol].store (curOutMax);
            lastCol = col;
            curInMin = curInMax = inSample;
            curOutMin = curOutMax = outSample;
        }
        else
        {
            curInMin = juce::jmin (curInMin, inSample);   curInMax = juce::jmax (curInMax, inSample);
            curOutMin = juce::jmin (curOutMin, outSample); curOutMax = juce::jmax (curOutMax, outSample);
        }
    }

    uiPhase.store (phase);
}

juce::AudioProcessorEditor* KickProcessor::createEditor() { return new KickEditor (*this); }

void KickProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void KickProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new KickProcessor(); }
