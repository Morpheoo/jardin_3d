using System;

namespace Jardin.Generation
{
    /// <summary>
    /// Todos los parámetros de generación en un solo objeto.
    ///
    /// Es [Serializable] y sin dependencias de UnityEngine, de modo que Unity lo
    /// muestra en el inspector pero el generador sigue siendo C# puro y probable
    /// fuera del motor.
    ///
    /// Regla: si un valor influye en el mundo, vive aquí. Nada de constantes
    /// escondidas dentro de los generadores, porque entonces el conjunto
    /// "semilla + settings" dejaría de describir el mundo por completo.
    /// </summary>
    [Serializable]
    public class GenerationSettings
    {
        // ---------- semilla ----------

        public int seed = 827461;

        // ---------- dimensiones ----------

        /// <summary>Lado del mapa en metros. El protocolo compromete ~128.</summary>
        public float worldSize = 128f;

        /// <summary>Celdas por lado. Las muestras por lado son resolution + 1.
        /// 192 da celdas de ~0.67 m en un mapa de 128 m: suficiente detalle
        /// sin llegar al límite de vértices de una malla de 16 bits.</summary>
        public int resolution = 192;

        // ---------- campo de altura ----------

        public float maxHeight = 14f;

        /// <summary>Ciclos de la octava base a lo ancho del mundo.
        /// Valores bajos dan colinas amplias; altos dan terreno ruidoso.</summary>
        public float frequency = 1.6f;

        public int octaves = 5;
        public float lacunarity = 2f;
        public float gain = 0.5f;

        /// <summary>Exponente aplicado a la altura normalizada.
        /// Por encima de 1 aplana los valles y deja las cimas más aisladas,
        /// que es lo que da aspecto de valle o parque en vez de ruido uniforme.</summary>
        public float heightExponent = 1.35f;

        // ---------- campo de humedad ----------

        public float moistureFrequency = 1.1f;
        public int moistureOctaves = 3;
        public float moistureLacunarity = 2f;
        public float moistureGain = 0.5f;

        // ---------- validación ----------

        /// <summary>
        /// Acota los valores a rangos en los que el generador se comporta.
        /// Se llama siempre antes de generar: evita que un valor tecleado por error
        /// en el inspector produzca una malla de 4 millones de vértices o una
        /// división entre cero.
        /// </summary>
        public void Validate()
        {
            if (worldSize < 16f) worldSize = 16f;
            if (worldSize > 1024f) worldSize = 1024f;

            if (resolution < 8) resolution = 8;
            if (resolution > 512) resolution = 512;

            if (maxHeight < 0f) maxHeight = 0f;

            if (frequency < 0.01f) frequency = 0.01f;
            if (octaves < 1) octaves = 1;
            if (octaves > 12) octaves = 12;
            if (lacunarity < 1.01f) lacunarity = 1.01f;
            if (gain <= 0f) gain = 0.01f;
            if (gain > 1f) gain = 1f;
            if (heightExponent < 0.1f) heightExponent = 0.1f;

            if (moistureFrequency < 0.01f) moistureFrequency = 0.01f;
            if (moistureOctaves < 1) moistureOctaves = 1;
            if (moistureOctaves > 8) moistureOctaves = 8;
            if (moistureLacunarity < 1.01f) moistureLacunarity = 1.01f;
            if (moistureGain <= 0f) moistureGain = 0.01f;
            if (moistureGain > 1f) moistureGain = 1f;
        }

        public GenerationSettings Clone() => (GenerationSettings)MemberwiseClone();
    }
}
