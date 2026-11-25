using System.Diagnostics;
using Jardin.Core;

namespace Jardin.Generation
{
    /// <summary>
    /// Orquestador de la generación. Es el único punto de entrada.
    ///
    /// Nada de UnityEngine aquí: todo el pipeline de generación es C# puro,
    /// lo que permite ejecutarlo en pruebas, medirlo y, sobre todo, defenderlo
    /// como trabajo propio y no como configuración del motor.
    ///
    /// Orden fijo, porque cada paso depende del anterior:
    ///   semillas -> altura -> humedad -> pendiente -> zonas
    ///
    /// Fases posteriores insertan pasos nuevos aquí sin tocar los existentes:
    ///   Fase 3: excavar cuenca y calcular distanceToWater, antes de las zonas
    ///   Fase 4: trazar la spline y calcular pathMask, después de las zonas
    /// </summary>
    public static class WorldGenerator
    {
        /// <summary>Milisegundos que tardó la última generación. Solo informativo.</summary>
        public static double LastGenerationMs { get; private set; }

        public static WorldData Generate(GenerationSettings settings, ZoneSet zones)
        {
            settings.Validate();

            var timer = Stopwatch.StartNew();

            var seeds = new SeedBook(settings.seed);

            var world = new WorldData(
                settings.resolution,
                settings.worldSize,
                settings.maxHeight,
                seeds.Master);

            FieldGenerator.Generate(world, settings, seeds);
            ZoneClassifier.Classify(world, zones ?? ZoneSet.Default());

            timer.Stop();
            LastGenerationMs = timer.Elapsed.TotalMilliseconds;

            return world;
        }
    }
}
