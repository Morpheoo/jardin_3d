namespace Jardin.Core
{
    /// <summary>
    /// COSTURA 3.1 — Semillas derivadas.
    ///
    /// De una sola semilla maestra se derivan subsemillas independientes, una por
    /// subsistema, mediante el hash de su nombre.
    ///
    /// Consecuencia práctica: agregar el agua (Fase 3) o los caminos (Fase 4) no
    /// altera el terreno ni los árboles de los mundos ya generados, porque cada
    /// subsistema consume su propia secuencia. Con un solo generador compartido,
    /// cualquier sistema nuevo desplazaría a todos los demás y la semilla 827461
    /// dejaría de significar lo mismo que antes.
    ///
    /// Los nombres son cadenas y no un enum a propósito: agregar un subsistema
    /// nuevo no puede renumerar accidentalmente a los existentes.
    /// </summary>
    public sealed class SeedBook
    {
        public uint Master { get; }

        public SeedBook(int master) : this(unchecked((uint)master)) { }

        public SeedBook(uint master)
        {
            Master = master;
        }

        /// <summary>Subsemilla estable para un subsistema dado.</summary>
        public uint For(string subsystem) => Hash32.Combine(Master, Hash32.Text(subsystem));

        /// <summary>Generador propio para un subsistema dado.</summary>
        public DeterministicRandom Rng(string subsystem) => new DeterministicRandom(For(subsystem));

        // Nombres de subsistema en un solo lugar, para evitar erratas silenciosas.
        // Una errata en la cadena no da error de compilación: da otro mundo.
        public const string Terrain   = "terrain";
        public const string Moisture  = "moisture";
        public const string Scatter   = "scatter";
        public const string Variation = "variation";
        public const string Water     = "water";   // reservado, Fase 3
        public const string Path      = "path";    // reservado, Fase 4
    }
}
