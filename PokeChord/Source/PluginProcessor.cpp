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

    const char* const kRegenIds[] = { "style", "mood", "key", "mode", "numChords", "duration", "rhythm", "voicing",
                                      "octave", "bass", "complexity", "boldness", "humanize", "velRandom", "seed" };
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout PokeChordAudioProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "style", 1 }, "Style",
                                                         toStringArray (chords::styleNames(), "Random"), 1));

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mood", 1 }, "Ambiance",
                                                         toStringArray (chords::moodNames(), "Aléatoire"), 0));

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "key", 1 }, String::fromUTF8 ("Tonalité"),
                                                         toStringArray (chords::keyNames(), "Aléatoire"), 0));

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode",
                                                         StringArray { "Auto", "Major", "Minor" }, 0));

    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "numChords", 1 }, "Nombre d'accords", 2, 8, 4));

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "duration", 1 }, String::fromUTF8 ("Durée d'un accord"),
                                                         StringArray { "Random", "Full random", "1/2 mesure", "1 mesure", "2 mesures" }, 3));

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "rhythm", 1 }, "Rythme",
                                                         toStringArray (chords::rhythmNames(), "Random"), 1));

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "voicing", 1 }, "Disposition",
                                                         toStringArray (chords::voicingNames(), "Random"), 1));

    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "octave", 1 }, "Octave", -1, 1, 0));

    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "bass", 1 }, "Basse", true));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "complexity", 1 }, String::fromUTF8 ("Complexité"),
                                                        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "boldness", 1 }, "Audace",
                                                        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.3f));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "humanize", 1 }, "Humanisation",
                                                        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.25f));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "velRandom", 1 }, String::fromUTF8 ("Vélocité aléatoire"),
                                                        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.2f));

    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "seed", 1 }, "Graine", 0, 99999, 12345));

    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "sound", 1 }, String::fromUTF8 ("Son d'écoute"), true));

    {
        StringArray names;
        for (const auto& pr : sounds::presets())
            names.add (pr.name);
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "preset", 1 }, String::fromUTF8 ("Son d'écoute (preset)"), names, 0));
    }

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "volume", 1 }, "Volume",
                                                        NormalisableRange<float> (-30.0f, 6.0f, 0.1f), -8.0f,
                                                        AudioParameterFloatAttributes().withLabel ("dB")));

    return { p.begin(), p.end() };
}

//==============================================================================
PokeChordAudioProcessor::PokeChordAudioProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    frame.assign ((size_t) analyse::kFFTSize, 0.0f);
    startTimerHz (10);
    for (auto* id : kRegenIds)
        apvts.addParameterListener (id, this);

    regenerate();
}

PokeChordAudioProcessor::~PokeChordAudioProcessor()
{
    stopTimer();
    cancelPendingUpdate();
    for (auto* id : kRegenIds)
        apvts.removeParameterListener (id, this);
}

bool PokeChordAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void PokeChordAudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    hostSr = sampleRate;
    for (auto& v : voices)
        v = Voice();
    internalBeat = 0.0;
}

