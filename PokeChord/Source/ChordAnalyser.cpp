#include "ChordAnalyser.h"
#include <algorithm>
#include <cmath>

namespace analyse
{
    namespace
    {
        const char* const kNames[12] = { "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };

        struct Tone { int iv; float w; };
        struct Template { const char* suffix; std::vector<Tone> t; float bias; };

        // poids : fondamentale 1, tierce / septième = notes qui définissent l'accord, quinte plus discrète
        const std::vector<Template>& templates()
        {
            static const std::vector<Template> t = {
                { "",      { {0,1.0f}, {4,1.0f}, {7,0.75f} },                               0.045f },
                { "m",     { {0,1.0f}, {3,1.0f}, {7,0.75f} },                               0.045f },
                { "7",     { {0,1.0f}, {4,1.0f}, {7,0.65f}, {10,0.95f} },                   0.000f },
                { "maj7",  { {0,1.0f}, {4,1.0f}, {7,0.65f}, {11,0.95f} },                   0.000f },
                { "m7",    { {0,1.0f}, {3,1.0f}, {7,0.65f}, {10,0.95f} },                   0.000f },
                { "dim",   { {0,1.0f}, {3,1.0f}, {6,0.95f} },                              -0.015f },
                { "aug",   { {0,1.0f}, {4,1.0f}, {8,0.95f} },                              -0.035f },
                { "sus2",  { {0,1.0f}, {2,0.95f}, {7,0.8f} },                              -0.020f },
                { "sus4",  { {0,1.0f}, {5,0.95f}, {7,0.8f} },                              -0.020f },
                { "m7b5",  { {0,1.0f}, {3,1.0f}, {6,0.95f}, {10,0.95f} },                  -0.015f },
                { "dim7",  { {0,1.0f}, {3,1.0f}, {6,0.95f}, {9,0.95f} },                   -0.030f },
                { "6",     { {0,1.0f}, {4,1.0f}, {7,0.65f}, {9,0.9f} },                    -0.025f },
                { "m6",    { {0,1.0f}, {3,1.0f}, {7,0.65f}, {9,0.9f} },                    -0.030f },
                { "add9",  { {0,1.0f}, {2,0.85f}, {4,1.0f}, {7,0.65f} },                   -0.025f },
                { "madd9", { {0,1.0f}, {2,0.85f}, {3,1.0f}, {7,0.65f} },                   -0.030f },
                { "7sus4", { {0,1.0f}, {5,0.95f}, {7,0.65f}, {10,0.95f} },                 -0.030f },
                { "9",     { {0,1.0f}, {2,0.8f}, {4,1.0f}, {7,0.55f}, {10,0.95f} },        -0.025f },
                { "m9",    { {0,1.0f}, {2,0.8f}, {3,1.0f}, {7,0.55f}, {10,0.95f} },        -0.030f },
                { "maj9",  { {0,1.0f}, {2,0.8f}, {4,1.0f}, {7,0.55f}, {11,0.95f} },        -0.030f },
                { "m11",   { {0,1.0f}, {3,1.0f}, {5,0.8f}, {7,0.55f}, {10,0.95f} },        -0.035f }
            };
            return t;
        }

        struct Peak { double midi; double amp; };
    }

    std::string pitchClassName (int pc)
    {
        return kNames[((pc % 12) + 12) % 12];
    }

