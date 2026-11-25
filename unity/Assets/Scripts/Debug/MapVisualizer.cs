using UnityEngine;
using Jardin.Generation;

namespace Jardin.Debugging
{
    /// <summary>
    /// COSTURA 3.7 — Visualizador genérico de campos.
    ///
    /// UNA sola función convierte cualquier float[] de WorldData en una textura.
    /// Agregar el mapa de humedad, pendiente, densidad o la máscara del camino
    /// de la Fase 4 es una línea de código, no media tarde.
    ///
    /// La alternativa —escribir un visualizador específico para el mapa de alturas—
    /// parece más rápida hoy y cuesta una tarde por cada mapa nuevo después.
    ///
    /// Esto es lo que demuestra ante la profesora que el mundo no es decoración:
    /// se ve el dato numérico que produjo cada decisión visual.
    /// </summary>
    public static class MapVisualizer
    {
        /// <summary>Convierte un campo continuo en [0,1] a una textura en escala de grises.</summary>
        public static Texture2D FromField(WorldData world, float[] field, Gradient gradient = null)
        {
            int side = world.Side;
            var tex = new Texture2D(side, side, TextureFormat.RGBA32, false)
            {
                filterMode = FilterMode.Point,   // sin suavizado: queremos ver el dato, no una foto
                wrapMode = TextureWrapMode.Clamp
            };

            var pixels = new Color32[side * side];

            for (int y = 0; y < side; y++)
            {
                for (int x = 0; x < side; x++)
                {
                    int i = world.Index(x, y);
                    float v = Mathf.Clamp01(field[i]);

                    Color c = gradient != null ? gradient.Evaluate(v) : new Color(v, v, v, 1f);

                    // La textura se escribe de abajo hacia arriba respecto de la rejilla,
                    // para que el norte del mapa quede arriba en la imagen.
                    pixels[y * side + x] = c;
                }
            }

            tex.SetPixels32(pixels);
            tex.Apply();
            return tex;
        }

        /// <summary>Mapa de zonas: un color plano por zona.</summary>
        public static Texture2D FromZones(WorldData world, Color[] zoneColors)
        {
            int side = world.Side;
            var tex = new Texture2D(side, side, TextureFormat.RGBA32, false)
            {
                filterMode = FilterMode.Point,
                wrapMode = TextureWrapMode.Clamp
            };

            var pixels = new Color32[side * side];

            for (int i = 0; i < pixels.Length; i++)
            {
                byte z = world.Zone[i];
                Color c = (zoneColors != null && z < zoneColors.Length)
                        ? zoneColors[z]
                        : Color.magenta;   // magenta = id de zona sin color asignado

                pixels[i] = c;
            }

            tex.SetPixels32(pixels);
            tex.Apply();
            return tex;
        }

        /// <summary>
        /// Mapa de zonas modulado por ZoneBlend: las fronteras difusas se ven oscuras.
        /// En la Fase 0 es solo diagnóstico; en la Fase 6 es exactamente el dato
        /// que se usará para interpolar color y densidad en los bordes.
        /// </summary>
        public static Texture2D FromZoneBlend(WorldData world, Color[] zoneColors)
        {
            int side = world.Side;
            var tex = new Texture2D(side, side, TextureFormat.RGBA32, false)
            {
                filterMode = FilterMode.Point,
                wrapMode = TextureWrapMode.Clamp
            };

            var pixels = new Color32[side * side];

            for (int i = 0; i < pixels.Length; i++)
            {
                byte z = world.Zone[i];
                Color c = (zoneColors != null && z < zoneColors.Length) ? zoneColors[z] : Color.magenta;
                pixels[i] = c * Mathf.Lerp(0.25f, 1f, world.ZoneBlend[i]);
            }

            tex.SetPixels32(pixels);
            tex.Apply();
            return tex;
        }

        /// <summary>Gradiente de terreno para el mapa de alturas: agua, pasto, roca, nieve.</summary>
        public static Gradient TerrainGradient()
        {
            var g = new Gradient();
            g.SetKeys(
                new[]
                {
                    new GradientColorKey(new Color(0.12f, 0.20f, 0.35f), 0.00f),
                    new GradientColorKey(new Color(0.30f, 0.45f, 0.25f), 0.25f),
                    new GradientColorKey(new Color(0.45f, 0.55f, 0.30f), 0.55f),
                    new GradientColorKey(new Color(0.50f, 0.45f, 0.40f), 0.80f),
                    new GradientColorKey(new Color(0.92f, 0.92f, 0.95f), 1.00f),
                },
                new[] { new GradientAlphaKey(1f, 0f), new GradientAlphaKey(1f, 1f) });
            return g;
        }

        public static Color[] DefaultZoneColors()
        {
            return new[]
            {
                new Color(0.55f, 0.72f, 0.35f),   // Pradera
                new Color(0.16f, 0.38f, 0.22f),   // Bosque
                // Fase 6: new Color(0.45f, 0.42f, 0.38f),  // Rocosa-húmeda
            };
        }
    }
}
