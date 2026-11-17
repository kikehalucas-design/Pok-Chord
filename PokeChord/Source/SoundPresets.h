#pragma once

// Sons d'écoute : petits presets de synthé inspirés des sons classiques de la house.
// C++ pur, sans JUCE. Ce sont des approximations simples, pas des copies d'instruments.

#include <algorithm>
#include <array>
#include <cmath>

namespace sounds
{
    enum Wave { Saw, SawPulse, TriSine, Organ, EPiano };

    struct Preset
    {
        const char* name;
        Wave wave;
        int nOsc;
        float det[3];     // désaccord de chaque oscillateur
        float atk;        // attaque (s)
        float rel;        // relâchement (s)
        float decTime;    // durée de la décroissance vers le niveau de maintien (s), 0 = aucune
        float sus;        // niveau de maintien (0..1)
        float cut;        // fréquence du filtre passe-bas (Hz)
        float fenv;       // ouverture supplémentaire du filtre à l'attaque (x cut)
        float gain;
    };

    constexpr int kCount = 9;

    inline const std::array<Preset, kCount>& presets()
    {
        static const std::array<Preset, kCount> p = { {
            { "Piano house", SawPulse, 2, { 0.998f, 1.002f, 1.0f },  0.003f, 0.18f, 0.45f, 0.38f, 3200.0f, 0.9f, 1.70f },
            { "Rhodes",      EPiano,   1, { 1.0f,   1.0f,   1.0f },  0.004f, 0.35f, 0.90f, 0.45f, 6000.0f, 0.0f, 0.66f },
            { "Orgue house", Organ,    2, { 1.0f,   1.003f, 1.0f },  0.006f, 0.07f, 0.00f, 1.00f, 6000.0f, 0.0f, 0.90f },
            { "Stab",        SawPulse, 3, { 0.994f, 1.0f,   1.006f }, 0.003f, 0.14f, 0.22f, 0.22f, 2400.0f, 1.2f, 2.00f },
            { "Supersaw",    Saw,      3, { 0.988f, 1.0f,   1.012f }, 0.220f, 0.70f, 0.00f, 1.00f, 2300.0f, 0.0f, 1.08f },
            { "Pad",         TriSine,  2, { 0.998f, 1.002f, 1.0f },  0.450f, 0.90f, 0.00f, 1.00f, 1300.0f, 0.0f, 1.08f },
            { "Pluck",       Saw,      2, { 0.998f, 1.002f, 1.0f },  0.002f, 0.12f, 0.28f, 0.00f, 2600.0f, 2.0f, 0.78f },
            { "Cordes",      Saw,      3, { 0.996f, 1.0f,   1.004f }, 0.180f, 0.40f, 0.00f, 1.00f, 2200.0f, 0.0f, 0.95f },
            { "Synth",       Saw,      3, { 0.997f, 1.0f,   1.003f }, 0.008f, 0.28f, 0.00f, 1.00f, 2800.0f, 0.0f, 0.70f }
        } };
        return p;
    }

    // Coefficients dépendant de la fréquence d'échantillonnage
    struct Runtime
    {
        float atkC = 0.0f, relC = 0.0f, decC = 1.0f;
    };

    inline Runtime makeRuntime (const Preset& p, double sr)
    {
        Runtime r;
        r.atkC = 1.0f - std::exp (-1.0f / (std::max (0.0005f, p.atk) * (float) sr));
        r.relC = 1.0f - std::exp (-1.0f / (std::max (0.005f, p.rel) * (float) sr));
        r.decC = p.decTime > 0.0f ? std::exp (-1.0f / (p.decTime * (float) sr)) : 1.0f;
        return r;
    }

    // sinus par table (rapide), phase en cycles
    inline float sinCyc (float x)
    {
        static const std::array<float, 2050> table = [] {
            std::array<float, 2050> t {};
            for (size_t i = 0; i < t.size(); ++i)
                t[i] = (float) std::sin (6.28318530717958647692 * (double) i / 2048.0);
            return t;
        }();

        x -= std::floor (x);
        const float pos = x * 2048.0f;
        const int i = (int) pos;
        const float f = pos - (float) i;
        return table[(size_t) i] + (table[(size_t) i + 1] - table[(size_t) i]) * f;
    }

    // Un échantillon d'une voix. ph : phases, env : enveloppe, dec : décroissance, lp : mémoire du filtre.
    inline float tick (const Preset& p, const Runtime& rt, bool gate, float inc, float* ph,
                       float& env, float& dec, float& lp, double sr)
    {
        env += ((gate ? 1.0f : 0.0f) - env) * (gate ? rt.atkC : rt.relC);
        if (p.decTime > 0.0f)
            dec = p.sus + (dec - p.sus) * rt.decC;

        float s = 0.0f;
        for (int k = 0; k < p.nOsc; ++k)
        {
            ph[k] += inc * p.det[k];
            if (ph[k] >= 1.0f)
                ph[k] -= 1.0f;

            const float x = ph[k];
            switch (p.wave)
            {
                case Saw:
                    s += 2.0f * x - 1.0f;
                    break;
                case SawPulse:
                    s += (2.0f * x - 1.0f) * 0.6f + (x < 0.5f ? 0.45f : -0.45f);
                    break;
                case TriSine:
                    s += 0.7f * (4.0f * std::fabs (x - 0.5f) - 1.0f) + 0.3f * sinCyc (x);
                    break;
                case Organ:
                    s += (sinCyc (x) + 0.8f * sinCyc (2.0f * x) + 0.55f * sinCyc (3.0f * x)
                          + 0.35f * sinCyc (4.0f * x) + 0.2f * sinCyc (6.0f * x)) * (1.0f / 2.9f);
                    break;
                case EPiano:
                {
                    const float index = 0.25f + 1.6f * dec;   // le timbre s'adoucit en retombant
                    s += sinCyc (x + index * sinCyc (x)) + 0.12f * sinCyc (2.0f * x);
                    break;
                }
            }
        }
        s *= p.gain / (float) p.nOsc;

        const float fc = p.cut * (1.0f + p.fenv * dec);
        const float c = std::min (0.95f, 6.2831853f * fc / (float) sr);
        lp += c * (s - lp);

        return lp * env * (p.decTime > 0.0f ? dec : 1.0f);
    }
}
