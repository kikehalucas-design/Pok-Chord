#include "ChordEngine.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <map>
#include <random>
#include <utility>

namespace chords
{
namespace
{
    //==========================================================================
    // Qualités d'accords
    enum Q { QMaj, QMin, QDom7, QMaj7, QMin7, QMin9, QMaj9, QDom9, QDim, QHalfDim,
             QSus2, QSus4, QAdd9, QMin11, QDom13, QMaj69, QMaj6, QDom7b9, QMadd9, QDom7sus, QCount };

    struct QualityDef { const char* suffix; std::vector<int> iv; };

    const std::vector<QualityDef>& qualities()
    {
        static const std::vector<QualityDef> q = {
            { "",      { 0, 4, 7 } },
            { "m",     { 0, 3, 7 } },
            { "7",     { 0, 4, 7, 10 } },
            { "maj7",  { 0, 4, 7, 11 } },
            { "m7",    { 0, 3, 7, 10 } },
            { "m9",    { 0, 3, 10, 14 } },
            { "maj9",  { 0, 4, 11, 14 } },
            { "9",     { 0, 4, 10, 14 } },
            { "dim",   { 0, 3, 6 } },
            { "m7b5",  { 0, 3, 6, 10 } },
            { "sus2",  { 0, 2, 7 } },
            { "sus4",  { 0, 5, 7 } },
            { "add9",  { 0, 4, 7, 14 } },
            { "m11",   { 0, 3, 10, 14, 17 } },
            { "13",    { 0, 4, 10, 14, 21 } },
            { "6/9",   { 0, 4, 9, 14 } },
            { "6",     { 0, 4, 7, 9 } },
            { "7b9",   { 0, 4, 10, 13 } },
            { "madd9", { 0, 3, 7, 14 } },
            { "7sus4", { 0, 5, 7, 10 } }
        };
        return q;
    }

    //==========================================================================
    // Styles : qualités possibles par fonction (du plus simple au plus riche)
    // et progressions typiques, rangées par longueur (2 à 8 accords).
    struct StyleDef
    {
        const char* name;
        std::vector<Q> M, m, d, o;
        std::vector<const char*> maj, min;
    };

