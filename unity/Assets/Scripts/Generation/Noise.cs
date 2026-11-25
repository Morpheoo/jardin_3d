using System;
using Jardin.Core;

namespace Jardin.Generation
{
    /// <summary>
    /// Ruido procedural propio.
    ///
    /// No se usa Mathf.PerlinNoise por tres razones concretas:
    ///   1. No acepta semilla. Habría que desplazar coordenadas, que es un parche.
    ///   2. Devuelve exactamente 0.5 en todas las coordenadas enteras.
    ///   3. Su patrón se repite de forma visible en mapas grandes.
    /// Además, este es el algoritmo que el protocolo promete implementar.
    ///
    /// Se ofrecen dos ruidos base:
    ///   - Value: el más simple de explicar. Interpola valores aleatorios de la rejilla.
    ///   - Perlin: ruido de gradiente. Más costoso, pero visualmente mucho mejor
    ///     para terreno porque no deja el artefacto de bloques del value noise.
    /// Para el terreno se recomienda Perlin.
    /// </summary>
    public static class Noise
    {
        // ---------- utilidades ----------

        /// <summary>Quintic fade de Perlin: 6t^5 - 15t^4 + 10t^3.
        /// Tiene primera y segunda derivada nulas en 0 y 1, lo que elimina
        /// las discontinuidades visibles en las fronteras de la rejilla.</summary>
        private static float Fade(float t) => t * t * t * (t * (t * 6f - 15f) + 10f);

        private static float Lerp(float a, float b, float t) => a + (b - a) * t;

        private static int FloorToInt(float v)
        {
            int i = (int)v;
            return v < i ? i - 1 : i;
        }

        // ---------- value noise ----------

        public static float Value(float x, float y, uint seed)
        {
            int x0 = FloorToInt(x);
            int y0 = FloorToInt(y);
            float fx = x - x0;
            float fy = y - y0;
            float u = Fade(fx);
            float v = Fade(fy);

            float v00 = Hash32.ToSigned(Hash32.Coord(x0,     y0,     seed));
            float v10 = Hash32.ToSigned(Hash32.Coord(x0 + 1, y0,     seed));
            float v01 = Hash32.ToSigned(Hash32.Coord(x0,     y0 + 1, seed));
            float v11 = Hash32.ToSigned(Hash32.Coord(x0 + 1, y0 + 1, seed));

            return Lerp(Lerp(v00, v10, u), Lerp(v01, v11, u), v);
        }

        // ---------- gradient (Perlin) noise ----------

        private const float S = 0.70710678f; // sqrt(2)/2

        // 8 direcciones unitarias: 4 ejes y 4 diagonales.
        private static readonly float[] GradX = {  1f, -1f,  0f,  0f,   S,  -S,   S,  -S };
        private static readonly float[] GradY = {  0f,  0f,  1f, -1f,   S,   S,  -S,  -S };

        private static float DotGrad(int ix, int iy, uint seed, float dx, float dy)
        {
            uint h = Hash32.Coord(ix, iy, seed) & 7u;
            return GradX[h] * dx + GradY[h] * dy;
        }

        /// <summary>Ruido de gradiente 2D. Salida aproximadamente en [-1, 1].</summary>
        public static float Perlin(float x, float y, uint seed)
        {
            int x0 = FloorToInt(x);
            int y0 = FloorToInt(y);
            float fx = x - x0;
            float fy = y - y0;
            float u = Fade(fx);
            float v = Fade(fy);

            float n00 = DotGrad(x0,     y0,     seed, fx,        fy);
            float n10 = DotGrad(x0 + 1, y0,     seed, fx - 1f,   fy);
            float n01 = DotGrad(x0,     y0 + 1, seed, fx,        fy - 1f);
            float n11 = DotGrad(x0 + 1, y0 + 1, seed, fx - 1f,   fy - 1f);

            float r = Lerp(Lerp(n00, n10, u), Lerp(n01, n11, u), v);

            // El rango crudo del Perlin 2D es aproximadamente [-0.707, 0.707].
            // Se reescala para aprovechar todo [-1, 1].
            return r * 1.41421356f;
        }

        // ---------- fBm ----------

        /// <summary>
        /// Movimiento browniano fraccional: suma de octavas de ruido, cada una con
        /// el doble de frecuencia (lacunarity) y la mitad de amplitud (gain) que la anterior.
        ///
        /// Es lo que convierte un ruido suave y aburrido en un relieve con detalle
        /// a varias escalas: colinas grandes, ondulaciones medianas y rugosidad fina.
        ///
        /// Cada octava usa su propia subsemilla para que no se correlacionen entre sí.
        /// El resultado se divide entre la suma de amplitudes para conservar [-1, 1]
        /// sin importar cuántas octavas se usen.
        /// </summary>
        public static float Fbm(float x, float y, uint seed, int octaves,
                                float frequency, float lacunarity, float gain,
                                bool usePerlin = true)
        {
            if (octaves < 1) octaves = 1;

            float sum = 0f;
            float amplitude = 1f;
            float norm = 0f;
            float f = frequency;

            for (int i = 0; i < octaves; i++)
            {
                uint octaveSeed = Hash32.Combine(seed, unchecked((uint)i));
                float n = usePerlin ? Perlin(x * f, y * f, octaveSeed)
                                    : Value(x * f, y * f, octaveSeed);
                sum += n * amplitude;
                norm += amplitude;
                amplitude *= gain;
                f *= lacunarity;
            }

            return norm > 0f ? sum / norm : 0f;
        }

        /// <summary>fBm remapeado a [0, 1]. Es la forma que usan los campos del mundo.</summary>
        public static float Fbm01(float x, float y, uint seed, int octaves,
                                  float frequency, float lacunarity, float gain,
                                  bool usePerlin = true)
        {
            float n = Fbm(x, y, seed, octaves, frequency, lacunarity, gain, usePerlin);
            n = n * 0.5f + 0.5f;
            return n < 0f ? 0f : (n > 1f ? 1f : n);
        }
    }
}
