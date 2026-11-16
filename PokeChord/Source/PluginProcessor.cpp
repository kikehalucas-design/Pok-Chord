#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace
{
    juce::StringArray toStringArray (const std::vector<std::string>& v, const char* first = nullptr)
    {
        juce::StringArray out;
        if (first != nullptr)
            out.add (juce::String::fromUTF8 (first));
        for (const auto& s : v)
            out.add (juce::String::fromUTF8 (s.c_str()));
        return out;
    }

    const char* const kRegenIds[] = { "style", "mood", "key", "numChords", "duration", "rhythm", "voicing",
                                      "octave", "bass", "complexity", "boldness", "humanize", "seed" };
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout PokeChordAudioProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "style", 1 }, "Style",
                                                         toStringArray (chords::styleNames()), 0));

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mood", 1 }, "Ambiance",
                                                         toStringArray (chords::moodNames(), "Aléatoire"), 0));

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "key", 1 }, String::fromUTF8 ("Tonalité"),
                                                         toStringArray (chords::keyNames(), "Aléatoire"), 0));

    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "numChords", 1 }, "Nombre d'accords", 2, 8, 4));

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "duration", 1 }, String::fromUTF8 ("Durée d'un accord"),
                                                         StringArray { "1/2 mesure", "1 mesure", "2 mesures" }, 1));

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "rhythm", 1 }, "Rythme",
                                                         toStringArray (chords::rhythmNames()), 0));

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "voicing", 1 }, "Disposition",
                                                         toStringArray (chords::voicingNames()), 0));

    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "octave", 1 }, "Octave", -1, 1, 0));

    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "bass", 1 }, "Basse", true));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "complexity", 1 }, String::fromUTF8 ("Complexité"),
                                                        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "boldness", 1 }, "Audace",
                                                        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.3f));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "humanize", 1 }, "Humanisation",
                                                        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.25f));

    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "seed", 1 }, "Graine", 0, 99999, 12345));

    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "sound", 1 }, String::fromUTF8 ("Son d'écoute"), true));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "volume", 1 }, "Volume",
                                                        NormalisableRange<float> (-30.0f, 6.0f, 0.1f), -8.0f,
                                                        AudioParameterFloatAttributes().withLabel ("dB")));

    return { p.begin(), p.end() };
}

//==============================================================================
PokeChordAudioProcessor::PokeChordAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (auto* id : kRegenIds)
        apvts.addParameterListener (id, this);

    regenerate();
}

PokeChordAudioProcessor::~PokeChordAudioProcessor()
{
    cancelPendingUpdate();
    for (auto* id : kRegenIds)
        apvts.removeParameterListener (id, this);
}

bool PokeChordAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void PokeChordAudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    for (auto& v : voices)
        v = Voice();
    internalBeat = 0.0;
}

//==============================================================================
chords::Settings PokeChordAudioProcessor::readSettings() const
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    chords::Settings s;
    s.style = (int) std::lround (get ("style"));
    s.mood = (int) std::lround (get ("mood")) - 1;
    s.key = (int) std::lround (get ("key")) - 1;
    s.numChords = (int) std::lround (get ("numChords"));

    static const double beats[] = { 2.0, 4.0, 8.0 };
    s.beatsPerChord = beats[juce::jlimit (0, 2, (int) std::lround (get ("duration")))];

    s.rhythm = (int) std::lround (get ("rhythm"));
    s.voicing = (int) std::lround (get ("voicing"));
    s.octave = (int) std::lround (get ("octave"));
    s.bass = get ("bass") > 0.5f;
    s.complexity = get ("complexity");
    s.boldness = get ("boldness");
    s.humanize = get ("humanize");
    s.seed = (uint32_t) std::lround (get ("seed"));
    return s;
}

void PokeChordAudioProcessor::regenerate()
{
    progression = chords::generate (readSettings());

    {
        const juce::SpinLock::ScopedLockType lock (podLock);

        pod.count = juce::jmin (kMaxEvents, (int) progression.events.size());
        for (int i = 0; i < pod.count; ++i)
        {
            const auto& e = progression.events[(size_t) i];
            pod.ev[i] = { (float) e.start, (float) (e.start + e.length), e.note, e.velocity };
        }
        pod.lengthBeats = (float) progression.lengthBeats;
        pod.beatsPerChord = (float) progression.beatsPerChord;
    }

    ++version;
}

juce::String PokeChordAudioProcessor::describeProgression() const
{
    juce::String s = fromUtf8 (progression.keyName) + "  |  " + fromUtf8 (progression.styleName)
                     + "  |  " + fromUtf8 (progression.moodName);
    return s;
}

juce::File PokeChordAudioProcessor::writeMidiFile()
{
    const auto bytes = chords::toMidiFile (progression, (int) std::lround (hostBpm.load()));

    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("PokeChord");
    dir.createDirectory();

    const int seed = (int) std::lround (apvts.getRawParameterValue ("seed")->load());
    const auto name = juce::File::createLegalFileName (
        fromUtf8 (progression.keyName) + " " + fromUtf8 (progression.styleName) + " "
        + fromUtf8 (progression.moodName) + " " + juce::String (seed)) + ".mid";

    auto f = dir.getChildFile (name);
    f.replaceWithData (bytes.data(), bytes.size());
    return f;
}