    const std::vector<StyleDef>& styles()
    {
        static const std::vector<StyleDef> s = {
            { "House",
              { QMaj, QMaj7, QMaj9 }, { QMin, QMin7, QMin9 }, { QDom7, QDom9 }, { QDim, QHalfDim },
              { "vi V IV V", "I iii IV V", "IV V vi vi", "vi IV I V", "I V vi IV", "I IV vi V", "IV I V vi",
                "vi V IV I", "I vi IV V", "IV V I vi", "ii V I IV", "vi IV", "I IV", "IV V", "vi V", "I V",
                "I IV V", "vi IV V", "IV V vi", "I vi IV", "vi IV I V vi IV", "I V vi iii IV I",
                "IV V vi V IV V", "vi IV I V vi IV V V", "I V vi IV I V IV V", "vi V IV V vi V IV I" },
              { "i bVII bVI bVII", "i iv bVII bVII", "i bVI bVII i", "i bIII bVII iv", "i bVI bIII bVII",
                "i bVII bVI V", "i iv bVI bVII", "bVI bVII i i", "i bVII bVI iv", "i IV bVII i", "i IV i IV",
                "i bVII", "i bVI", "i iv", "bVI bVII", "i bVII bVI", "i bVI bVII", "i iv bVII", "bVI bVII i",
                "i bVII bVI bVII i bVII", "i bVI bIII bVII iv bVII", "i iv bVII bIII bVI bVII",
                "i bVII bVI bVII i bVII bVI V", "i bVI bIII bVII i bVI iv V", "i iv bVII bIII i iv bVI V" } },

            { "Deep House",
              { QMaj7, QMaj9, QMaj69 }, { QMin7, QMin9, QMin11 }, { QDom7, QDom9, QDom13 }, { QHalfDim },
              { "I iii IV V", "ii V I vi", "IV iii ii I", "I vi ii V", "iii vi ii V", "I IV iii vi",
                "IV V iii vi", "ii iii IV V", "I iii vi IV", "IV I ii V", "ii V", "IV iii", "I iii", "IV V",
                "ii V I", "I iii IV", "vi ii V", "IV iii ii", "ii V I vi ii V", "I iii IV V I vi",
                "IV iii ii I vi ii", "ii V I vi ii V I I", "I iii IV iii ii V I vi", "IV iii ii I vi ii V I" },
              { "i iv bVII bIII", "i bVI bIII bVII", "i bVII bVI iv", "i iv i bVII", "i IV bVII i",
                "i bVI iv V", "iio V i bVI", "iv bVII bIII bVI", "i ii IV i", "i iv", "i bVII", "iv bVII",
                "i bVI", "i iv bVII", "iio V i", "i bVI bVII", "i iv bVII bIII bVI iio", "i bVII bVI iv iio V",
                "i iv bVII bIII i bVII", "i iv bVII bIII bVI iio V i", "i bVI bIII bVII i iv bVII bVII",
                "iv bVII bIII bVI iio V i i" } },

            { "Afro House",
              { QMaj, QAdd9, QMaj7 }, { QMin, QMin7, QMin9 }, { QDom7, QDom9 }, { QDim },
              { "I IV V IV", "I vi IV V", "IV I V vi", "I IV I V", "I V IV V", "vi IV V I", "I IV vi V",
                "I iii IV V", "I bVII IV I", "I IV", "I V", "I vi", "IV V", "I IV V", "I vi IV", "I bVII IV",
                "IV V I", "I IV V IV I V", "I vi IV V I IV", "I IV I V IV V", "I IV V IV I vi IV V",
                "I vi IV V I IV V V", "I IV V IV I IV V I" },
              { "i bVII bVI bVII", "i iv i bVII", "i bVI bVII i", "i bIII bVII bVI", "i iv bVII bIII",
                "i bVII bVI V", "i IV bVII i", "i bVII", "i iv", "i bVI", "bVI bVII", "i bVII bVI",
                "i iv bVII", "i bVI bVII", "i bVII bVI bVII i iv", "i iv bVII bIII bVI bVII",
                "i bVI bVII i iv bVII", "i bVII bVI bVII i iv bVII bVII", "i iv i bVII i iv bVII V",
                "i bVI bVII i bIII bVII bVI bVII" } },

            { "Disco / Funk",
              { QMaj7, QMaj9, QMaj6 }, { QMin7, QMin9, QMin11 }, { QDom7, QDom9, QDom13 }, { QHalfDim },
              { "I VI7 ii V7", "ii V7 I VI7", "I IV7 ii V7", "I iii ii V7", "I IV iii vi", "vi ii V I",
                "IV iii ii I", "I vi IV V", "I bVII IV I", "ii V7 iii vi", "I IV", "ii V7", "I VI7", "IV V",
                "I IV V", "ii V7 I", "I vi ii", "I VI7 ii V7 I IV", "ii V7 I VI7 ii V7", "vi ii V I IV V",
                "I VI7 ii V7 iii VI7 ii V7", "ii V7 I VI7 ii V7 I I", "I IV iii vi ii V7 I I" },
              { "i iv bVII7 bIII", "i bVI bVII i", "i iv V7 i", "i bVII7 bVI V7", "i bVII bVI V",
                "i iv bVII bVI", "iio V7 i i", "i IV bVII i", "i iv", "i bVII", "iio V7", "i IV", "i iv V7",
                "i bVII bVI", "iio V7 i", "i iv bVII7 bIII bVI iio", "i bVII bVI V7 i iv",
                "i iv V7 i bVII bVI", "i iv bVII7 bIII bVI iio V7 i", "i bVII bVI bVII i iv V7 i",
                "iio V7 i i iv bVII bIII V7" } },

            { "Jazz",
              { QMaj7, QMaj9, QMaj69 }, { QMin7, QMin9, QMin11 }, { QDom9, QDom13, QDom7b9 }, { QHalfDim },
              { "ii V7 I vi", "ii V7 I I", "I vi ii V7", "iii vi ii V7", "I VI7 ii V7", "ii V7 iii VI7",
                "I I7 IV iv", "vi ii V7 I", "IV iv I V7", "ii V7", "I vi", "iii VI7", "IV iv", "ii V7 I",
                "I vi ii", "iii VI7 ii", "ii V7 I vi ii V7", "iii VI7 ii V7 I I", "I vi ii V7 iii VI7",
                "iii vi ii V7 I vi ii V7", "ii V7 I VI7 ii V7 I I", "I vi ii V7 iii VI7 ii V7",
                "ii V7 iii VI7 ii V7 I I" },
              { "iio V7 i i", "iio V7 i bVI", "i iv bVII bIII", "i bVI iio V7", "i iv V7 i", "i iio V7 i",
                "iv bVII bIII bVI", "i bVI bVII bIII", "iio V7", "i iv", "i bVI", "iv V7", "iio V7 i",
                "i iv V7", "i bVI iio", "iio V7 i bVI iio V7", "i iv bVII bIII bVI iio", "i bVI iio V7 i iv",
                "iio V7 i bVI iio V7 i i", "i iv bVII bIII bVI iio V7 i", "i bVI iio V7 i iv V7 i" } },

            { "Neo-Soul",
              { QMaj7, QMaj9, QAdd9 }, { QMin9, QMin11, QMin7 }, { QDom9, QDom13, QDom7sus }, { QHalfDim },
              { "ii iii IV iii", "IV iii ii I", "I IV iii vi", "I iii IV iv", "vi IV I V", "IV iii vi V",
                "ii V iii vi", "I vi ii iii", "IV V iii vi", "I iii vi IV", "IV iii", "ii iii", "I iii",
                "IV V", "IV iii ii", "I iii IV", "ii iii IV", "ii iii IV iii ii V", "IV iii ii I vi ii",
                "I IV iii vi ii V", "ii iii IV iii ii iii IV V", "IV iii ii I IV iii vi V",
                "I iii IV iv I iii IV V" },
              { "i iv bVII bIII", "bVI bVII i i", "i bVI bVII iv", "i bVII bVI bVII", "bVI V i iv",
                "i bIII bVI bVII", "i IV bVII bIII", "i iv bVI bVII", "bVI bVII bIII i", "i iv", "bVI bVII",
                "i bVII", "bVI V", "i bVI bVII", "bVI bVII i", "i iv bVII", "i iv bVII bIII bVI bVII",
                "bVI bVII i i iv bVII", "i bVII bVI bVII iv bVII", "i iv bVII bIII bVI bVII i i",
                "bVI bVII i i bVI bVII iv V", "i bIII bVI bVII i iv bVII V" } },

            { "Lo-Fi",
              { QMaj7, QMaj9, QMaj6 }, { QMin7, QMin9, QMin11 }, { QDom7, QDom9 }, { QHalfDim },
              { "ii V I vi", "I vi ii V", "iii vi ii V", "IV iii ii I", "I IV iii vi", "IV I ii V",
                "vi ii V I", "I iii IV V", "ii V", "I vi", "IV iii", "iii vi", "ii V I", "I vi ii",
                "iii vi ii", "ii V I vi ii V", "I vi ii V iii vi", "IV iii ii I vi ii", "ii V I vi ii V I I",
                "I vi ii V I vi ii V", "iii vi ii V I IV iii vi" },
              { "i iv bVII bIII", "i bVI iio V", "i bVII bVI V", "i iv bII bVII", "iio V i bVI",
                "i bVI iv bVII", "i bIII iv bVII", "bVI bVII i i", "i iv", "iio V", "i bVI", "bVI bVII",
                "iio V i", "i iv bVII", "i bVI iio", "i bVI iio V i iv", "i iv bVII bIII bVI iio",
                "iio V i bVI iv bVII", "i bVI iio V i iv bVII bIII", "iio V i bVI iio V i i",
                "i iv bVII bIII bVI iio V i" } },

            { "R&B",
              { QMaj7, QMaj9, QAdd9 }, { QMin7, QMin9, QMin11 }, { QDom7, QDom9 }, { QHalfDim },
              { "I iii IV V", "ii V I vi", "I vi IV V", "IV V iii vi", "I IV vi V", "vi IV I V", "IV iii vi V",
                "I V vi IV", "ii iii IV V", "I vi", "IV V", "ii V", "I iii", "I iii IV", "ii V I", "vi IV V",
                "I iii IV V I vi", "ii V I vi IV V", "I vi IV V iii vi", "I iii IV V I vi IV V",
                "ii V I vi ii V I I", "I vi IV V iii vi IV V" },
              { "i bVI bIII bVII", "i iv bVII bIII", "i bIII bVI bVII", "bVI bVII i iv", "i bVII bVI V",
                "i iv bVI V", "iio V i bVI", "i bVI iv bVII", "i bVI", "bVI bVII", "i iv", "iv bVII",
                "i bVI bVII", "i iv bVII", "iio V i", "i bVI bIII bVII iv bVII", "i iv bVII bIII bVI V",
                "bVI bVII i i iv V", "i bVI bIII bVII i iv bVII V", "i iv bVII bIII i bVI iv V",
                "bVI bVII i i bVI bVII iv V" } },

            { "Pop",
              { QMaj, QAdd9, QSus2 }, { QMin, QMin7, QMadd9 }, { QDom7 }, { QDim },
              { "I V vi IV", "vi IV I V", "I IV vi V", "I vi IV V", "I IV V IV", "I V IV V", "vi V IV V",
                "IV I V vi", "I iii IV V", "I V vi iii", "IV V I vi", "I IV", "I V", "vi IV", "IV V", "I IV V",
                "I V IV", "vi IV I", "I vi IV", "I V vi iii IV I", "I V vi IV I V", "vi IV I V vi IV",
                "I vi IV V I IV", "I V vi IV I V vi IV", "vi IV I V vi IV I V", "I V vi iii IV I IV V",
                "I IV vi V I IV V V" },
              { "i bVI bIII bVII", "i iv bVI V", "i bVII bVI bVII", "bVI bIII bVII i", "i bVII bVI V",
                "i iv bVII bIII", "i bIII bVII bVI", "bVI bVII i iv", "i bVI", "i bVII", "bVI bVII", "i iv",
                "i bVI bVII", "i bVII bVI", "bVI bVII i", "i bVI bIII bVII i bVII", "i iv bVI bVII i V",
                "bVI bIII bVII i iv V", "i bVI bIII bVII i bVI bIII bVII", "i iv bVI V i bVI bVII V",
                "bVI bIII bVII i bVI bIII bVII V" } },

            { "Techno / Minimal",
              { QMaj, QSus4 }, { QMin, QMin7, QSus2 }, { QDom7 }, { QDim },
              { "I I IV IV", "I V I V", "I I V V", "I I I IV", "I IV I IV", "I I bVII I", "I IV", "I V", "I I",
                "IV V", "I I IV", "I IV V", "I I V", "I I IV IV V V", "I I I IV I V", "I I IV IV I I V V",
                "I I I I IV IV V V" },
              { "i i bVII bVI", "i i i bII", "i bVII i bVII", "i i iv i", "i i bVI bVII", "i bII i bVII",
                "i i bVII i", "i bVI i bVII", "i bVII", "i bII", "i iv", "i i", "i bVI", "i v", "i i bVII",
                "i bVII bVI", "i i bII", "i iv i", "i i bVII bVII bVI bVI", "i i i bVII i bII",
                "i i iv i bVII bVII", "i i bVII bVI i i bVII bVII", "i i i bII i i bVII bVII",
                "i bVII i bVII i iv bVI bVII" } }

        };
        return s;
    }

