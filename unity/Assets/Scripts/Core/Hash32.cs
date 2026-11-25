namespace Jardin.Core
{
    /// <summary>
    /// Funciones hash deterministas de 32 bits.
    ///
    /// IMPORTANTE: aquí NO se usa string.GetHashCode(). En .NET moderno ese método
    /// está aleatorizado por proceso: devuelve un valor distinto en cada ejecución.
    /// Usarlo rompería la reproducibilidad por semilla, que es la propiedad central
    /// del proyecto. Por eso se implementa FNV-1a a mano.
    /// </summary>
    public static class Hash32
    {
        /// <summary>
        /// Mezcla de avalancha: cambiar un solo bit de la entrada cambia
        /// aproximadamente la mitad de los bits de la salida.
        /// </summary>
        public static uint Mix(uint x)
        {
            unchecked
            {
                x ^= 2747636419u;
                x *= 2654435769u;
                x ^= x >> 16;
                x *= 2654435769u;
                x ^= x >> 16;
                x *= 2654435769u;
                return x;
            }
        }

        /// <summary>Combina dos valores en uno solo, sin perder entropía.</summary>
        public static uint Combine(uint a, uint b)
        {
            unchecked
            {
                return Mix(a ^ (b + 0x9E3779B9u + (a << 6) + (a >> 2)));
            }
        }

        /// <summary>Hash de una coordenada entera 2D con semilla. Base del ruido.</summary>
        public static uint Coord(int x, int y, uint seed)
        {
            unchecked
            {
                uint h = (uint)x * 0x27D4EB2Du;
                h ^= (uint)y * 0x165667B1u;
                h ^= seed;
                return Mix(h);
            }
        }

        /// <summary>FNV-1a de 32 bits. Estable entre ejecuciones y plataformas.</summary>
        public static uint Text(string s)
        {
            unchecked
            {
                uint h = 2166136261u;
                for (int i = 0; i < s.Length; i++)
                {
                    h ^= s[i];
                    h *= 16777619u;
                }
                return h;
            }
        }

        /// <summary>Convierte un hash a un float en [0, 1).</summary>
        public static float ToUnit(uint h) => (h >> 8) * (1f / 16777216f);

        /// <summary>Convierte un hash a un float en [-1, 1).</summary>
        public static float ToSigned(uint h) => ToUnit(h) * 2f - 1f;
    }
}
