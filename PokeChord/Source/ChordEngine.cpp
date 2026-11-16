#include "ChordEngine.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <random>

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
    // et progressions typiques.
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
              { "vi V IV V", "I iii IV V", "IV V vi vi", "vi IV I V" },
              { "i bVII bVI bVII", "i iv bVII bVII", "i bVI bVII i", "i bIII bVII iv" } },

            { "Deep House",
              { QMaj7, QMaj9, QMaj69 }, { QMin7, QMin9, QMin11 }, { QDom7, QDom9, QDom13 }, { QHalfDim },
              { "I iii IV V", "ii V I vi", "IV iii ii I", "I vi ii V" },
              { "i iv bVII bIII", "i bVI bIII bVII", "i bVII bVI iv", "i iv i bVII" } },

            { "Afro House",
              { QMaj, QAdd9, QMaj7 }, { QMin, QMin7, QMin9 }, { QDom7, QDom9 }, { QDim },
              { "I IV V IV", "I vi IV V", "IV I V vi" },
              { "i bVII bVI bVII", "i iv i bVII", "i bVI bVII i", "i bIII bVII bVI" } },

            { "Disco / Funk",
              { QMaj7, QMaj9, QMaj6 }, { QMin7, QMin9, QMin11 }, { QDom7, QDom9, QDom13 }, { QHalfDim },
              { "I VI7 ii V7", "ii V7 I VI7", "I IV7 ii V7", "I iii ii V7" },
              { "i iv bVII7 bIII", "i bVI bVII i", "i iv V7 i", "i bVII7 bVI V7" } },

            { "Jazz",
              { QMaj7, QMaj9, QMaj69 }, { QMin7, QMin9, QMin11 }, { QDom9, QDom13, QDom7b9 }, { QHalfDim },
              { "ii V7 I vi", "ii V7 I I", "I vi ii V7", "iii vi ii V7", "I VI7 ii V7" },
              { "iio V7 i i", "iio V7 i bVI", "i iv bVII bIII", "i bVI iio V7", "i iv V7 i" } },

            { "Neo-Soul",
              { QMaj7, QMaj9, QAdd9 }, { QMin9, QMin11, QMin7 }, { QDom9, QDom13, QDom7sus }, { QHalfDim },
              { "ii iii IV iii", "IV iii ii I", "I IV iii vi", "I iii IV iv", "vi IV I V" },
              { "i iv bVII bIII", "bVI bVII i i", "i bVI bVII iv", "i bVII bVI bVII", "bVI V i iv" } },

            { "Lo-Fi",
              { QMaj7, QMaj9, QMaj6 }, { QMin7, QMin9, QMin11 }, { QDom7, QDom9 }, { QHalfDim },
              { "ii V I vi", "I vi ii V", "iii vi ii V", "IV iii ii I" },
              { "i iv bVII bIII", "i bVI iio V", "i bVII bVI V", "i iv bII bVII" } },

            { "R&B",
              { QMaj7, QMaj9, QAdd9 }, { QMin7, QMin9, QMin11 }, { QDom7, QDom9 }, { QHalfDim },
              { "I iii IV V", "ii V I vi", "I vi IV V", "IV V iii vi" },
              { "i bVI bIII bVII", "i iv bVII bIII", "i bIII bVI bVII", "bVI bVII i iv" } },

            { "Pop",
              { QMaj, QAdd9, QSus2 }, { QMin, QMin7, QMadd9 }, { QDom7 }, { QDim },
              { "I V vi IV", "vi IV I V", "I IV vi V", "I vi IV V" },
              { "i bVI bIII bVII", "i iv bVI V", "i bVII bVI bVII", "bVI bIII bVII i" } },

            { "Techno / Minimal",
              { QMaj, QSus4 }, { QMin, QMin7, QSus2 }, { QDom7 }, { QDim },
              { "I I IV IV", "I V I V" },
              { "i i bVII bVI", "i i i bII", "i bVII i bVII", "i i iv i" } }
        };
        return s;
    }

    //==========================================================================
    // Ambiances
    struct MoodDef { const char* name; bool minor; std::vector<const char*> progs; };

    const std::vector<MoodDef>& moods()
    {
        static const std::vector<MoodDef> m = {
            { "Dansant", false,
              { "I V vi IV", "vi IV I V", "I IV V IV", "IV V iii vi", "I iii IV V",
                "IV I V vi", "vi V IV V", "I bVII IV I", "I IV bVII IV" } },
            { "Joyeux", false,
              { "I IV V I", "I V IV I", "I vi ii V", "IV I V vi", "I iii vi IV", "I IV I V" } },
            { "Énergique", false,
              { "I V vi IV", "I bVII IV V", "vi V IV I", "I IV V V", "I bIII IV V",
                "vi IV V I", "I V bVII IV" } },
            { "Romantique", false,
              { "I vi IV V", "I iii IV iv", "I V vi iii IV I IV V", "IV iv I I",
                "I vi ii V", "I IV iv I", "iii vi ii V", "I iii vi V" } },
            { "Planant", false,
              { "I IV I IV", "I iii IV I", "I II IV I", "I II iii IV", "IV V I I", "I V II IV", "IV I IV V" } },
            { "Nostalgique", false,
              { "I vi IV V", "IV iv I vi", "I vi iii IV", "vi IV I iv", "I iv bVII I", "vi ii V I", "I iii vi ii" } },
            { "Mélancolique", true,
              { "i iv bVII bIII", "i bVI bIII bVII", "i iv i V", "i v bVI iv",
                "i bVI iv V", "i bIII bVII iv", "iv bVII bIII bVI", "i bVII bVI iv" } },
            { "Triste", true,
              { "i bVI bIII bVII", "i iv bVII bIII", "i bVII bVI V", "i bVI iv V",
                "i bVII bVI bVII", "i iv v i", "i bVI bVII i", "bVI bVII i i" } },
            { "Sombre", true,
              { "i bII i bVII", "i bVII bVI bVII", "i i bVII bVI", "i bVI bVII i",
                "i bII bIII bII", "i bII v i", "i iv bII i" } },
            { "Mystérieux", true,
              { "i bII bVII i", "i bIII bVI bII", "i iv bII V", "i bVI bII V",
                "i bVII bII i", "i bV i bV", "i bII bVII bVI" } },
            { "Tendu", true,
              { "i iv V i", "i bVI V V", "i V bVI V", "i iv bII V", "i bII V i", "i bVI iv V", "iio V i i" } }
        };
        return m;
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
    const int key = (st.key < 0) ? (int) rint (12) : st.key % 12;
    const int n = clampi (st.numChords, 1, 8);
    const double bpc = st.beatsPerChord;
    const float bold = std::max (0.0f, std::min (1.0f, st.boldness));
    const float human = std::max (0.0f, std::min (1.0f, st.humanize));

    // ---- Choix de la progression
    auto fit = [&] (const std::vector<const char*>& src)
    {
        std::vector<std::string> out;
        for (auto* p : src)
            if ((int) tokenize (p).size() == n)
                out.push_back (p);
        return out;
    };

    const auto& styleProgs = mood.minor ? style.min : style.maj;
    const auto A = fit (mood.progs);
    const auto B = fit (styleProgs);

    std::string chosen;
    if (! A.empty() && (B.empty() || r01() < 0.55))
        chosen = A[rint (A.size())];
    else if (! B.empty())
        chosen = B[rint (B.size())];
    else
        chosen = mood.progs[rint (mood.progs.size())];

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

            if (! mood.minor && d.offset == 5 && d.fn == 'M' && r01() < bold * 0.6)
            {
                d.fn = 'm';
                d.label = "iv";
            }
        }
    }

    // ---- Qualités, disposition et événements
    Progression prog;
    prog.minor = mood.minor;
    prog.key = key;
    const char* const* spell = useSharps (key, mood.minor) ? kSharpNames : kNoteNames;
    prog.keyName = std::string (spell[key]) + (mood.minor ? "m" : "");
    prog.styleName = style.name;
    prog.moodName = mood.name;
    prog.beatsPerChord = bpc;
    prog.lengthBeats = bpc * n;

    auto pickQ = [&] (const std::vector<Q>& list, Q fallback) -> Q
    {
        if (list.empty())
            return fallback;
        const int cnt = (int) list.size();
        const double x = st.complexity * (cnt - 1) + (r01() - 0.5) * 1.0;
        return list[(size_t) clampi ((int) std::lround (x), 0, cnt - 1)];
    };

    const auto& rh = R[(size_t) clampi (st.rhythm, 0, (int) R.size() - 1)];
    const double centreBase = 60.0 + 12.0 * st.octave;
    double prevCentre = centreBase;

    for (int k = 0; k < n; ++k)
    {
        const auto& d = degs[(size_t) k];

        Q q = QMaj;
        switch (d.fn)
        {
            case 'M': q = pickQ (style.M, QMaj); break;
            case 'm': q = pickQ (style.m, QMin); break;
            case 'd': q = pickQ (style.d, QDom7); break;
            default:  q = pickQ (style.o, QDim); break;
        }

        const int root = (key + d.offset) % 12;
        const auto& qd = Qs[(size_t) q];

        Chord ch;
        ch.root = root;
        ch.name = std::string (spell[root]) + qd.suffix;
        ch.roman = d.label;

        const double target = std::max (centreBase - 7.0, std::min (centreBase + 7.0, prevCentre));
        std::vector<int> v = makeVoicing (qd.iv, root, target + (st.voicing == 2 ? 4.0 : 0.0), st.voicing == 1 ? 1 : 0);

        if (st.voicing == 2)
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
            v.push_back (lowRoot + 7);
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
            const double dv = (r01() - 0.5) * 2.0 * human * 14.0;
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
