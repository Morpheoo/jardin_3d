using System;

namespace Jardin.Generation
{
    /// <summary>
    /// COSTURA 3.3 — Clasificación por puntaje, no por if.
    ///
    /// La tentación en la Fase 0, con solo dos zonas, es escribir:
    ///     if (altura > 0.5f) zona = Bosque; else zona = Pradera;
    ///
    /// Eso funciona hoy y es una trampa. Agregar una tercera zona con fronteras
    /// suaves obligaría a reescribir el clasificador entero y, con él, toda la
    /// colocación de objetos que dependa de la zona.
    ///
    /// En su lugar, cada zona calcula un puntaje y gana el más alto. Agregar la
    /// zona rocosa-húmeda de la Fase 6 es agregar una entrada a este arreglo.
    /// </summary>
    [Serializable]
    public class ZoneRule
    {
        public string name = "Zona";
        public byte id = 0;

        /// <summary>Puntaje base. Sube o baja la presencia global de la zona.</summary>
        public float bias = 0f;

        // Cada campo aporta mediante una "banda de preferencia": la zona puntúa
        // alto cuando el valor está cerca de su centro y cae linealmente hasta
        // cero a una distancia igual a su ancho.
        //
        // Se usa esto y no umbrales porque una banda expresa "a esta zona le gusta
        // la humedad media-alta" en lugar de "humedad mayor que 0.6", que es lo
        // que produce fronteras rectas y artificiales.

        public float heightCenter = 0.5f;
        public float heightWidth = 1f;
        public float heightWeight = 1f;

        public float moistureCenter = 0.5f;
        public float moistureWidth = 1f;
        public float moistureWeight = 1f;

        public float slopeCenter = 0f;
        public float slopeWidth = 1f;
        public float slopeWeight = 1f;

        /// <summary>Función tienda: 1 en el centro, 0 a una distancia igual al ancho.</summary>
        private static float Band(float value, float center, float width)
        {
            if (width <= 0.0001f) return 0f;
            float d = Math.Abs(value - center) / width;
            float r = 1f - d;
            return r < 0f ? 0f : (r > 1f ? 1f : r);
        }

        public float Score(float height, float moisture, float slope)
        {
            return bias
                 + heightWeight   * Band(height,   heightCenter,   heightWidth)
                 + moistureWeight * Band(moisture, moistureCenter, moistureWidth)
                 + slopeWeight    * Band(slope,    slopeCenter,    slopeWidth);
        }
    }

    /// <summary>Conjunto de reglas de zona más los parámetros de mezcla.</summary>
    [Serializable]
    public class ZoneSet
    {
        public const byte Pradera = 0;
        public const byte Bosque  = 1;
        // Fase 6: public const byte Rocosa = 2;

        public ZoneRule[] rules;

        /// <summary>
        /// Margen de puntaje a partir del cual una zona se considera "decidida".
        /// Márgenes menores se guardan como valores intermedios en ZoneBlend,
        /// que es lo que la Fase 6 usará para difuminar las fronteras.
        /// </summary>
        public float blendScale = 0.25f;

        /// <summary>Las dos zonas comprometidas en el protocolo.</summary>
        public static ZoneSet Default()
        {
            return new ZoneSet
            {
                blendScale = 0.25f,
                rules = new[]
                {
                    new ZoneRule
                    {
                        name = "Pradera", id = Pradera, bias = 0.05f,
                        heightCenter = 0.35f, heightWidth = 0.65f, heightWeight = 0.8f,
                        moistureCenter = 0.30f, moistureWidth = 0.55f, moistureWeight = 1.2f,
                        slopeCenter = 0.00f, slopeWidth = 0.50f, slopeWeight = 0.6f,
                    },
                    new ZoneRule
                    {
                        name = "Bosque", id = Bosque, bias = 0f,
                        heightCenter = 0.55f, heightWidth = 0.65f, heightWeight = 0.8f,
                        moistureCenter = 0.75f, moistureWidth = 0.55f, moistureWeight = 1.2f,
                        slopeCenter = 0.15f, slopeWidth = 0.60f, slopeWeight = 0.6f,
                    },
                }
            };
        }

        public string NameOf(byte id)
        {
            if (rules != null)
                for (int i = 0; i < rules.Length; i++)
                    if (rules[i].id == id) return rules[i].name;
            return "?";
        }
    }

    public static class ZoneClassifier
    {
        /// <summary>
        /// Llena Zone y ZoneBlend de WorldData.
        /// Requiere que Height, Moisture y Slope ya estén generados.
        /// </summary>
        public static void Classify(WorldData world, ZoneSet zones)
        {
            if (zones == null || zones.rules == null || zones.rules.Length == 0)
                throw new ArgumentException("ZoneSet vacío: no hay reglas que evaluar.");

            var rules = zones.rules;
            float blendScale = zones.blendScale > 0.0001f ? zones.blendScale : 0.25f;
            int count = world.Height.Length;

            for (int i = 0; i < count; i++)
            {
                float h = world.Height[i];
                float m = world.Moisture[i];
                float s = world.Slope[i];

                float bestScore = float.NegativeInfinity;
                float secondScore = float.NegativeInfinity;
                byte bestId = rules[0].id;

                for (int r = 0; r < rules.Length; r++)
                {
                    float score = rules[r].Score(h, m, s);

                    if (score > bestScore)
                    {
                        secondScore = bestScore;
                        bestScore = score;
                        bestId = rules[r].id;
                    }
                    else if (score > secondScore)
                    {
                        secondScore = score;
                    }
                }

                world.Zone[i] = bestId;

                // Con una sola regla no hay segundo lugar: la zona es total.
                float margin = float.IsNegativeInfinity(secondScore)
                             ? 1f
                             : (bestScore - secondScore) / blendScale;

                world.ZoneBlend[i] = margin < 0f ? 0f : (margin > 1f ? 1f : margin);
            }
        }
    }
}
