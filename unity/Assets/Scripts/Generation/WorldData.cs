using Jardin.Core;

namespace Jardin.Generation
{
    /// <summary>
    /// COSTURA 3.2 — Los campos son datos, no cálculos incrustados.
    ///
    /// La generación LLENA estos arreglos. El renderizado, la colocación de objetos
    /// y el modo de depuración solo los LEEN. Nadie recalcula ruido por su cuenta.
    ///
    /// Consecuencia: agregar un campo nuevo en una fase posterior
    /// (distanceToWater en la Fase 3, pathMask en la Fase 4) es agregar un arreglo
    /// aquí y llenarlo. No se toca nada de lo existente.
    ///
    /// Los arreglos son 1D con un índice calculado, no [,]. En C# los arreglos
    /// multidimensionales tienen acceso notablemente más lento porque no reciben
    /// las mismas optimizaciones de rango que los de una dimensión.
    ///
    /// Sistema de coordenadas: el mundo está centrado en el origen.
    /// X y Z van de -worldSize/2 a +worldSize/2. Y es la altura en metros.
    /// </summary>
    public sealed class WorldData
    {
        /// <summary>Celdas por lado.</summary>
        public readonly int Resolution;

        /// <summary>Muestras por lado. Siempre Resolution + 1.</summary>
        public readonly int Side;

        public readonly float WorldSize;
        public readonly float MaxHeight;
        public readonly uint MasterSeed;

        /// <summary>Altura normalizada en [0, 1].</summary>
        public readonly float[] Height;

        /// <summary>Humedad en [0, 1].</summary>
        public readonly float[] Moisture;

        /// <summary>Pendiente en [0, 1]. 0 es plano, 1 es vertical.</summary>
        public readonly float[] Slope;

        /// <summary>Identificador de zona ganadora.</summary>
        public readonly byte[] Zone;

        /// <summary>
        /// Margen entre el primer y el segundo puntaje de zona, en [0, 1].
        /// En la Fase 0 no se usa. Existe desde ahora porque es exactamente
        /// lo que necesita la Fase 6 para fronteras difusas en vez de cortes duros,
        /// y recalcularlo después obligaría a reejecutar el clasificador.
        /// </summary>
        public readonly float[] ZoneBlend;

        public WorldData(int resolution, float worldSize, float maxHeight, uint masterSeed)
        {
            Resolution = resolution;
            Side = resolution + 1;
            WorldSize = worldSize;
            MaxHeight = maxHeight;
            MasterSeed = masterSeed;

            int count = Side * Side;
            Height    = new float[count];
            Moisture  = new float[count];
            Slope     = new float[count];
            Zone      = new byte[count];
            ZoneBlend = new float[count];
        }

        /// <summary>Tamaño de una celda en metros.</summary>
        public float CellSize => WorldSize / Resolution;

        public int Index(int x, int y) => y * Side + x;

        public float HeightMeters(int x, int y) => Height[Index(x, y)] * MaxHeight;

        // ---------- conversión entre rejilla y mundo ----------

        /// <summary>Coordenada X de mundo, en metros, de la muestra de rejilla x.</summary>
        public float GridToWorldX(int x) => x * CellSize - WorldSize * 0.5f;

        /// <summary>Coordenada Z de mundo, en metros, de la muestra de rejilla y.</summary>
        public float GridToWorldZ(int y) => y * CellSize - WorldSize * 0.5f;

        // ---------- muestreo continuo ----------

        private static float Clamp01(float v) => v < 0f ? 0f : (v > 1f ? 1f : v);
        private static float Lerp(float a, float b, float t) => a + (b - a) * t;

        /// <summary>
        /// Interpolación bilineal de un campo cualquiera en coordenadas de mundo.
        /// Sirve para consultar altura o pendiente en posiciones que no caen
        /// exactamente sobre un vértice: es lo que necesitan la colocación de
        /// objetos y el jugador.
        /// </summary>
        public float SampleField(float[] field, float worldX, float worldZ)
        {
            float u = Clamp01((worldX + WorldSize * 0.5f) / WorldSize) * Resolution;
            float v = Clamp01((worldZ + WorldSize * 0.5f) / WorldSize) * Resolution;

            int x0 = (int)u;
            int y0 = (int)v;
            if (x0 >= Resolution) x0 = Resolution - 1;
            if (y0 >= Resolution) y0 = Resolution - 1;

            float tx = u - x0;
            float ty = v - y0;

            float a = field[Index(x0,     y0)];
            float b = field[Index(x0 + 1, y0)];
            float c = field[Index(x0,     y0 + 1)];
            float d = field[Index(x0 + 1, y0 + 1)];

            return Lerp(Lerp(a, b, tx), Lerp(c, d, tx), ty);
        }

        /// <summary>Altura del terreno en metros en una posición de mundo.</summary>
        public float SampleHeightMeters(float worldX, float worldZ)
            => SampleField(Height, worldX, worldZ) * MaxHeight;

        public float SampleSlope(float worldX, float worldZ)
            => SampleField(Slope, worldX, worldZ);

        public float SampleMoisture(float worldX, float worldZ)
            => SampleField(Moisture, worldX, worldZ);

        /// <summary>Zona en una posición de mundo. Vecino más cercano, no interpolada.</summary>
        public byte SampleZone(float worldX, float worldZ)
        {
            float u = Clamp01((worldX + WorldSize * 0.5f) / WorldSize) * Resolution;
            float v = Clamp01((worldZ + WorldSize * 0.5f) / WorldSize) * Resolution;
            int x = (int)(u + 0.5f);
            int y = (int)(v + 0.5f);
            if (x > Resolution) x = Resolution;
            if (y > Resolution) y = Resolution;
            return Zone[Index(x, y)];
        }

        /// <summary>True si la posición está dentro de los límites del mapa.</summary>
        public bool InsideBounds(float worldX, float worldZ, float margin = 0f)
        {
            float half = WorldSize * 0.5f - margin;
            return worldX >= -half && worldX <= half && worldZ >= -half && worldZ <= half;
        }

        // ---------- verificación ----------

        /// <summary>
        /// Huella digital del mundo generado.
        ///
        /// REGLA DE CONTROL 2: después de cada fase, generar la misma semilla dos veces
        /// y comparar este valor. Si cambió, algo introdujo una fuente de aleatoriedad
        /// no determinista y la fase no está terminada.
        ///
        /// Las alturas se cuantizan antes de mezclarse: un float idéntico da siempre
        /// los mismos bits, pero cuantizar evita que una diferencia irrelevante
        /// en el último bit dispare una falsa alarma.
        /// </summary>
        public uint Checksum()
        {
            unchecked
            {
                uint h = 2166136261u;
                for (int i = 0; i < Height.Length; i++)
                {
                    uint q = (uint)(Height[i] * 65535f);
                    h = Hash32.Combine(h, q);
                    h = Hash32.Combine(h, Zone[i]);
                }
                return h;
            }
        }
    }
}