//==============================================================================
chords::Settings PokeChordAudioProcessor::readSettings() const
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    chords::Settings s;
    s.style = (int) std::lround (get ("style")) - 1;   // -1 = Random
    s.mood = (int) std::lround (get ("mood")) - 1;
    s.key = (int) std::lround (get ("key")) - 1;
    s.mode = (int) std::lround (get ("mode")) - 1;
    s.numChords = (int) std::lround (get ("numChords"));

    static const double beats[] = { 0.0, -1.0, 2.0, 4.0, 8.0 };   // 0 = au hasard
    s.beatsPerChord = beats[juce::jlimit (0, 4, (int) std::lround (get ("duration")))];

    s.rhythm = (int) std::lround (get ("rhythm")) - 1;
    s.voicing = (int) std::lround (get ("voicing")) - 1;
    s.octave = (int) std::lround (get ("octave"));
    s.bass = get ("bass") > 0.5f;
    s.complexity = get ("complexity");
    s.boldness = get ("boldness");
    s.humanize = get ("humanize");
    s.velRandom = get ("velRandom");
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
        pod.numChords = juce::jmin (16, (int) progression.lens.size());
        for (int i = 0; i < pod.numChords; ++i)
            pod.chordEnd[i] = (float) (progression.starts[(size_t) i] + progression.lens[(size_t) i]);
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
    target->dec = 1.0f;
    target->preset = blockPreset;
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

    const int numSamples = buffer.getNumSamples();
    blockPreset = juce::jlimit (0, sounds::kCount - 1, (int) std::lround (apvts.getRawParameterValue ("preset")->load()));

    // l'audio entrant passe tel quel ; on en garde une copie mono pour l'analyse
    {
        const int numCh = juce::jmax (1, buffer.getNumChannels());
        const int pos = ringPos.load (std::memory_order_relaxed);
        double sumSq = 0.0;
        for (int i = 0; i < numSamples; ++i)
        {
            float s = 0.0f;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                s += buffer.getReadPointer (ch)[i];
            const float m = s / (float) numCh;
            ring[(size_t) ((pos + i) & (kRing - 1))] = m;
            sumSq += (double) m * (double) m;
        }
        ringPos.store ((pos + numSamples) & (kRing - 1), std::memory_order_release);
        inLevel.store (juce::jlimit (0.0f, 1.0f, (float) std::sqrt (sumSq / (double) juce::jmax (1, numSamples)) * 7.0f));
    }

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

        int pc = audioPod.numChords > 0 ? audioPod.numChords - 1 : -1;
        for (int i = 0; i < audioPod.numChords; ++i)
            if (loopBeat < (double) audioPod.chordEnd[i] - 1.0e-6) { pc = i; break; }
        playingChord = pc;
        playBeat = (float) loopBeat;
    }
    else
    {
        for (auto& v : voices)
            v.gate = false;
        playingChord = -1;
        playBeat = -1.0f;
    }

    // rendu du synthé d'écoute
    const bool soundOn = apvts.getRawParameterValue ("sound")->load() > 0.5f;
    const float gain = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("volume")->load()) * 0.16f;

    std::array<sounds::Runtime, sounds::kCount> rt;
    for (int i = 0; i < sounds::kCount; ++i)
        rt[(size_t) i] = sounds::makeRuntime (sounds::presets()[(size_t) i], sr);

    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        float mix = 0.0f;

        for (auto& v : voices)
        {
            if (! v.active)
                continue;

            const int pi = juce::jlimit (0, sounds::kCount - 1, v.preset);
            mix += sounds::tick (sounds::presets()[(size_t) pi], rt[(size_t) pi], v.gate, v.inc, v.ph,
                                 v.env, v.dec, v.lp, sr) * v.vel;

            if (! v.gate && v.env < 0.0005f)
                v.active = false;
        }

        const float out = soundOn ? std::tanh (mix * gain * 2.0f) : 0.0f;
        left[i] += out;
        if (right != nullptr)
            right[i] += out;
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

//==============================================================================
// Analyse de l'audio entrant
//==============================================================================
void PokeChordAudioProcessor::clearHistory()
{
    history.clear();
    live = analyse::Result();
    recent.clear();
    detector.reset();
    ++analysisVersion;
}

