#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "ChordEngine.h"
#include "ChordAnalyser.h"
#include <array>
#include <atomic>
#include <string>
#include <vector>

class PokeChordAudioProcessor : public juce::AudioProcessor,
                                   private juce::AudioProcessorValueTreeState::Listener,
                                   private juce::AsyncUpdater,
                                   private juce::Timer
{
public:
    PokeChordAudioProcessor();
    ~PokeChordAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "PokeChord"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    //==========================================================================
    // Utilisé par l'interface (thread principal uniquement)
    const chords::Progression& getProgression() const { return progression; }
    int getProgressionVersion() const { return version.load(); }
    juce::File writeMidiFile();
    juce::String describeProgression() const;

    void startAudition();
    void stopAudition() { auditioning = false; }

    std::atomic<bool> auditioning { false };
    std::atomic<int> playingChord { -1 };

    //==========================================================================
    // Analyse de l'audio entrant (thread principal uniquement)
    struct Detected
    {
        std::string name;
        int root = 0;
        std::vector<int> pcs;
        double timeSec = 0.0;
    };

    const analyse::Result& getLiveChord() const { return live; }          // dernier accord stable (valid=false : rien entendu)
    const std::array<float, 12>& getLiveChroma() const { return liveChroma; }
    const std::vector<Detected>& getHistory() const { return history; }
    int getAnalysisVersion() const { return analysisVersion.load(); }
    void clearHistory();
    void setAnalysing (bool on) { analysing = on; }
    bool isAnalysing() const { return analysing.load(); }
    double getBpm() const { return hostBpm.load(); }
    juce::File writeDetectedMidiFile();                                   // File() si l'historique est vide

    static juce::String fromUtf8 (const std::string& s) { return juce::String::fromUTF8 (s.c_str()); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    void parameterChanged (const juce::String&, float) override { triggerAsyncUpdate(); }
    void timerCallback() override;
    void handleAsyncUpdate() override { regenerate(); }
    void regenerate();
    chords::Settings readSettings() const;

    //==========================================================================
    // Progression partagée avec le thread audio (copie simple, protégée par un verrou)
    static constexpr int kMaxEvents = 1024;

    struct AudioEvent { float start; float end; int note; int vel; };
    struct AudioPod
    {
        AudioEvent ev[kMaxEvents];
        int count = 0;
        float lengthBeats = 16.0f;
        float beatsPerChord = 4.0f;
    };

    chords::Progression progression;      // thread principal
    AudioPod pod;                          // écrit par le thread principal
    AudioPod audioPod;                     // copie lue par le thread audio
    juce::SpinLock podLock;
    std::atomic<int> version { 0 };
    int audioVersion = -1;

    //==========================================================================
    // Petit synthé d'écoute
    struct Voice
    {
        bool active = false;
        bool gate = false;
        float inc = 0.0f;
        float ph[3] = { 0.0f, 0.0f, 0.0f };
        float env = 0.0f;
        float lp = 0.0f;
        float vel = 1.0f;
        double startBeat = 0.0;
        double endBeat = 0.0;
    };

    void noteOn (int note, int vel, double startBeat, double endBeat);
    void triggerRange (double from, double to);

    static constexpr int kVoices = 40;
    Voice voices[kVoices];

    // Analyse : l'audio entrant est copié dans un tampon circulaire, lu par le timer (10 Hz)
    static constexpr int kRing = 32768;
    std::array<float, kRing> ring {};
    std::atomic<int> ringPos { 0 };
    std::atomic<double> hostSr { 44100.0 };
    std::atomic<bool> analysing { false };
    std::atomic<int> analysisVersion { 0 };

    analyse::Detector detector;
    std::vector<float> frame;
    analyse::Result live;
    std::array<float, 12> liveChroma {};
    std::vector<Detected> history;
    std::string candidateName;
    int candidateCount = 0;
    int silentTicks = 0;

    std::atomic<bool> resetAudition { false };
    std::atomic<double> hostBpm { 120.0 };
    double internalBeat = 0.0;
    double sr = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PokeChordAudioProcessor)
};
