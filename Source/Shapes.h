#pragma once
#include <cmath>

// 16 formes de courbe. t = 0..1 (début -> fin de la forme). Retour : gain 0 (coupé à fond) .. 1 (plein volume)
namespace KS
{
constexpr int numShapes = 16;

inline float clamp01 (float x) { return x < 0.f ? 0.f : (x > 1.f ? 1.f : x); }
inline float sm (float x)      { x = clamp01 (x); return x * x * (3.f - 2.f * x); }
inline float easeOut (float x, float k) { return 1.f - std::pow (1.f - clamp01 (x), k); }

inline float sig (float t, float c, float k)
{
    auto f = [=] (float x) { return 1.f / (1.f + std::exp (-k * (x - c))); };
    return (f (t) - f (0.f)) / (f (1.f) - f (0.f));
}

inline float shapeValue (int idx, float t)
{
    t = clamp01 (t);
    const float pi = 3.14159265f;
    switch (idx)
    {
        case 0:  return easeOut (t / 0.75f, 3.f);
        case 1:  return t < 0.06f ? 0.f : easeOut ((t - 0.06f) / 0.7f, 3.f);
        case 2:  return sig (t, 0.40f, 14.f);
        case 3:  { const float x = clamp01 (t / 0.8f); return x * x; }
        case 4:  return sig (t, 0.28f, 16.f) * (1.f - sm ((t - 0.8f) / 0.15f));
        case 5:  return 1.f - sm ((t - 0.12f) / 0.03f);
        case 6:  return 1.f - sm (t / 0.25f) * (1.f - sm ((t - 0.75f) / 0.2f));
        case 7:  return t < 0.7f ? std::pow (t / 0.7f, 1.5f) : 1.f - sm ((t - 0.7f) / 0.3f);
        case 8:  return easeOut (t / 0.9f, 2.f);
        case 9:  return t < 0.08f ? 0.f : easeOut ((t - 0.08f) / 0.8f, 2.f);
        case 10: return sm (t / 0.6f) * (1.f - sm ((t - 0.95f) / 0.03f));
        case 11: return clamp01 (t / 0.2f) * (1.f - sm ((t - 0.96f) / 0.02f));
        case 12: return clamp01 ((t - 0.1f) / 0.25f) * (1.f - clamp01 ((t - 0.78f) / 0.15f));
        case 13: return 1.f - clamp01 (t / 0.8f);
        case 14: return 1.f - std::pow (std::sin (pi * t), 1.3f);
        case 15:
        {
            const float hill = t < 0.45f ? 0.35f * std::pow (std::sin (pi * t / 0.45f), 2.f) : 0.f;
            return clamp01 (hill + sm ((t - 0.45f) / 0.5f));
        }
        default: return 1.f;
    }
}
} // namespace KS
