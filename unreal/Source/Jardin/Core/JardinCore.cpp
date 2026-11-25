// JardinCore.cpp — Implementacion del ruido.

#include "JardinCore.h"

namespace jardin
{
namespace noise
{

float Value(float x, float y, uint32_t seed)
{
    const int32_t x0 = FloorToInt(x);
    const int32_t y0 = FloorToInt(y);
    const float fx = x - static_cast<float>(x0);
    const float fy = y - static_cast<float>(y0);
    const float u = Fade(fx);
    const float v = Fade(fy);

    const float v00 = hash32::ToSigned(hash32::Coord(x0,     y0,     seed));
    const float v10 = hash32::ToSigned(hash32::Coord(x0 + 1, y0,     seed));
    const float v01 = hash32::ToSigned(hash32::Coord(x0,     y0 + 1, seed));
    const float v11 = hash32::ToSigned(hash32::Coord(x0 + 1, y0 + 1, seed));

    return Lerp(Lerp(v00, v10, u), Lerp(v01, v11, u), v);
}

namespace
{
    constexpr float S = 0.70710678f; // sqrt(2)/2

    // 8 direcciones unitarias: 4 ejes y 4 diagonales.
    constexpr float GradX[8] = { 1.0f, -1.0f,  0.0f,  0.0f,    S,   -S,    S,   -S };
    constexpr float GradY[8] = { 0.0f,  0.0f,  1.0f, -1.0f,    S,    S,   -S,   -S };

    inline float DotGrad(int32_t ix, int32_t iy, uint32_t seed, float dx, float dy)
    {
        const uint32_t h = hash32::Coord(ix, iy, seed) & 7u;
        return GradX[h] * dx + GradY[h] * dy;
    }
}

float Perlin(float x, float y, uint32_t seed)
{
    const int32_t x0 = FloorToInt(x);
    const int32_t y0 = FloorToInt(y);
    const float fx = x - static_cast<float>(x0);
    const float fy = y - static_cast<float>(y0);
    const float u = Fade(fx);
    const float v = Fade(fy);

    const float n00 = DotGrad(x0,     y0,     seed, fx,          fy);
    const float n10 = DotGrad(x0 + 1, y0,     seed, fx - 1.0f,   fy);
    const float n01 = DotGrad(x0,     y0 + 1, seed, fx,          fy - 1.0f);
    const float n11 = DotGrad(x0 + 1, y0 + 1, seed, fx - 1.0f,   fy - 1.0f);

    const float r = Lerp(Lerp(n00, n10, u), Lerp(n01, n11, u), v);

    // El rango crudo del Perlin 2D es aproximadamente [-0.707, 0.707].
    // Se reescala para aprovechar todo [-1, 1].
    return r * 1.41421356f;
}

float Fbm(float x, float y, uint32_t seed, int32_t octaves,
          float frequency, float lacunarity, float gain, bool usePerlin)
{
    if (octaves < 1) octaves = 1;

    float sum = 0.0f;
    float amplitude = 1.0f;
    float norm = 0.0f;
    float f = frequency;

    for (int32_t i = 0; i < octaves; ++i)
    {
        const uint32_t octaveSeed = hash32::Combine(seed, static_cast<uint32_t>(i));
        const float n = usePerlin ? Perlin(x * f, y * f, octaveSeed)
                                  : Value(x * f, y * f, octaveSeed);
        sum += n * amplitude;
        norm += amplitude;
        amplitude *= gain;
        f *= lacunarity;
    }

    return norm > 0.0f ? sum / norm : 0.0f;
}

float Fbm01(float x, float y, uint32_t seed, int32_t octaves,
            float frequency, float lacunarity, float gain, bool usePerlin)
{
    float n = Fbm(x, y, seed, octaves, frequency, lacunarity, gain, usePerlin);
    n = n * 0.5f + 0.5f;
    return n < 0.0f ? 0.0f : (n > 1.0f ? 1.0f : n);
}

float RidgedFbm01(float x, float y, uint32_t seed, int32_t octaves,
                  float frequency, float lacunarity, float gain)
{
    if (octaves < 1) octaves = 1;

    float sum = 0.0f;
    float amplitude = 1.0f;
    float norm = 0.0f;
    float f = frequency;
    float weight = 1.0f;

    for (int32_t i = 0; i < octaves; ++i)
    {
        const uint32_t octaveSeed = hash32::Combine(seed, static_cast<uint32_t>(i));
        float r = 1.0f - std::fabs(Perlin(x * f, y * f, octaveSeed));
        r *= r;          // crestas afiladas
        r *= weight;     // el detalle solo sobre lo que ya es alto

        weight = r * 2.0f;
        if (weight > 1.0f) weight = 1.0f;
        if (weight < 0.0f) weight = 0.0f;

        sum += r * amplitude;
        norm += amplitude;
        amplitude *= gain;
        f *= lacunarity;
    }

    const float n = norm > 0.0f ? sum / norm : 0.0f;
    return n < 0.0f ? 0.0f : (n > 1.0f ? 1.0f : n);
}

} // namespace noise
} // namespace jardin
