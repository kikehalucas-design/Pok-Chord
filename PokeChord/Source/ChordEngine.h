#pragma once

// Moteur de génération d'accords : C++ pur, sans dépendance à JUCE.

#include <cstdint>
#include <string>
#include <vector>

namespace chords
{
    struct Settings
    {
        int style = 0;               // index dans styleNames()
        int mood = -1;               // index dans moodNames(), -1 = aléatoire
        int key = -1;                // 0..11 (C..B), -1 = aléatoire
        int numChords = 4;           // 1..8
        double beatsPerChord = 4.0;  // 2, 4 ou 8
        int rhythm = 0;              // index dans rhythmNames()
        int voicing = 0;             // 0 serré, 1 ouvert (drop 2), 2 piano
        int octave = 0;              // -1, 0, +1
        bool bass = true;            // ajoute la fondamentale dans le grave
        float complexity = 0.5f;     // 0 = accords simples, 1 = accords riches
        float boldness = 0.3f;       // 0 = sage, 1 = audacieux (dominantes secondaires, emprunts…)
        float humanize = 0.2f;       // 0..1
        uint32_t seed = 1;
    };

    struct Chord
    {
        std::string name;            // ex. "Fm9"
        std::string roman;           // ex. "i", "bVI", "V7/bIII"
        std::vector<int> notes;      // notes MIDI jouées (sans la basse)
        int bassNote = -1;           // note MIDI de la basse, ou -1
        int root = 0;                // 0..11
    };

    struct Event
    {
        int note;
        double start;                // en noires
        double length;               // en noires
        int velocity;
    };

    struct Progression
    {
        std::vector<Chord> chords;
        std::vector<Event> events;
        double lengthBeats = 16.0;
        double beatsPerChord = 4.0;
        int key = 0;
        bool minor = false;
        std::string keyName;         // ex. "Fm"
        std::string styleName;
        std::string moodName;
    };

    const std::vector<std::string>& styleNames();
    const std::vector<std::string>& moodNames();
    const std::vector<std::string>& rhythmNames();
    const std::vector<std::string>& voicingNames();
    const std::vector<std::string>& keyNames();

    Progression generate (const Settings&);

    // Fichier MIDI standard (format 0, 480 ppq) prêt à être glissé dans Ableton
    std::vector<unsigned char> toMidiFile (const Progression&, int bpm = 120);

    std::string noteName (int midiNote);
}