    //==========================================================================
    // Humeurs : mode + progressions caractéristiques + "richesse" des accords
    struct MoodDef { const char* name; bool minor; std::vector<const char*> progs; float richness; };

    const std::vector<MoodDef>& moods()
    {
        static const std::vector<MoodDef> m = {
            { "Dansant", false,
              { "I V vi IV", "vi IV I V", "I IV V IV", "IV V iii vi", "I iii IV V", "IV I V vi", "vi V IV V",
                "I bVII IV I", "I IV bVII IV", "I IV vi V", "vi IV", "I IV", "I V", "IV V", "I IV V",
                "vi IV I", "I vi IV", "I V vi IV I V", "vi IV I V vi IV", "I IV V IV I V", "IV V vi iii IV V",
                "I V vi IV I V vi IV", "vi IV I V vi IV I V", "I IV V IV I IV V V", "IV V iii vi IV V I I" },
              -0.05f },

            { "Joyeux", false,
              { "I IV V I", "I V IV I", "I vi ii V", "IV I V vi", "I iii vi IV", "I IV I V", "I V vi IV",
                "I IV V IV", "IV V I I", "I V", "I IV", "IV I", "I vi", "I IV V", "I V IV", "IV I V",
                "I IV V I IV V", "I V IV I V IV", "I vi IV V I IV", "I IV V I I IV V I", "I V vi IV I V IV I",
                "I vi ii V I IV V I" },
              -0.15f },

            { "Énergique", false,
              { "I V vi IV", "I bVII IV V", "vi V IV I", "I IV V V", "I bIII IV V", "vi IV V I", "I V bVII IV",
                "I bVII", "I V", "vi V", "I bIII", "I bVII IV", "I bIII IV", "vi V IV", "I bVII IV I bVII IV",
                "vi V IV I V IV", "I V bVII IV I V", "I bVII IV V I bVII IV V", "vi V IV I vi V IV V",
                "I V vi IV I V bVII IV" },
              -0.10f },

            { "Romantique", false,
              { "I vi IV V", "I iii IV iv", "I V vi iii IV I IV V", "IV iv I I", "I vi ii V", "I IV iv I",
                "iii vi ii V", "I iii vi V", "I vi", "IV iv", "I iii", "ii V", "I vi IV", "ii V I", "I iii IV",
                "I vi IV V I IV", "I iii IV iv I V", "iii vi ii V I IV", "I vi ii V iii vi ii V",
                "I iii IV iv I vi ii V" },
              0.10f },

            { "Planant", false,
              { "I IV I IV", "I iii IV I", "I II IV I", "I II iii IV", "IV V I I", "I V II IV", "IV I IV V",
                "I IV", "I II", "IV I", "I iii", "I II IV", "I IV V", "IV I iii", "I IV I IV I V",
                "I II IV I II IV", "IV V I I IV V", "I IV I IV I II IV V", "I iii IV I I iii IV V",
                "I II IV I V II IV I" },
              0.20f },

            { "Nostalgique", false,
              { "I vi IV V", "IV iv I vi", "I vi iii IV", "vi IV I iv", "I iv bVII I", "vi ii V I",
                "I iii vi ii", "I vi", "vi IV", "I iv", "IV iv", "I vi IV", "I iv I", "vi IV I",
                "I vi IV V I iv", "vi IV I iv vi IV", "I vi iii IV I V", "I vi IV V I vi iii IV",
                "IV iv I vi IV iv I V", "vi IV I iv vi IV I V" },
              0.05f },

            { "Mélancolique", true,
              { "i iv bVII bIII", "i bVI bIII bVII", "i iv i V", "i v bVI iv", "i bVI iv V", "i bIII bVII iv",
                "iv bVII bIII bVI", "i bVII bVI iv", "i iv", "i bVII", "bVI bVII", "i bVI", "i bVII bVI",
                "i iv bVII", "i bVI bVII", "i iv bVII bIII bVI iio", "i bVI bIII bVII iv bVII",
                "i bVII bVI iv i V", "i iv bVII bIII bVI iio V i", "i bVI bIII bVII i iv bVII V",
                "i v bVI iv i bVII bVI V" },
              0.10f },

            { "Triste", true,
              { "i bVI bIII bVII", "i iv bVII bIII", "i bVII bVI V", "i bVI iv V", "i bVII bVI bVII",
                "i iv v i", "i bVI bVII i", "bVI bVII i i", "i bVI", "bVI bVII", "i bVII", "i iv",
                "i bVI bVII", "i bVII bVI", "i iv bVII", "i bVI bIII bVII i V", "i iv bVII bIII bVI V",
                "i bVII bVI V i iv", "i bVI bIII bVII i bVI iv V", "i iv bVII bIII bVI iv i V",
                "i bVII bVI V i bVII bVI V" },
              -0.05f },

            { "Sombre", true,
              { "i bII i bVII", "i bVII bVI bVII", "i i bVII bVI", "i bVI bVII i", "i bII bIII bII",
                "i bII v i", "i iv bII i", "i bII", "i bVII", "i bVI", "bII i", "i bII bVII", "i bVII bVI",
                "i bII i", "i bII i bVII bVI bVII", "i i bVII bVI bII i", "i bVI bVII i bII bVII",
                "i bII i bVII i bII bIII bII", "i i bVII bVI i i bII bVII", "i bVI bVII i bII bVII bVI bVII" },
              -0.05f },

            { "Mystérieux", true,
              { "i bII bVII i", "i bIII bVI bII", "i iv bII V", "i bVI bII V", "i bVII bII i", "i bV i bV",
                "i bII bVII bVI", "i bII", "i bV", "i bIII", "bII i", "i bII bVII", "i bIII bII", "i bVI bII",
                "i bII bVII i bIII bVI", "i iv bII V i bII", "i bVI bII V i bV", "i bII bVII i bIII bVI bII V",
                "i bV i bV i bII bVII i", "i iv bII V i bVI bII bVII" },
              0.10f },

            { "Tendu", true,
              { "i iv V i", "i bVI V V", "i V bVI V", "i iv bII V", "i bII V i", "i bVI iv V", "iio V i i",
                "i V", "i bII", "iv V", "i bVI", "i iv V", "i bVI V", "iio V i", "i iv V i bVI V",
                "i bII V i iv V", "iio V i bVI iv V", "i iv V i bVI iv V V", "i bII V i iv bII V V",
                "iio V i i iv bVI V V" },
              0.00f }

        };
        return m;
    }