void PokeChordAudioProcessor::startAudition()
{
    resetAudition = true;
    auditioning = true;
}

//==============================================================================
void PokeChordAudioProcessor::noteOn (int note, int vel, double startBeat, double endBeat)
{
    Voice* target = nullptr;
    for (auto& v : voices)
        if (! v.active) { target = &v; break; }

    if (target == nullptr)  // aucune voix libre : on vole la plus faible
    {
        target = &voices[0];
        for (auto& v : voices)
            if (v.env < target->env)
                target = &v;
    }

    target->active = true;
    target->gate = true;
    target->inc = (float) (440.0 * std::pow (2.0, (note - 69) / 12.0) / sr);
    target->ph[0] = 0.0f;
    target->ph[1] = 0.33f;
    target->ph[2] = 0.66f;
    target->env = 0.0f;
    target->lp = 0.0f;
    target->vel = (float) vel / 127.0f;
    target->startBeat = startBeat;
    target->endBeat = endBeat;
}

void PokeChordAudioProcessor::triggerRange (double from, double to)
{
    for (int i = 0; i < audioPod.count; ++i)
    {
        const auto& e = audioPod.ev[i];
        if ((double) e.start >= from && (double) e.start < to)
            noteOn (e.note, e.vel, (double) e.start, (double) e.end);
    }
}

void PokeChordAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();
    buffer.clear();

    const int numSamples = buffer.getNumSamples();

    // dernière progression disponible
    const int ver = version.load();
    if (ver != audioVersion)
    {
        const juce::SpinLock::ScopedTryLockType lock (podLock);
        if (lock.isLocked())
        {
            audioPod = pod;
            audioVersion = ver;
        }
    }

    // transport de l'hôte
    double bpm = 120.0, ppq = 0.0;
    bool hostPlaying = false, havePpq = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm())
                bpm = *b;
            hostPlaying = pos->getIsPlaying();
            if (auto pp = pos->getPpqPosition())
            {
                ppq = *pp;
                havePpq = true;
            }
        }
    bpm = juce::jlimit (20.0, 300.0, bpm);
    hostBpm = bpm;

    const double len = (double) audioPod.lengthBeats;
    const double bpc = (double) audioPod.beatsPerChord;
    const double blockBeats = (double) numSamples * bpm / 60.0 / sr;

    bool run = false;
    double loopBeat = 0.0;

    if (len > 0.0)
    {
        if (hostPlaying && havePpq)
        {
            run = true;
            loopBeat = std::fmod (ppq, len);
            if (loopBeat < 0.0)
                loopBeat += len;
        }
        else if (auditioning.load())
        {
            run = true;
            if (resetAudition.exchange (false))
                internalBeat = 0.0;
            loopBeat = std::fmod (internalBeat, len);
            internalBeat = loopBeat + blockBeats;
        }
    }

    if (run)
    {
        for (auto& v : voices)
            if (v.active && v.gate && (loopBeat >= v.endBeat || loopBeat < v.startBeat))
                v.gate = false;

        const double b1 = loopBeat + blockBeats;
        triggerRange (loopBeat, juce::jmin (b1, len));
        if (b1 > len)
            triggerRange (0.0, b1 - len);

        playingChord = bpc > 0.0 ? (int) (loopBeat / bpc) : -1;
    }
    else
    {
        for (auto& v : voices)
            v.gate = false;
        playingChord = -1;
    }

    // rendu du synthé d'écoute
    const bool soundOn = apvts.getRawParameterValue ("sound")->load() > 0.5f;
    const float gain = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("volume")->load()) * 0.16f;

    const float atk = 1.0f - std::exp (-1.0f / (0.008f * (float) sr));
    const float rel = 1.0f - std::exp (-1.0f / (0.28f * (float) sr));
    const float lpc = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 2800.0f / (float) sr);
    static const float det[3] = { 0.997f, 1.0f, 1.003f };

    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        float mix = 0.0f;

        for (auto& v : voices)
        {
            if (! v.active)
                continue;

            v.env += ((v.gate ? 1.0f : 0.0f) - v.env) * (v.gate ? atk : rel);

            float s = 0.0f;
            for (int k = 0; k < 3; ++k)
            {
                v.ph[k] += v.inc * det[k];
                if (v.ph[k] >= 1.0f)
                    v.ph[k] -= 1.0f;
                s += 2.0f * v.ph[k] - 1.0f;
            }

            v.lp += lpc * (s * (1.0f / 3.0f) - v.lp);
            mix += v.lp * v.env * v.vel;

            if (! v.gate && v.env < 0.0005f)
                v.active = false;
        }

        const float out = soundOn ? std::tanh (mix * gain * 2.0f) : 0.0f;
        left[i] = out;
        if (right != nullptr)
            right[i] = out;
    }
}

//==============================================================================
juce::AudioProcessorEditor* PokeChordAudioProcessor::createEditor()
{
    return new PokeChordAudioProcessorEditor (*this);
}

void PokeChordAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void PokeChordAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            regenerate();
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PokeChordAudioProcessor();
}