    Detector::Detector()
    {
        const int n = kFFTSize;
        window.resize ((size_t) n);
        cosT.resize ((size_t) n / 2);
        sinT.resize ((size_t) n / 2);
        bitrev.resize ((size_t) n);
        re.resize ((size_t) n);
        im.resize ((size_t) n);
        mag.resize ((size_t) n / 2);
        floorMag.resize ((size_t) n / 2);
        csum.resize ((size_t) n / 2 + 1);

        const double pi = 3.14159265358979323846;
        // fenêtre de Blackman-Harris : très peu de fuites, les notes graves ne noient pas les aiguës
        for (int i = 0; i < n; ++i)
        {
            const double x = 2.0 * pi * i / (n - 1);
            window[(size_t) i] = 0.35875 - 0.48829 * std::cos (x) + 0.14128 * std::cos (2.0 * x) - 0.01168 * std::cos (3.0 * x);
        }
        for (int i = 0; i < n / 2; ++i)
        {
            cosT[(size_t) i] = std::cos (2.0 * pi * i / n);
            sinT[(size_t) i] = -std::sin (2.0 * pi * i / n);
        }

        int bits = 0;
        while ((1 << bits) < n)
            ++bits;
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < bits; ++b)
                if (i & (1 << b))
                    r |= 1 << (bits - 1 - b);
            bitrev[(size_t) i] = r;
        }
    }

    void Detector::setSampleRate (double sr)
    {
        sampleRate = sr > 1000.0 ? sr : 44100.0;
    }

    void Detector::reset()
    {
        smooth.fill (0.0f);
        acc.fill (0.0);
        tuneCents = 0.0;
        tuneKnown = false;
    }

    void Detector::fft (std::vector<double>& a, std::vector<double>& b) const
    {
        const int n = kFFTSize;
        for (int i = 0; i < n; ++i)
        {
            const int j = bitrev[(size_t) i];
            if (j > i)
            {
                std::swap (a[(size_t) i], a[(size_t) j]);
                std::swap (b[(size_t) i], b[(size_t) j]);
            }
        }
        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len / 2, step = n / len;
            for (int i = 0; i < n; i += len)
                for (int k = 0; k < half; ++k)
                {
                    const double wr = cosT[(size_t) (k * step)], wi = sinT[(size_t) (k * step)];
                    const size_t p = (size_t) (i + k), q = (size_t) (i + k + half);
                    const double tr = a[q] * wr - b[q] * wi;
                    const double ti = a[q] * wi + b[q] * wr;
                    a[q] = a[p] - tr;
                    b[q] = b[p] - ti;
                    a[p] += tr;
                    b[p] += ti;
                }
        }
    }

    Result Detector::analyse (const float* x)
    {
        const int n = kFFTSize;
        Result out;

        // niveau du signal : en dessous, on considère que c'est le silence
        double energy = 0.0;
        for (int i = 0; i < n; ++i)
            energy += (double) x[i] * (double) x[i];
        const double rms = std::sqrt (energy / n);

        if (rms < 0.002)
        {
            for (auto& v : smooth)
                v *= 0.5f;
            for (auto& v : acc)
                v *= 0.5;
            out.chroma = smooth;
            return out;
        }

        // ---- spectre
        double mean = 0.0;
        for (int i = 0; i < n; ++i)
            mean += (double) x[i];
        mean /= n;
        for (int i = 0; i < n; ++i)
        {
            re[(size_t) i] = ((double) x[i] - mean) * window[(size_t) i];
            im[(size_t) i] = 0.0;
        }
        fft (re, im);

        const int half = n / 2;
        for (int k = 0; k < half; ++k)
            mag[(size_t) k] = std::sqrt (re[(size_t) k] * re[(size_t) k] + im[(size_t) k] * im[(size_t) k]);

        const double binHz = sampleRate / n;
        const int kMin = std::max (3, (int) std::ceil (28.0 / binHz));
        const int kMax = std::min (half - 3, (int) std::floor (5200.0 / binHz));

        // plancher local (moyenne glissante) : ne garde que les vrais pics, pas le bruit ni la batterie
        csum[0] = 0.0;
        for (int k = 0; k < half; ++k)
            csum[(size_t) k + 1] = csum[(size_t) k] + mag[(size_t) k];
        const int W = 40;
        double maxMag = 0.0;
        for (int k = kMin; k <= kMax; ++k)
        {
            const int a = std::max (0, k - W), b = std::min (half, k + W + 1);
            floorMag[(size_t) k] = (csum[(size_t) b] - csum[(size_t) a]) / (double) (b - a);
            maxMag = std::max (maxMag, mag[(size_t) k]);
        }
        if (maxMag <= 0.0)
        {
            out.chroma = smooth;
            return out;
        }

        // ---- pics du spectre, fréquence affinée par interpolation parabolique
        std::vector<Peak> peaks;
        peaks.reserve (400);
        for (int k = kMin; k <= kMax; ++k)
        {
            const double m = mag[(size_t) k];
            if (m < maxMag * 0.008 || m < mag[(size_t) k - 1] || m <= mag[(size_t) k + 1])
                continue;
            if (m < floorMag[(size_t) k] * 3.2)
                continue;

            const double a = std::log (mag[(size_t) k - 1] + 1e-12);
            const double b = std::log (m + 1e-12);
            const double c = std::log (mag[(size_t) k + 1] + 1e-12);
            const double denom = a - 2.0 * b + c;
            const double delta = denom != 0.0 ? 0.5 * (a - c) / denom : 0.0;
            const double freq = (k + std::max (-0.5, std::min (0.5, delta))) * binHz;
            peaks.push_back ({ 69.0 + 12.0 * std::log2 (freq / 440.0), std::sqrt (m / maxMag) });
        }

        // ---- accordage de la source (si le morceau est à 435 Hz ou désaccordé)
        {
            double sx = 0.0, sy = 0.0, wsum = 0.0;
            const double pi2 = 6.283185307179586;
            for (const auto& p : peaks)
                if (p.midi > 43.0 && p.midi < 96.0)
                {
                    const double ph = pi2 * (p.midi - std::floor (p.midi));
                    sx += p.amp * std::cos (ph);
                    sy += p.amp * std::sin (ph);
                    wsum += p.amp;
                }
            if (wsum > 1.0)
            {
                double dev = std::atan2 (sy, sx) / pi2;       // -0.5..0.5 demi-tons
                dev = std::max (-0.5, std::min (0.5, dev));
                const double conc = std::sqrt (sx * sx + sy * sy) / wsum;
                if (conc > 0.45)
                {
                    const double est = dev * 100.0;
                    tuneCents = tuneKnown ? 0.85 * tuneCents + 0.15 * est : est;
                    tuneKnown = true;
                }
            }
        }
        const double shift = tuneCents / 100.0;
        for (auto& p : peaks)
            p.midi -= shift;

        // ---- détection des notes : saillance harmonique, avec soustraction des harmoniques déjà expliquées
        struct Note { int midi; double sal; };
        std::vector<Note> notes;
        {
            static const double hw[10] = { 1.0, 0.85, 0.7, 0.6, 0.5, 0.42, 0.35, 0.3, 0.25, 0.2 };
            std::vector<double> amp (peaks.size());
            for (size_t i = 0; i < peaks.size(); ++i)
                amp[i] = peaks[i].amp;

            double firstSal = 0.0;
            for (int iter = 0; iter < 8; ++iter)
            {
                double bestSal = 0.0;
                int bestN = -1;
                for (int cand = 28; cand <= 84; ++cand)
                {
                    bool dup = false;
                    for (const auto& nt : notes)
                        dup |= (nt.midi == cand);
                    if (dup)
                        continue;
                    double sal = 0.0;
                    int found = 0, expected = 0, foundLow = 0;
                    bool hasFund = false;
                    for (int h = 1; h <= 10; ++h)
                    {
                        const double target = cand + 12.0 * std::log2 ((double) h);
                        if (target > 100.0)
                            break;
                        double a = 0.0;
                        for (size_t i = 0; i < peaks.size(); ++i)
                            if (std::fabs (peaks[i].midi - target) < 0.26)
                                a = std::max (a, amp[i]);
                        if (h <= 8)
                            ++expected;
                        if (a > 0.04)
                        {
                            sal += hw[h - 1] * a;
                            if (h <= 8)
                                ++found;
                            if (h <= 4)
                                ++foundLow;
                            if (h == 1)
                                hasFund = true;
                        }
                    }
                    if (found < 2 || foundLow < 1 || (! hasFund && found < 3))
                        continue;
                    // une note réelle explique tous ses harmoniques ; une octave ou une quinte "fantôme" en manque beaucoup
                    const double frac = (double) found / (double) std::max (1, expected);
                    sal *= 0.35 + 0.65 * frac * frac;
                    if (sal > bestSal)
                    {
                        bestSal = sal;
                        bestN = cand;
                    }
                }

                if (bestN < 0)
                    break;
                if (iter == 0)
                    firstSal = bestSal;
                else if (bestSal < 0.12 * firstSal)
                    break;
                notes.push_back ({ bestN, bestSal });

                // on retire (en partie) l'énergie expliquée par cette note ; un harmonique plus fort que le précédent
                // est "renforcé" par une autre note : on le laisse
                double prevA = 0.0;
                for (int h = 1; h <= 10; ++h)
                {
                    const double target = bestN + 12.0 * std::log2 ((double) h);
                    double hMax = 0.0;
                    for (size_t i = 0; i < peaks.size(); ++i)
                        if (std::fabs (peaks[i].midi - target) < 0.26)
                            hMax = std::max (hMax, peaks[i].amp);
                    const bool boosted = h >= 2 && prevA > 0.0 && hMax > 1.15 * prevA;
                    for (size_t i = 0; i < peaks.size(); ++i)
                        if (std::fabs (peaks[i].midi - target) < 0.26)
                            amp[i] *= boosted ? 0.7 : 0.22;
                    prevA = std::max (prevA * 0.8, hMax);
                }
            }
        }

        if (notes.empty())
        {
            for (auto& v : smooth)
                v *= 0.7f;
            out.chroma = smooth;
            return out;
        }

        // ---- classes de hauteur et basse
        std::array<double, 12> cur {};
        double salMax = 0.0;
        for (const auto& nt : notes)
            salMax = std::max (salMax, nt.sal);
        for (const auto& nt : notes)
            cur[(size_t) (nt.midi % 12)] += std::sqrt (nt.sal / salMax);

        int bassPc = -1, bassMidi = 999;
        for (const auto& nt : notes)
            if (nt.sal >= 0.3 * salMax && nt.midi < bassMidi && nt.midi <= 55)
            {
                bassMidi = nt.midi;
                bassPc = nt.midi % 12;
            }

        // lissage temporel, avec remise à zéro quand l'accord change nettement
        {
            double dot = 0.0, na = 0.0, nb = 0.0;
            for (int i = 0; i < 12; ++i)
            {
                dot += acc[(size_t) i] * cur[(size_t) i];
                na += acc[(size_t) i] * acc[(size_t) i];
                nb += cur[(size_t) i] * cur[(size_t) i];
            }
            const double cosSim = (na > 0.0 && nb > 0.0) ? dot / std::sqrt (na * nb) : 0.0;
            const double keep = cosSim > 0.75 ? 0.45 : 0.0;
            for (int i = 0; i < 12; ++i)
                acc[(size_t) i] = keep * acc[(size_t) i] + (1.0 - keep) * cur[(size_t) i];
        }

        double amax = 0.0;
        for (double v : acc)
            amax = std::max (amax, v);
        if (amax <= 0.0)
        {
            out.chroma = smooth;
            return out;
        }
        std::array<double, 12> ch {};
        for (int i = 0; i < 12; ++i)
            ch[(size_t) i] = std::min (1.0, acc[(size_t) i] / amax);
        for (int i = 0; i < 12; ++i)
            smooth[(size_t) i] = 0.4f * smooth[(size_t) i] + 0.6f * (float) ch[(size_t) i];
        out.chroma = smooth;
        out.bass = bassPc;

        // ---- comparaison avec les modèles d'accords
        double cnorm = 0.0;
        for (double v : ch)
            cnorm += v * v;
        cnorm = std::sqrt (cnorm);
        if (cnorm <= 0.0)
            return out;

        double best = -1.0, second = -1.0;
        int bestRoot = 0;
        const Template* bestT = nullptr;
        std::vector<int> bestPcs;

        for (int root = 0; root < 12; ++root)
            for (const auto& t : templates())
            {
                double dot = 0.0, wn = 0.0;
                int missing = 0;
                for (const auto& tone : t.t)
                {
                    const double v = ch[(size_t) ((root + tone.iv) % 12)];
                    dot += (double) tone.w * v;
                    wn += (double) tone.w * tone.w;
                    if (v < 0.12 && tone.w >= 0.9f)
                        ++missing;     // il manque une note qui définit l'accord
                }
                double outside = 0.0;
                {
                    bool inT[12] = {};
                    for (const auto& tone : t.t)
                        inT[(root + tone.iv) % 12] = true;
                    for (int i = 0; i < 12; ++i)
                        if (! inT[i])
                            outside += ch[(size_t) i];
                }
                double score = dot / (cnorm * std::sqrt (wn));
                score += t.bias - 0.05 * missing - 0.20 * outside;
                if (bassPc >= 0)
                {
                    if (root == bassPc)
                        score += 0.06;
                    else
                    {
                        for (const auto& tone : t.t)
                            if ((root + tone.iv) % 12 == bassPc)
                            {
                                score += 0.015;
                                break;
                            }
                    }
                }
                if (score > best)
                {
                    second = best;
                    best = score;
                    bestRoot = root;
                    bestT = &t;
                }
                else if (score > second)
                    second = score;
            }

        if (bestT == nullptr || best < 0.60)
            return out;

        out.valid = true;
        out.root = bestRoot;
        out.name = pitchClassName (bestRoot) + bestT->suffix;
        for (const auto& tone : bestT->t)
            out.pcs.push_back ((bestRoot + tone.iv) % 12);

        // renversement : la basse n'est pas la fondamentale mais une note de l'accord
        if (bassPc >= 0 && bassPc != bestRoot)
            for (int pc : out.pcs)
                if (pc == bassPc)
                {
                    out.name += "/" + pitchClassName (bassPc);
                    break;
                }

        const double margin = std::max (0.0, best - second);
        out.confidence = (float) std::max (0.0, std::min (1.0, 0.85 * best + std::min (0.15, margin * 2.0)));
        return out;
    }
}
