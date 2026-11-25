namespace Jardin.Core
{
    /// <summary>
    /// Generador pseudoaleatorio determinista (xorshift32).
    ///
    /// Sustituye por completo a UnityEngine.Random dentro de la generación del mundo.
    /// UnityEngine.Random comparte un estado global: cualquier otro sistema que lo use
    /// desplazaría la secuencia y el mismo número de semilla dejaría de producir
    /// el mismo mundo.
    /// </summary>
    public sealed class DeterministicRandom
    {
        private uint _state;

        public DeterministicRandom(uint seed)
        {
            // El estado 0 es un punto fijo de xorshift: se quedaría atascado en 0.
            _state = seed == 0u ? 0x9E3779B9u : seed;
        }

        /// <summary>Estado interno actual. Útil para depurar y para checksums.</summary>
        public uint State => _state;

        public uint NextUInt()
        {
            unchecked
            {
                uint x = _state;
                x ^= x << 13;
                x ^= x >> 17;
                x ^= x << 5;
                _state = x;
                return x;
            }
        }

        /// <summary>Float en [0, 1).</summary>
        public float NextFloat() => (NextUInt() >> 8) * (1f / 16777216f);

        /// <summary>Float en [min, max).</summary>
        public float Range(float min, float max) => min + (max - min) * NextFloat();

        /// <summary>Entero en [minInclusive, maxExclusive).</summary>
        public int Range(int minInclusive, int maxExclusive)
        {
            int span = maxExclusive - minInclusive;
            if (span <= 0) return minInclusive;
            return minInclusive + (int)(NextUInt() % (uint)span);
        }

        /// <summary>Devuelve true con probabilidad p.</summary>
        public bool Chance(float p) => NextFloat() < p;
    }
}