    // Progressions courantes, valables pour toutes les humeurs
    const std::vector<const char*>& libMajor()
    {
        static const std::vector<const char*> v = {
            "I IV", "I V", "vi IV", "IV V", "ii V", "I vi", "I IV V", "ii V I", "vi IV V", "I V vi",
            "IV V I", "I V vi IV", "vi IV I V", "I vi IV V", "I IV V IV", "ii V I vi", "I IV vi V",
            "I vi ii V", "IV I V vi", "I iii IV V", "I IV V IV I", "vi IV I V I", "I vi ii V I",
            "ii V I vi IV", "I V vi iii IV I", "I vi IV V I IV", "ii V I vi ii V", "I V vi iii IV I V",
            "I vi IV V I IV V", "I V vi IV I V vi IV", "I vi ii V iii vi ii V" };
        return v;
    }

    const std::vector<const char*>& libMinor()
    {
        static const std::vector<const char*> v = {
            "i bVII", "i bVI", "i iv", "bVI bVII", "i bVII bVI", "i iv V", "iio V i", "i bVI bVII",
            "i bVI bIII bVII", "i iv bVII bIII", "i bVII bVI V", "i bVI iv V", "i iv V i", "iio V i bVI",
            "i bVI bIII bVII V", "i iv bVII bIII V", "i bVII bVI bVII i", "i bVII bVI V i iv",
            "i bVI bIII bVII iv V", "i bVI bIII bVII i iv V", "i iv bVII bIII bVI iio V",
            "i bVI bIII bVII i bVI iv V", "i iv bVII bIII bVI iio V i" };
        return v;
    }


    //==========================================================================
    // Générateur harmonique : une marche aléatoire sur les degrés de la gamme.
    // Les poids reprennent les enchaînements les plus courants de la musique
    // populaire (IV→V/I, ii→V, vi→IV/ii, bVII→IV/I … ; en mineur i→bVII/bVI/iv,
    // V→i …), puis sont modulés par l'humeur et par le style.
    struct Edge { const char* to; double w; };
    using Graph = std::map<std::string, std::vector<Edge>>;

    const Graph& graphMajor()
    {
        static const Graph g = {
            { "I",    { { "IV", 3.0 }, { "V", 3.0 }, { "vi", 3.0 }, { "ii", 1.5 }, { "iii", 1.0 }, { "bVII", 0.8 },
                        { "II", 0.4 }, { "bIII", 0.4 }, { "iv", 0.3 } } },
            { "ii",   { { "V", 5.0 }, { "IV", 1.0 }, { "vi", 0.6 }, { "iii", 0.5 }, { "I", 0.3 } } },
            { "iii",  { { "vi", 3.0 }, { "IV", 3.0 }, { "ii", 1.0 } } },
            { "IV",   { { "V", 3.0 }, { "I", 2.5 }, { "vi", 1.5 }, { "ii", 1.0 }, { "iii", 0.7 }, { "iv", 0.5 }, { "bVII", 0.5 } } },
            { "V",    { { "I", 3.5 }, { "vi", 3.0 }, { "IV", 2.0 }, { "iii", 0.5 } } },
            { "vi",   { { "IV", 4.0 }, { "ii", 2.5 }, { "V", 2.5 }, { "iii", 1.0 }, { "I", 1.0 } } },
            { "bVII", { { "IV", 3.0 }, { "I", 3.0 } } },
            { "II",   { { "IV", 3.0 }, { "I", 2.0 }, { "V", 1.0 } } },
            { "bIII", { { "IV", 3.0 }, { "bVII", 1.5 }, { "I", 1.0 } } },
            { "iv",   { { "I", 3.0 }, { "V", 1.0 }, { "bVII", 1.0 } } }
        };
        return g;
    }

    const Graph& graphMinor()
    {
        static const Graph g = {
            { "i",    { { "bVII", 3.0 }, { "bVI", 3.0 }, { "iv", 2.5 }, { "bIII", 2.0 }, { "V", 1.5 }, { "iio", 0.8 },
                        { "bII", 0.4 }, { "IV", 0.6 }, { "v", 0.5 } } },
            { "bVII", { { "bVI", 3.0 }, { "bIII", 3.0 }, { "i", 2.5 }, { "iv", 1.0 } } },
            { "bVI",  { { "bVII", 3.0 }, { "V", 2.5 }, { "iv", 2.0 }, { "bIII", 1.5 }, { "i", 1.0 } } },
            { "bIII", { { "bVII", 3.0 }, { "iv", 2.0 }, { "bVI", 2.0 }, { "i", 1.5 } } },
            { "iv",   { { "i", 2.5 }, { "bVII", 3.0 }, { "V", 2.5 }, { "bVI", 1.5 }, { "bII", 0.5 } } },
            { "V",    { { "i", 6.0 }, { "bVI", 1.5 } } },
            { "iio",  { { "V", 6.0 } } },
            { "v",    { { "bVI", 2.0 }, { "i", 2.0 }, { "iv", 1.0 } } },
            { "bII",  { { "i", 2.0 }, { "V", 2.0 }, { "bVII", 1.0 } } },
            { "IV",   { { "i", 3.0 }, { "bVII", 2.0 }, { "V", 1.0 }, { "bVI", 0.5 } } }
        };
        return g;
    }

