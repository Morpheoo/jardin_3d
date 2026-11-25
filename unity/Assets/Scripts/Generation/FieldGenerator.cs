using System;
using Jardin.Core;

namespace Jardin.Generation
{
    /// <summary>
    /// Llena los campos continuos de WorldData: altura, humedad y pendiente.
    ///
    /// Orden obligatorio: la pendiente se deriva de la altura, así que la altura
    /// tiene que estar completa antes de calcularla.
    /// </summary>
    public static class FieldGenerator
    {
        public static void Generate(WorldData world, GenerationSettings s, SeedBook seeds)
        {
            GenerateHeight(world, s, seeds.For(SeedBook.Terrain));
            GenerateMoisture(world, s, seeds.For(SeedBook.Moisture));
            GenerateSlope(world, s);
        }

        // ---------- altura ----------

        private static void GenerateHeight(WorldData world, GenerationSettings s, uint seed)
        {
            int side = world.Side;

            for (int y = 0; y < side; y++)
            {
                for (int x = 0; x < side; x++)
                {
                    // Se muestrea en coordenadas normalizadas [0,1] y no en metros.
                    // Así, cambiar worldSize reescala el mundo en vez de mostrar
                    // otra región distinta del ruido: el mismo seed da el mismo
                    // paisaje a cualquier tamaño o resolución.
                    float u = (float)x / world.Resolution;
                    float v = (float)y / world.Resolution;

                    float h = Noise.Fbm01(u, v, seed,
                                          s.octaves, s.frequency, s.lacunarity, s.gain);

                    // El exponente aplana los valles y aísla las cimas.
                    if (Math.Abs(s.heightExponent - 1f) > 0.001f)
                        h = (float)Math.Pow(h, s.heightExponent);

                    // Caída suave hacia los bordes: el mapa es delimitado y esto
                    // evita que el terreno quede cortado en vertical en el límite.
                    h *= EdgeFalloff(u, v);

                    world.Height[world.Index(x, y)] = h;
                }
            }
        }

        /// <summary>
        /// Multiplicador en [0,1] que baja el terreno cerca del borde del mapa.
        /// Usa una distancia de Chebyshev al centro, de modo que la caída sigue
        /// la forma cuadrada del mapa y no un círculo.
        /// </summary>
        private static float EdgeFalloff(float u, float v)
        {
            const float start = 0.82f; // dónde empieza a caer, en [0,1]

            float dx = Math.Abs(u - 0.5f) * 2f;
            float dy = Math.Abs(v - 0.5f) * 2f;
            float d = Math.Max(dx, dy);

            if (d <= start) return 1f;

            float t = (d - start) / (1f - start);
            if (t > 1f) t = 1f;

            // Smoothstep invertido: la transición no tiene esquina visible.
            return 1f - (t * t * (3f - 2f * t));
        }

        // ---------- humedad ----------

        private static void GenerateMoisture(WorldData world, GenerationSettings s, uint seed)
        {
            int side = world.Side;

            for (int y = 0; y < side; y++)
            {
                for (int x = 0; x < side; x++)
                {
                    float u = (float)x / world.Resolution;
                    float v = (float)y / world.Resolution;

                    float m = Noise.Fbm01(u, v, seed,
                                          s.moistureOctaves, s.moistureFrequency,
                                          s.moistureLacunarity, s.moistureGain);

                    // Sesgo suave: lo bajo tiende a ser más húmedo que lo alto.
                    // No es simulación de nada, solo una correlación que hace
                    // que las zonas resultantes se lean como coherentes.
                    float h = world.Height[world.Index(x, y)];
                    m = m * 0.75f + (1f - h) * 0.25f;

                    world.Moisture[world.Index(x, y)] = m < 0f ? 0f : (m > 1f ? 1f : m);
                }
            }
        }

        // ---------- pendiente ----------

        /// <summary>
        /// Pendiente por diferencias centrales sobre el campo de altura.
        ///
        /// dh/dx se aproxima con (h[x+1] - h[x-1]) / (2 * cellSize), en metros por metro.
        /// La magnitud del gradiente es la tangente del ángulo de inclinación,
        /// así que atan de esa magnitud da el ángulo, y dividir entre PI/2 lo
        /// normaliza a [0,1]: 0 es plano, 1 es vertical.
        ///
        /// En los bordes se usan diferencias hacia adelante o hacia atrás.
        /// </summary>
        private static void GenerateSlope(WorldData world, GenerationSettings s)
        {
            int side = world.Side;
            float cell = world.CellSize;
            const float halfPi = 1.57079633f;

            for (int y = 0; y < side; y++)
            {
                for (int x = 0; x < side; x++)
                {
                    int xm = x > 0 ? x - 1 : x;
                    int xp = x < side - 1 ? x + 1 : x;
                    int ym = y > 0 ? y - 1 : y;
                    int yp = y < side - 1 ? y + 1 : y;

                    float dx = (world.HeightMeters(xp, y) - world.HeightMeters(xm, y))
                               / ((xp - xm) * cell);
                    float dy = (world.HeightMeters(x, yp) - world.HeightMeters(x, ym))
                               / ((yp - ym) * cell);

                    float magnitude = (float)Math.Sqrt(dx * dx + dy * dy);
                    float slope = (float)Math.Atan(magnitude) / halfPi;

                    world.Slope[world.Index(x, y)] = slope > 1f ? 1f : slope;
                }
            }
        }
    }
}
