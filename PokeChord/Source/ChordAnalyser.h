#pragma once

// Détection d'accords à partir d'audio : C++ pur, sans dépendance à JUCE.

#include <array>
#include <string>
#include <vector>

namespace analyse
{
    constexpr int kFFTSize = 16384;

    struct Result
    {
        bool valid = false;
        int root = 0;                       // 0..11
        int bass = -1;                      // note la plus grave entendue (0..11), -1 si inconnue
        std::string name;                   // ex. "Cmaj7"
        std::vector<int> pcs;               // classes de hauteur de l'accord (0..11)
        float confidence = 0.0f;            // 0..1
        std::array<float, 12> chroma {};    // énergie par note (0..1), pour l'affichage
    };

    class Detector
    {
    public:
        Detector();

        void setSampleRate (double sr);
        void reset();

        // samples : les kFFTSize derniers échantillons mono
        Result analyse (const float* samples);

    private:
        void fft (std::vector<double>& re, std::vector<double>& im) const;

        double sampleRate = 44100.0;
        std::vector<double> window, cosT, sinT;
        std::vector<int> bitrev;
        std::array<float, 12> smooth {};
        std::array<double, 12> acc {};      // chroma des notes, lissé pour la décision
        double tuneCents = 0.0;             // accordage estimé de la source
        bool tuneKnown = false;
        std::vector<double> re, im, mag, floorMag, csum;
    };

    std::string pitchClassName (int pc);    // "C", "Db", "D", …
}