    // Coefficient appliqué à un degré selon l'humeur et le style
    double tokenFactor (const std::string& t, const std::string& mood, const std::string& style)
    {
        double f = 1.0;

        if (mood == "Sombre" || mood == "Mystérieux" || mood == "Tendu")
        {
            if (t == "bII") f *= 4.0;
        }
        if (mood == "Tendu")
        {
            if (t == "V") f *= 2.0;
            if (t == "iio") f *= 3.0;
            if (t == "iv") f *= 1.5;
        }
        else if (mood == "Planant")
        {
            if (t == "II") f *= 3.0;
            if (t == "iii") f *= 1.5;
            if (t == "I") f *= 1.5;
            if (t == "ii") f *= 0.5;
        }
        else if (mood == "Joyeux" || mood == "Dansant")
        {
            if (t == "V" || t == "IV") f *= 1.3;
            if (t == "ii") f *= 0.5;
        }
        else if (mood == "Romantique")
        {
            if (t == "iii") f *= 1.6;
            if (t == "ii") f *= 1.5;
            if (t == "vi") f *= 1.3;
            if (t == "iv") f *= 2.0;
        }
        else if (mood == "Nostalgique")
        {
            if (t == "vi") f *= 1.6;
            if (t == "iv") f *= 3.0;
            if (t == "bVII") f *= 1.5;
        }
        else if (mood == "Énergique")
        {
            if (t == "bVII") f *= 2.0;
            if (t == "bIII") f *= 2.5;
            if (t == "V") f *= 1.3;
        }
        else if (mood == "Mélancolique")
        {
            if (t == "iv" || t == "iio") f *= 1.5;
            if (t == "bVII") f *= 1.3;
            if (t == "v" || t == "IV") f *= 1.6;
        }
        else if (mood == "Triste")
        {
            if (t == "bVI") f *= 1.5;
            if (t == "V") f *= 1.3;
            if (t == "v") f *= 2.0;
        }

        if (style == "Jazz")
        {
            if (t == "ii" || t == "iio") f *= 2.0;
            if (t == "V") f *= 1.5;
        }
        else if (style == "Deep House" || style == "Neo-Soul" || style == "Lo-Fi" || style == "R&B")
        {
            if (t == "iii") f *= 1.6;
            if (t == "ii") f *= 1.4;
            if (t == "iv" || t == "iio") f *= 1.3;
        }
        else if (style == "House" || style == "Pop" || style == "Afro House")
        {
            if (t == "V" || t == "IV") f *= 1.2;
        }
        else if (style == "Disco / Funk")
        {
            if (t == "ii") f *= 1.4;
            if (t == "V") f *= 1.3;
        }
        return f;
    }

    std::string markovProgression (std::mt19937& rng, int n, bool minor, const std::string& mood, const std::string& style)
    {
        const Graph& G = minor ? graphMinor() : graphMajor();
        const bool repeats = (style == "Techno / Minimal");
        auto u01 = [&] { return std::uniform_real_distribution<double> (0.0, 1.0) (rng); };

        auto pick = [&] (const std::vector<std::pair<std::string, double>>& c) -> std::string
        {
            double total = 0.0;
            for (const auto& e : c) total += e.second;
            double x = u01() * total;
            for (const auto& e : c)
            {
                x -= e.second;
                if (x <= 0.0)
                    return e.first;
            }
            return c.back().first;
        };

        std::vector<std::pair<std::string, double>> starts;
        if (minor)
            starts = { { "i", 8.0 }, { "bVI", 1.0 }, { "iv", 1.0 } };
        else
            starts = { { "I", 6.0 }, { "vi", 2.5 }, { "IV", 1.5 } };

        auto goodEnd = [&] (const std::string& t)
        {
            if (minor)
                return t == "i" || t == "bVII" || t == "V" || t == "iv" || t == "bVI";
            return t == "I" || t == "IV" || t == "V" || t == "vi";
        };

        std::vector<std::string> best;
        for (int attempt = 0; attempt < 14; ++attempt)
        {
            std::vector<std::string> seq { pick (starts) };
            while ((int) seq.size() < n)
            {
                const std::string& cur = seq.back();
                std::vector<std::pair<std::string, double>> c;
                const auto it = G.find (cur);
                double total = 0.0;
                if (it != G.end())
                    for (const auto& e : it->second)
                    {
                        const double w = e.w * tokenFactor (e.to, mood, style);
                        c.push_back ({ e.to, w });
                        total += w;
                    }
                if (c.empty())
                    c.push_back ({ minor ? "i" : "I", 1.0 });
                if (repeats)
                    c.push_back ({ cur, total * 0.6 });
                seq.push_back (pick (c));
            }
            best = seq;
            if (n <= 2 || goodEnd (seq.back()))
                break;
        }

        const bool seventh = (style == "Jazz" || style == "Disco / Funk");
        std::string out;
        for (size_t i = 0; i < best.size(); ++i)
        {
            std::string t = best[i];
            if (seventh && t == "V")
                t = "V7";
            out += (i ? " " : "") + t;
        }
        return out;
    }

    //==========================================================================
    // Rythmes
    struct Hit { double s, l; int v; };
    struct RhythmDef { const char* name; int kind; std::vector<Hit> hits; }; // kind : 0 normal, 1 arpège montant, 2 aller-retour

    const std::vector<RhythmDef>& rhythms()
    {
        static const std::vector<RhythmDef> r = {
            { "Tenu (1 accord par mesure)", 0, { { 0, 3.96, 88 } } },
            { "Blanches", 0, { { 0, 1.96, 90 }, { 2, 1.96, 82 } } },
            { "Noires", 0, { { 0, 0.96, 92 }, { 1, 0.96, 80 }, { 2, 0.96, 86 }, { 3, 0.96, 80 } } },
            { "Offbeat house", 0, { { 0.5, 0.4, 92 }, { 1.5, 0.4, 92 }, { 2.5, 0.4, 92 }, { 3.5, 0.4, 92 } } },
            { "Syncopé 3-3-2", 0, { { 0, 0.9, 95 }, { 1.5, 0.9, 88 }, { 3, 0.9, 90 } } },
            { "Charleston", 0, { { 0, 1.4, 95 }, { 1.5, 0.45, 85 } } },
            { "Stabs funk", 0, { { 0, 0.25, 100 }, { 0.75, 0.25, 80 }, { 1.5, 0.25, 95 }, { 2.5, 0.25, 85 }, { 3.25, 0.5, 90 } } },
            { "Pulse croches", 0, { { 0, 0.4, 100 }, { 0.5, 0.4, 80 }, { 1, 0.4, 95 }, { 1.5, 0.4, 80 },
                                    { 2, 0.4, 100 }, { 2.5, 0.4, 80 }, { 3, 0.4, 95 }, { 3.5, 0.4, 80 } } },
            { "Arpège montant", 1, {} },
            { "Arpège aller-retour", 2, {} },
            { "Tenu + reprise", 0, { { 0, 2.46, 90 }, { 2.5, 1.46, 80 } } }
        };
        return r;
    }

