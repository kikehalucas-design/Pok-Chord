#include "ChordAnalyser.h"
#include <algorithm>
#include <cmath>

namespace analyse
{
    namespace
    {
        const char* const kNames[12] = { "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };

        struct Template { const char* suffix; std::vector<int> iv; float bias; };

        const std::vector<Template>& templates()
        {
            static const std::vector<Template> t = {
                { "",      { 0, 4, 7 },          0.040f },
                { "m",     { 0, 3, 7 },          0.040f },
                { "7",     { 0, 4, 7, 10 },      0.000f },
                { "maj7",  { 0, 4, 7, 11 },      0.000f },
                { "m7",    { 0, 3, 7, 10 },      0.000f },
                { "dim",   { 0, 3, 6 },         -0.010f },
                { "aug",   { 0, 4, 8 },         -0.030f },
                { "sus2",  { 0, 2, 7 },         -0.020f },
                { "sus4",  { 0, 5, 7 },         -0.020f },
                { "m7b5",  { 0, 3, 6, 10 },     -0.010f },
                { "dim7",  { 0, 3, 6, 9 },      -0.030f },
                { "6",     { 0, 4, 7, 9 },      -0.020f },
                { "m6",    { 0, 3, 7, 9 },      -0.030f },
                { "9",     { 0, 2, 4, 7, 10 },  -0.020f },
                { "m9",    { 0, 2, 3, 7, 10 },  -0.025f },
                { "maj9",  { 0, 2, 4, 7, 11 },  -0.025f }
            };
            return t;
        }
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

        const double pi = 3.14159265358979323846;
        for (int i = 0; i < n; ++i)
            window[(size_t) i] = 0.5 - 0.5 * std::cos (2.0 * pi * i / (n - 1));
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
            out.chroma = smooth;
            return out;
        }

        for (int i = 0; i < n; ++i)
        {
            re[(size_t) i] = (double) x[i] * window[(size_t) i];
            im[(size_t) i] = 0.0;
        }
        fft (re, im);

        const int half = n / 2;
        double maxMag = 0.0;
        for (int k = 0; k < half; ++k)
        {
            mag[(size_t) k] = std::sqrt (re[(size_t) k] * re[(size_t) k] + im[(size_t) k] * im[(size_t) k]);
        }

        const double binHz = sampleRate / n;
        const int kMin = std::max (2, (int) std::ceil (62.0 / binHz));
        const int kMax = std::min (half - 2, (int) std::floor (2400.0 / binHz));

        for (int k = kMin; k <= kMax; ++k)
            maxMag = std::max (maxMag, mag[(size_t) k]);
        if (maxMag <= 0.0)
        {
            out.chroma = smooth;
            return out;
        }

        std::array<double, 12> chroma {};
        std::array<double, 12> bass {};

        for (int k = kMin; k <= kMax; ++k)
        {
            const double m = mag[(size_t) k];
            if (m < maxMag * 0.03 || m < mag[(size_t) k - 1] || m <= mag[(size_t) k + 1])
                continue;

            // interpolation parabolique pour la fréquence exacte du pic
            const double a = std::log (mag[(size_t) k - 1] + 1e-12);
            const double b = std::log (m + 1e-12);
            const double c = std::log (mag[(size_t) k + 1] + 1e-12);
            const double denom = a - 2.0 * b + c;
            const double delta = denom != 0.0 ? 0.5 * (a - c) / denom : 0.0;
            const double freq = (k + std::max (-0.5, std::min (0.5, delta))) * binHz;

            const double midi = 69.0 + 12.0 * std::log2 (freq / 440.0);
            const double nearest = std::round (midi);
            const double dev = std::fabs (midi - nearest);
            if (dev > 0.38)
                continue;

            const int pc = (((int) nearest) % 12 + 12) % 12;
            const double w = std::sqrt (m / maxMag) * (1.0 - dev * 1.2);
            chroma[(size_t) pc] += w;
            if (freq < 200.0)
                bass[(size_t) pc] += m / maxMag;
        }

        // normalisation puis lissage dans le temps
        double cmax = 0.0;
        for (double v : chroma)
            cmax = std::max (cmax, v);
        if (cmax <= 0.0)
        {
            out.chroma = smooth;
            return out;
        }
        for (int i = 0; i < 12; ++i)
            smooth[(size_t) i] = 0.5f * smooth[(size_t) i] + 0.5f * (float) (chroma[(size_t) i] / cmax);

        out.chroma = smooth;

        // basse la plus forte
        int bassPc = -1;
        double bassMax = 0.0;
        for (int i = 0; i < 12; ++i)
            if (bass[(size_t) i] > bassMax)
            {
                bassMax = bass[(size_t) i];
                bassPc = i;
            }
        if (bassMax < 0.25)
            bassPc = -1;

        // comparaison avec les modèles d'accords (similarité cosinus)
        double cnorm = 0.0;
        for (float v : smooth)
            cnorm += (double) v * v;
        cnorm = std::sqrt (cnorm);
        if (cnorm <= 0.0)
            return out;

        double best = -1.0;
        int bestRoot = 0;
        const Template* bestT = nullptr;

        for (int root = 0; root < 12; ++root)
            for (const auto& t : templates())
            {
                double dot = 0.0;
                for (int iv : t.iv)
                    dot += smooth[(size_t) ((root + iv) % 12)];
                double score = dot / (cnorm * std::sqrt ((double) t.iv.size()));
                score += t.bias;
                if (root == bassPc)
                    score += 0.07;
                if (score > best)
                {
                    best = score;
                    bestRoot = root;
                    bestT = &t;
                }
            }

        if (bestT == nullptr || best < 0.62)
            return out;

        out.valid = true;
        out.root = bestRoot;
        out.name = pitchClassName (bestRoot) + bestT->suffix;
        for (int iv : bestT->iv)
            out.pcs.push_back ((bestRoot + iv) % 12);
        out.confidence = (float) std::max (0.0, std::min (1.0, best));
        return out;
    }
}