void PokeChordAudioProcessor::timerCallback()
{
    if (! analysing.load())
        return;

    // les derniers échantillons, dans l'ordre
    const int p = ringPos.load (std::memory_order_acquire);
    for (int i = 0; i < analyse::kFFTSize; ++i)
        frame[(size_t) i] = ring[(size_t) ((p - analyse::kFFTSize + i) & (kRing - 1))];

    detector.setSampleRate (hostSr.load());
    const auto r = detector.analyse (frame.data());
    liveChroma = r.chroma;

    if (! r.valid)
    {
        recent.clear();
        if (++silentTicks >= 6)
            live = analyse::Result();   // plus rien d'audible depuis ~0,6 s
    }
    else
    {
        silentTicks = 0;

        recent.push_back (r);
        if (recent.size() > 5)
            recent.erase (recent.begin());

        // vote : l'accord retenu est celui vu au moins 3 fois dans les 5 dernières mesures,
        // ce qui résiste à une mesure isolée faussée par la mélodie ou une transition
        int votes = 0;
        const analyse::Result* win = nullptr;
        for (const auto& a : recent)
        {
            int c = 0;
            for (const auto& b : recent)
                if (b.name == a.name)
                    ++c;
            if (c > votes || (c == votes && win != nullptr && a.name == r.name))
            {
                votes = c;
                win = &a;
            }
        }

        // même fondamentale et notes en plus (C puis Cmaj7) : on passe à l'accord le plus riche sans attendre
        bool upgrade = false;
        if (live.valid && r.root == live.root && r.pcs.size() > live.pcs.size() && votes >= 2 && win != nullptr && win->name == r.name)
        {
            upgrade = true;
            for (int pc : live.pcs)
            {
                bool found = false;
                for (int q : r.pcs)
                    found |= (q == pc);
                upgrade &= found;
            }
        }

        if (win != nullptr && (votes >= 3 || upgrade) && (! live.valid || live.name != win->name))
        {
            const analyse::Result& w = upgrade ? r : *win;
            live = w;

            Detected d;
            d.name = w.name;
            d.root = w.root;
            d.bass = w.bass;
            d.pcs = w.pcs;
            d.timeSec = juce::Time::getMillisecondCounterHiRes() / 1000.0 - 0.3;

            // l'accord précédent était le même en plus simple : on le remplace au lieu d'empiler
            if (upgrade && ! history.empty() && history.back().root == w.root)
                history.back() = d;
            else
            {
                history.push_back (d);
                if (history.size() > 64)
                    history.erase (history.begin());
            }
        }
        else if (live.valid && live.name == r.name)
        {
            live.confidence = r.confidence;
        }
    }

    ++analysisVersion;
}

juce::File PokeChordAudioProcessor::writeDetectedMidiFile()
{
    if (history.empty())
        return {};

    const double bpm = juce::jlimit (20.0, 300.0, hostBpm.load());
    const double t0 = history.front().timeSec;
    const double nowSec = juce::Time::getMillisecondCounterHiRes() / 1000.0;

    struct Seg { const Detected* d; double start; };
    std::vector<Seg> segs;
    for (const auto& d : history)
    {
        const double beats = std::round ((d.timeSec - t0) * bpm / 60.0 * 2.0) / 2.0;   // grille d'une croche
        if (! segs.empty() && beats <= segs.back().start)
            segs.back().d = &d;
        else
            segs.push_back ({ &d, beats });
    }

    chords::Progression prog;
    prog.keyName = "Analyse";
    prog.styleName = "Analyse";
    prog.moodName = "Analyse";

    double endAll = 0.0;
    for (size_t i = 0; i < segs.size(); ++i)
    {
        double end = (i + 1 < segs.size())
                         ? segs[i + 1].start
                         : std::max (segs[i].start + 2.0, std::round ((nowSec - t0) * bpm / 60.0 * 2.0) / 2.0);
        end = std::max (end, segs[i].start + 0.5);
        endAll = std::max (endAll, end);

        const auto& d = *segs[i].d;
        chords::Chord ch;
        ch.name = d.name;
        ch.root = d.root;
        prog.chords.push_back (ch);

        prog.events.push_back ({ 36 + (d.bass >= 0 && d.name.find ('/') != std::string::npos ? d.bass : d.root), segs[i].start, end - segs[i].start, 80 });
        for (int pc : d.pcs)
            prog.events.push_back ({ 60 + pc, segs[i].start, end - segs[i].start, 90 });
    }
    prog.lengthBeats = std::ceil (endAll / 4.0) * 4.0;
    prog.beatsPerChord = 4.0;

    const auto bytes = chords::toMidiFile (prog, (int) std::lround (bpm));

    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("PokeChord");
    dir.createDirectory();

    auto f = dir.getChildFile (juce::File::createLegalFileName (
        "Accords detectes " + juce::Time::getCurrentTime().formatted ("%H-%M-%S")) + ".mid");
    f.replaceWithData (bytes.data(), bytes.size());
    return f;
}