    //==========================================================================
    const char* const kNoteNames[12] = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
    const char* const kKeyListNames[12] = { "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    const char* const kSharpNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    // Les tonalités "à dièses" épellent leurs notes avec des dièses, les autres avec des bémols
    bool useSharps (int tonic, bool minor)
    {
        static const bool majorSharp[12] = { false, false, true, false, true, false, true, true, false, true, false, true };
        static const bool minorSharp[12] = { false, true, false, false, true, false, true, false, true, false, false, true };
        return minor ? minorSharp[tonic] : majorSharp[tonic];
    }

    struct Degree { int offset; char fn; std::string label; };

    std::vector<std::string> tokenize (const std::string& s)
    {
        std::vector<std::string> out;
        std::string cur;
        for (char c : s)
        {
            if (c == ' ')
            {
                if (! cur.empty()) { out.push_back (cur); cur.clear(); }
            }
            else
                cur += c;
        }
        if (! cur.empty())
            out.push_back (cur);
        return out;
    }

    // "bVII", "ii", "V7", "iio" ...
    Degree parseToken (const std::string& tok)
    {
        size_t i = 0;
        int acc = 0;
        while (i < tok.size() && (tok[i] == 'b' || tok[i] == '#'))
        {
            acc += (tok[i] == '#') ? 1 : -1;
            ++i;
        }

        std::string rom;
        while (i < tok.size() && std::strchr ("IViv", tok[i]) != nullptr && tok[i] != '\0')
            rom += tok[i++];

        const bool upper = ! rom.empty() && std::isupper ((unsigned char) rom[0]);
        std::string low;
        for (char c : rom)
            low += (char) std::tolower ((unsigned char) c);

        int base = 0;
        if      (low == "i")   base = 0;
        else if (low == "ii")  base = 2;
        else if (low == "iii") base = 4;
        else if (low == "iv")  base = 5;
        else if (low == "v")   base = 7;
        else if (low == "vi")  base = 9;
        else if (low == "vii") base = 11;

        char fn = upper ? 'M' : 'm';
        for (; i < tok.size(); ++i)
        {
            if (tok[i] == '7' && upper) fn = 'd';
            if (tok[i] == 'o')          fn = 'o';
        }

        Degree d;
        d.offset = ((base + acc) % 12 + 12) % 12;
        d.fn = fn;
        d.label = tok;
        return d;
    }

    double meanOf (const std::vector<int>& v)
    {
        if (v.empty())
            return 60.0;
        double s = 0.0;
        for (int x : v)
            s += x;
        return s / (double) v.size();
    }

    // Construit une disposition (voicing) de l'accord, la plus proche possible de la cible
    std::vector<int> makeVoicing (const std::vector<int>& iv, int rootPc, double target, int type)
    {
        const int n = (int) iv.size();
        std::vector<int> best;
        double bestScore = 1e9;

        for (int inv = 0; inv < n; ++inv)
        {
            std::vector<int> v;
            for (int i = 0; i < n; ++i)
                v.push_back (48 + rootPc + iv[(size_t) i] + (i < inv ? 12 : 0));
            std::sort (v.begin(), v.end());

            const double c = meanOf (v);
            const int shift = (int) std::lround ((target - c) / 12.0) * 12;
            for (auto& x : v)
                x += shift;

            const double score = std::abs (meanOf (v) - target) + inv * 0.4;
            if (score < bestScore)
            {
                bestScore = score;
                best = v;
            }
        }

        if (type == 1 && best.size() >= 3)  // drop 2 : la deuxième note la plus haute descend d'une octave
        {
            best[best.size() - 2] -= 12;
            std::sort (best.begin(), best.end());
        }
        return best;
    }

    void clampRegister (std::vector<int>& v)
    {
        if (v.empty())
            return;
        while (*std::max_element (v.begin(), v.end()) > 96)
            for (auto& x : v) x -= 12;
        while (*std::min_element (v.begin(), v.end()) < 28)
            for (auto& x : v) x += 12;
    }
}

//==============================================================================
const std::vector<std::string>& styleNames()
{
    static const std::vector<std::string> n = [] { std::vector<std::string> v; for (auto& s : styles()) v.push_back (s.name); return v; }();
    return n;
}

const std::vector<std::string>& moodNames()
{
    static const std::vector<std::string> n = [] { std::vector<std::string> v; for (auto& s : moods()) v.push_back (s.name); return v; }();
    return n;
}

const std::vector<std::string>& rhythmNames()
{
    static const std::vector<std::string> n = [] { std::vector<std::string> v; for (auto& s : rhythms()) v.push_back (s.name); return v; }();
    return n;
}

const std::vector<std::string>& voicingNames()
{
    static const std::vector<std::string> n = { "Serré", "Ouvert (drop 2)", "Piano (main gauche + droite)" };
    return n;
}

const std::vector<std::string>& keyNames()
{
    static const std::vector<std::string> n (kKeyListNames, kKeyListNames + 12);
    return n;
}

std::string noteName (int midi)
{
    return std::string (kNoteNames[((midi % 12) + 12) % 12]) + std::to_string (midi / 12 - 1);
}

//==============================================================================
Progression generate (const Settings& st)
{
    std::mt19937 rng (st.seed * 2654435761u + 0x9e3779b9u);
    auto r01 = [&] { return std::uniform_real_distribution<double> (0.0, 1.0) (rng); };
    auto rint = [&] (size_t n) { return (size_t) (rng() % (uint32_t) std::max<size_t> (1, n)); };
    auto clampi = [] (int v, int lo, int hi) { return std::max (lo, std::min (hi, v)); };

    const auto& S = styles();
    const auto& Mo = moods();
    const auto& R = rhythms();
    const auto& Qs = qualities();

    const auto& style = S[(size_t) clampi (st.style, 0, (int) S.size() - 1)];
    const auto& mood = (st.mood < 0) ? Mo[rint (Mo.size())] : Mo[(size_t) clampi (st.mood, 0, (int) Mo.size() - 1)];
    // mode : -1 = celui de l'humeur, 0 = majeur, 1 = mineur
    const bool minorMode = (st.mode < 0) ? mood.minor : (st.mode == 1);
    const int key = (st.key < 0) ? (int) rint (12) : st.key % 12;
    const int n = clampi (st.numChords, 1, 8);
    // durée, rythme et disposition : valeur négative ou nulle = au hasard (tirage lié à la graine)
    double bpc = st.beatsPerChord;
    if (bpc <= 0.0)
    {
        const double x = r01();
        bpc = x < 0.25 ? 2.0 : (x < 0.80 ? 4.0 : 8.0);
    }
    const int voicingKind = (st.voicing < 0) ? (int) rint (3) : st.voicing;
    const int rhythmKind = (st.rhythm < 0) ? (int) rint (rhythms().size()) : st.rhythm;
    const float bold = std::max (0.0f, std::min (1.0f, st.boldness));
    const float human = std::max (0.0f, std::min (1.0f, st.humanize));
    const float velRand = std::max (0.0f, std::min (1.0f, st.velRandom));

    // ---- Choix de la progression
    // 1) une progression "signature" de l'humeur, 2) une du style, 3) une progression courante,
    // 4) sinon une progression inventée par le générateur harmonique (toujours de la bonne longueur).
    auto fit = [&] (const std::vector<const char*>& src)
    {
        std::vector<std::string> out;
        for (auto* p : src)
            if ((int) tokenize (p).size() == n)
                out.push_back (p);
        return out;
    };

    const auto& styleProgs = minorMode ? style.min : style.maj;
    // les progressions propres à l'humeur ne valent que dans son mode d'origine
    const auto A = (minorMode == mood.minor) ? fit (mood.progs) : std::vector<std::string>();
    const auto B = fit (styleProgs);
    const auto C = fit (minorMode ? libMinor() : libMajor());

    std::string chosen;
    const double dice = r01();
    if (dice < 0.34 && ! A.empty())
        chosen = A[rint (A.size())];
    else if (dice < 0.62 && ! B.empty())
        chosen = B[rint (B.size())];
    else if (dice < 0.72 && ! C.empty())
        chosen = C[rint (C.size())];
    else
        chosen = markovProgression (rng, n, minorMode, mood.name, style.name);

    const auto toks = tokenize (chosen);
    std::vector<Degree> degs;
    for (int i = 0; i < n; ++i)
        degs.push_back (parseToken (toks[(size_t) i % toks.size()]));

    // ---- Audace : dominantes secondaires, substitutions tritoniques, emprunts
    {
        const auto orig = degs;
        for (int k = 0; k < n; ++k)
        {
            auto& d = degs[(size_t) k];

            if (k >= 1 && k + 1 < n && orig[(size_t) k].fn != 'd' && orig[(size_t) k + 1].fn != 'o'
                && r01() < bold * 0.4)
            {
                const int target = orig[(size_t) k + 1].offset;
                if (orig[(size_t) k].offset != (target + 7) % 12)
                {
                    d.offset = (target + 7) % 12;
                    d.fn = 'd';
                    d.label = "V7/" + orig[(size_t) k + 1].label;
                    continue;
                }
            }

            if (d.fn == 'd' && r01() < bold * 0.5)
            {
                d.offset = (d.offset + 6) % 12;
                d.label += "(sub)";
                continue;
            }

            if (! minorMode && d.offset == 5 && d.fn == 'M' && r01() < bold * 0.6)
            {
                d.fn = 'm';
                d.label = "iv";
            }
        }
    }

    // ---- Qualités, disposition et événements
    Progression prog;
    prog.minor = minorMode;
    prog.key = key;
    const char* const* spell = useSharps (key, minorMode) ? kSharpNames : kNoteNames;
    prog.keyName = std::string (spell[key]) + (minorMode ? "m" : "");
    prog.styleName = style.name;
    prog.moodName = mood.name;
    prog.beatsPerChord = bpc;
    prog.lengthBeats = bpc * n;

    auto pickQ = [&] (const std::vector<Q>& list, Q fallback) -> Q
    {
        if (list.empty())
            return fallback;
        const int cnt = (int) list.size();
        const double cx = std::max (0.0, std::min (1.0, (double) st.complexity + (double) mood.richness));
        const double x = cx * (cnt - 1) + (r01() - 0.5) * 1.0;
        return list[(size_t) clampi ((int) std::lround (x), 0, cnt - 1)];
    };

    const auto& rh = R[(size_t) clampi (rhythmKind, 0, (int) R.size() - 1)];
    const double centreBase = 60.0 + 12.0 * st.octave;
    double prevCentre = centreBase;

    for (int k = 0; k < n; ++k)
    {
        const auto& d = degs[(size_t) k];

        Q q = QMaj;
        switch (d.fn)
        {
            case 'M':
                if (d.label == "V")
                {
                    // le V est un accord de dominante : triade simple, ou 7e/9e/13e si l'on monte la complexité
                    const double cxv = std::max (0.0, std::min (1.0, (double) st.complexity + (double) mood.richness));
                    q = (cxv < 0.3) ? QMaj : pickQ (style.d, QDom7);
                }
                else if (minorMode && d.label == "bVII")
                {
                    // bVII en mineur : triade simple, ou 7e/9e de dominante (Bb7 en do mineur)
                    const double cxv = std::max (0.0, std::min (1.0, (double) st.complexity + (double) mood.richness));
                    q = (cxv < 0.3) ? QMaj : pickQ (style.d, QDom7);
                }
                else if (d.label == "II" || (minorMode && d.label == "IV"))
                {
                    // II en majeur (lydien) et IV en mineur (dorien) : ce sont des accords de 7e de dominante
                    q = pickQ (style.d, QDom7);
                }
                else
                    q = pickQ (style.M, QMaj);
                break;
            case 'm':
                if (d.offset == 4 && ! minorMode)
                {
                    // iii en majeur : sa 9e (fa#) sort de la tonalité, on reste sur triade ou 7e
                    q = (st.complexity + mood.richness < 0.45f) ? QMin : QMin7;
                }
                else if (d.offset == 7 && minorMode)
                {
                    // v mineur naturel : pas d'extensions
                    q = (st.complexity + mood.richness < 0.45f) ? QMin : QMin7;
                }
                else
                    q = pickQ (style.m, QMin);
                break;
            case 'd': q = pickQ (style.d, QDom7); break;
            default:  q = pickQ (style.o, QDim); break;
        }

        // les accords sus ne doivent pas sortir de la tonalité (ex. Fsus4 contient un sib en do majeur)
        {
            static const bool mj[12] = { 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1 };
            static const bool mn[12] = { 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0 };
            auto inScale = [&] (int rel) { return (minorMode ? mn : mj)[((rel % 12) + 12) % 12]; };

            if (q == QSus4 && ! inScale (d.offset + 5))
                q = inScale (d.offset + 2) ? QSus2 : (d.fn == 'm' ? QMin : QMaj);
            if (q == QSus2 && ! inScale (d.offset + 2))
                q = (d.fn == 'm') ? QMin : QMaj;
            if (q == QDom7sus && ! inScale (d.offset + 5))
                q = QDom7;
        }

        const int root = (key + d.offset) % 12;
        const auto& qd = Qs[(size_t) q];

        Chord ch;
        ch.root = root;
        ch.name = std::string (spell[root]) + qd.suffix;
        ch.roman = d.label;

        const double target = std::max (centreBase - 7.0, std::min (centreBase + 7.0, prevCentre));
        std::vector<int> v = makeVoicing (qd.iv, root, target + (voicingKind == 2 ? 4.0 : 0.0), voicingKind == 1 ? 1 : 0);

        if (voicingKind == 2)
        {
            // main droite sans la fondamentale pour les accords riches, main gauche : fondamentale + quinte
            if (v.size() >= 4)
            {
                std::vector<int> upper;
                for (int x : v)
                    if (((x % 12) + 12) % 12 != root)
                        upper.push_back (x);
                if (upper.size() >= 3)
                    v = upper;
            }
            const int lowRoot = 36 + root;
            v.push_back (lowRoot);
            const bool dimFifth = std::find (qd.iv.begin(), qd.iv.end(), 6) != qd.iv.end();
            v.push_back (lowRoot + (dimFifth ? 6 : 7));
            std::sort (v.begin(), v.end());
        }
        clampRegister (v);

        // le centre utilisé pour la conduite des voix ne tient pas compte de la main gauche
        {
            std::vector<int> up;
            for (int x : v)
                if (x >= 48)
                    up.push_back (x);
            prevCentre = meanOf (up.empty() ? v : up);
        }

        ch.notes = v;
        if (st.bass && st.voicing != 2)
        {
            int b = 36 + root;
            while (b < 28) b += 12;
            ch.bassNote = b;
        }

        // ---- Événements MIDI de cet accord
        const double t0 = k * bpc;
        const double patScale = (bpc < 4.0) ? bpc / 4.0 : 1.0;
        const int reps = (bpc > 4.0) ? (int) std::lround (bpc / 4.0) : 1;
        auto vel = [&] (int base)
        {
            const double dv = (r01() - 0.5) * 2.0 * (human * 14.0 + velRand * 45.0);
            return clampi ((int) std::lround (base + dv), 1, 127);
        };

        if (rh.kind == 0)
        {
            for (int rep = 0; rep < reps; ++rep)
                for (const auto& h : rh.hits)
                {
                    const double hs = t0 + rep * 4.0 + h.s * patScale;
                    for (size_t ni = 0; ni < v.size(); ++ni)
                    {
                        const double strum = (double) ni * human * 0.02;
                        const double start = hs + strum;
                        double len = h.l * patScale - strum;
                        len = std::min (len, t0 + bpc - start - 0.02);
                        if (len <= 0.02)
                            continue;
                        prog.events.push_back ({ v[ni], start, len, vel (h.v) });
                    }
                }
        }
        else
        {
            std::vector<int> seq = v;
            if (rh.kind == 2 && seq.size() > 2)
                for (int i = (int) seq.size() - 2; i >= 1; --i)
                    seq.push_back (seq[(size_t) i]);

            const double step = 0.5 * patScale;
            const int steps = std::max (1, (int) std::lround (bpc / step));
            for (int s = 0; s < steps; ++s)
            {
                const double start = t0 + s * step;
                const double len = std::min (step * 0.9, t0 + bpc - start - 0.02);
                if (len <= 0.02)
                    continue;
                prog.events.push_back ({ seq[(size_t) s % seq.size()], start, len, vel ((s % 4 == 0) ? 100 : 82) });
            }
        }

        if (ch.bassNote >= 0)
            prog.events.push_back ({ ch.bassNote, t0, bpc - 0.04, vel (96) });

        prog.chords.push_back (ch);
    }

    std::stable_sort (prog.events.begin(), prog.events.end(),
                      [] (const Event& a, const Event& b) { return a.start < b.start; });
    return prog;
}

//==============================================================================
std::vector<unsigned char> toMidiFile (const Progression& p, int bpm)
{
    constexpr int ppq = 480;

    struct Ev { int tick; int order; int status; int d1; int d2; };
    std::vector<Ev> evs;

    for (const auto& e : p.events)
    {
        const int on = (int) std::lround (e.start * ppq);
        const int off = std::max (on + 1, (int) std::lround ((e.start + e.length) * ppq));
        evs.push_back ({ on, 1, 0x90, e.note, e.velocity });
        evs.push_back ({ off, 0, 0x80, e.note, 0 });
    }
    std::stable_sort (evs.begin(), evs.end(), [] (const Ev& a, const Ev& b)
    {
        return a.tick != b.tick ? a.tick < b.tick : a.order < b.order;
    });

    std::vector<unsigned char> trk;
    auto varlen = [&] (uint32_t v)
    {
        unsigned char buf[5];
        int n = 0;
        buf[n++] = (unsigned char) (v & 0x7f);
        while ((v >>= 7) != 0)
            buf[n++] = (unsigned char) ((v & 0x7f) | 0x80);
        while (n > 0)
            trk.push_back (buf[--n]);
    };

    // nom de piste
    const std::string name = "PokeChord " + p.keyName;
    varlen (0); trk.push_back (0xFF); trk.push_back (0x03); trk.push_back ((unsigned char) name.size());
    for (char c : name) trk.push_back ((unsigned char) c);

    // tempo
    const uint32_t mpq = (uint32_t) (60000000.0 / std::max (20, std::min (300, bpm)));
    varlen (0); trk.push_back (0xFF); trk.push_back (0x51); trk.push_back (0x03);
    trk.push_back ((unsigned char) ((mpq >> 16) & 0xff));
    trk.push_back ((unsigned char) ((mpq >> 8) & 0xff));
    trk.push_back ((unsigned char) (mpq & 0xff));

    // signature 4/4
    varlen (0); trk.push_back (0xFF); trk.push_back (0x58); trk.push_back (0x04);
    trk.push_back (4); trk.push_back (2); trk.push_back (24); trk.push_back (8);

    int last = 0;
    for (const auto& e : evs)
    {
        varlen ((uint32_t) (e.tick - last));
        last = e.tick;
        trk.push_back ((unsigned char) e.status);
        trk.push_back ((unsigned char) e.d1);
        trk.push_back ((unsigned char) e.d2);
    }

    // fin de piste : on cale la durée sur la longueur exacte de la boucle
    const int endTick = std::max (last, (int) std::lround (p.lengthBeats * ppq));
    varlen ((uint32_t) (endTick - last));
    trk.push_back (0xFF); trk.push_back (0x2F); trk.push_back (0x00);

    std::vector<unsigned char> out;
    auto put32 = [&] (uint32_t v)
    {
        out.push_back ((unsigned char) (v >> 24)); out.push_back ((unsigned char) (v >> 16));
        out.push_back ((unsigned char) (v >> 8));  out.push_back ((unsigned char) v);
    };
    auto put16 = [&] (uint32_t v) { out.push_back ((unsigned char) (v >> 8)); out.push_back ((unsigned char) v); };

    const char hdr[] = { 'M', 'T', 'h', 'd' };
    out.insert (out.end(), hdr, hdr + 4);
    put32 (6); put16 (0); put16 (1); put16 (ppq);

    const char trh[] = { 'M', 'T', 'r', 'k' };
    out.insert (out.end(), trh, trh + 4);
    put32 ((uint32_t) trk.size());
    out.insert (out.end(), trk.begin(), trk.end());
    return out;
}

} // namespace chords
