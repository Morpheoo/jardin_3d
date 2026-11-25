using System;
using System.Collections.Generic;
using Jardin.Core;

namespace Jardin.Generation
{
    /// <summary>Configuración de una familia de objetos: árboles, rocas, arbustos...</summary>
    [Serializable]
    public class PropLayer
    {
        public string name = "Arboles";

        [NonSerialized] public int RuntimeIndex;

        /// <summary>Separación nominal de la rejilla de candidatos, en metros.
        /// Es el techo de densidad: nunca habrá más de un objeto por celda.</summary>
        public float spacing = 5f;

        /// <summary>Espacio que reserva el objeto. Dos objetos chocan si su distancia
        /// es menor que la suma de sus radios.</summary>
        public float exclusionRadius = 2.2f;

        /// <summary>Pendiente máxima admitida, en [0,1].</summary>
        public float maxSlope = 0.35f;

        /// <summary>Altura normalizada admitida.</summary>
        public float minHeight01 = 0.02f;
        public float maxHeight01 = 1f;

        /// <summary>Zonas donde puede aparecer. Vacío o null significa todas.</summary>
        public byte[] allowedZones;

        /// <summary>Frecuencia de la máscara de densidad. Baja agrupa en manchas
        /// grandes; alta produce un moteado fino.</summary>
        public float densityFrequency = 2.5f;

        /// <summary>0 acepta casi todo, 1 casi nada.</summary>
        public float densityThreshold = 0.35f;

        /// <summary>Margen al borde del mapa.</summary>
        public float boundsMargin = 2f;

        public float minScale = 0.85f;
        public float maxScale = 1.25f;

        /// <summary>Tope duro. Red de seguridad contra una configuración accidental
        /// que intentara colocar cientos de miles de objetos.</summary>
        public int maxCount = 20000;
    }

    public sealed class ScatterResult
    {
        public readonly List<Placement> Placements = new List<Placement>();
        public int[] CountPerLayer;
        public int CandidatesTried;
        public double ElapsedMs;

        public int Total => Placements.Count;
    }

    /// <summary>
    /// Coloca objetos sobre el terreno respetando las reglas.
    ///
    /// Estrategia de muestreo: rejilla con perturbación (jittered grid) más rechazo.
    /// Se recorre una rejilla regular y dentro de cada celda se propone un punto
    /// desplazado al azar. Es más barato que un Poisson disk completo y, combinado
    /// con la regla de distancia mínima, produce una distribución sin patrón
    /// reconocible y sin solapamientos.
    ///
    /// Un Poisson disk de Bridson daría un reparto algo más parejo, pero no es
    /// necesario para cumplir el protocolo y añadiría complejidad a la Fase 0.
    /// Queda como mejora si alguna vez se nota el patrón.
    ///
    /// ORDEN: las capas se procesan de mayor a menor radio de exclusión. Los objetos
    /// grandes se colocan primero y los pequeños esquivan lo ya puesto. Al revés,
    /// las flores ocuparían el terreno y los árboles no cabrían en ninguna parte.
    /// </summary>
    public static class Scatterer
    {
        public static ScatterResult Scatter(WorldData world, PropLayer[] layers, SeedBook seeds)
        {
            var result = new ScatterResult();

            if (layers == null || layers.Length == 0)
            {
                result.CountPerLayer = new int[0];
                return result;
            }

            var timer = System.Diagnostics.Stopwatch.StartNew();

            result.CountPerLayer = new int[layers.Length];

            for (int i = 0; i < layers.Length; i++) layers[i].RuntimeIndex = i;

            // La rejilla se comparte entre capas: así las rocas ven a los árboles.
            // Su celda se dimensiona con el radio más grande declarado.
            float maxRadius = 0.5f;
            for (int i = 0; i < layers.Length; i++)
                if (layers[i].exclusionRadius > maxRadius) maxRadius = layers[i].exclusionRadius;

            var grid = new SpatialGrid(world.WorldSize, maxRadius * 2f);

            var ordered = OrderByRadiusDescending(layers);

            for (int li = 0; li < ordered.Length; li++)
                ScatterLayer(world, ordered[li], seeds, grid, result);

            timer.Stop();
            result.ElapsedMs = timer.Elapsed.TotalMilliseconds;

            return result;
        }

        /// <summary>
        /// Ordena por radio descendente con inserción, que es estable.
        /// List.Sort y Array.Sort NO son estables: dos capas con el mismo radio
        /// podrían intercambiarse entre ejecuciones y cambiar el mundo.
        /// </summary>
        private static PropLayer[] OrderByRadiusDescending(PropLayer[] layers)
        {
            var a = (PropLayer[])layers.Clone();

            for (int i = 1; i < a.Length; i++)
            {
                var key = a[i];
                int j = i - 1;
                while (j >= 0 && a[j].exclusionRadius < key.exclusionRadius)
                {
                    a[j + 1] = a[j];
                    j--;
                }
                a[j + 1] = key;
            }

            return a;
        }

        private static void ScatterLayer(WorldData world, PropLayer layer, SeedBook seeds,
                                         SpatialGrid grid, ScatterResult result)
        {
            float spacing = layer.spacing < 0.25f ? 0.25f : layer.spacing;

            // Una subsemilla por capa: agregar una capa nueva no desplaza a las demás.
            uint layerSeed = Hash32.Combine(seeds.For(SeedBook.Scatter), Hash32.Text(layer.name));
            uint varSeed = Hash32.Combine(seeds.For(SeedBook.Variation), Hash32.Text(layer.name));

            var placeRng = new DeterministicRandom(layerSeed);
            var varRng = new DeterministicRandom(varSeed);

            // Las reglas van de la más barata a la más cara: la mayoría de candidatos
            // muere en pendiente o zona y nunca llega a consultar la rejilla.
            //
            // El orden forma parte de la definición del mundo: cambiarlo cambia
            // qué candidatos consumen números aleatorios, y por tanto el resultado.
            var rules = new IPlacementRule[]
            {
                new BoundsRule(layer.boundsMargin),
                new SlopeRule(layer.maxSlope),
                new HeightRule(layer.minHeight01, layer.maxHeight01),
                new AllowedZonesRule(layer.allowedZones),
                new DensityMaskRule(layerSeed, layer.densityFrequency,
                                    layer.densityThreshold, placeRng),
                new MinDistanceRule(grid, layer.exclusionRadius),
                // Fase 3: new NoWaterRule(waterLevel01)
                // Fase 4: new NoPathRule(world.PathMask, pathClearance)
            };

            float half = world.WorldSize * 0.5f;
            int cells = (int)(world.WorldSize / spacing);
            int placed = 0;

            for (int cz = 0; cz < cells; cz++)
            {
                for (int cx = 0; cx < cells; cx++)
                {
                    if (placed >= layer.maxCount) return;

                    // Punto perturbado dentro de la celda.
                    float x = -half + (cx + placeRng.NextFloat()) * spacing;
                    float z = -half + (cz + placeRng.NextFloat()) * spacing;

                    var c = new PlacementCandidate
                    {
                        X = x,
                        Z = z,
                        Height01 = world.SampleField(world.Height, x, z),
                        Slope = world.SampleSlope(x, z),
                        Moisture = world.SampleMoisture(x, z),
                        Zone = world.SampleZone(x, z),
                    };
                    c.HeightMeters = c.Height01 * world.MaxHeight;

                    result.CandidatesTried++;

                    bool allowed = true;
                    for (int r = 0; r < rules.Length; r++)
                    {
                        if (!rules[r].Allows(ref c, world)) { allowed = false; break; }
                    }
                    if (!allowed) continue;

                    grid.Add(x, z, layer.exclusionRadius);

                    result.Placements.Add(new Placement
                    {
                        LayerIndex = layer.RuntimeIndex,
                        X = x,
                        Y = c.HeightMeters,
                        Z = z,
                        RotationY = varRng.Range(0f, 360f),
                        Scale = varRng.Range(layer.minScale, layer.maxScale),
                    });

                    result.CountPerLayer[layer.RuntimeIndex]++;
                    placed++;
                }
            }
        }

        /// <summary>Las capas comprometidas en el protocolo: dos árboles y una roca.</summary>
        public static PropLayer[] DefaultLayers()
        {
            return new[]
            {
                new PropLayer
                {
                    name = "ArbolA", spacing = 4.5f, exclusionRadius = 2.4f,
                    maxSlope = 0.32f, minHeight01 = 0.05f, maxHeight01 = 0.85f,
                    allowedZones = new[] { ZoneSet.Bosque },
                    densityFrequency = 2.2f, densityThreshold = 0.20f,
                    minScale = 0.85f, maxScale = 1.30f,
                },
                new PropLayer
                {
                    name = "ArbolB", spacing = 7f, exclusionRadius = 2.0f,
                    maxSlope = 0.30f, minHeight01 = 0.05f, maxHeight01 = 0.75f,
                    allowedZones = new[] { ZoneSet.Bosque, ZoneSet.Pradera },
                    densityFrequency = 3.0f, densityThreshold = 0.55f,
                    minScale = 0.80f, maxScale = 1.20f,
                },
                new PropLayer
                {
                    name = "Roca", spacing = 3.5f, exclusionRadius = 0.9f,
                    maxSlope = 0.60f, minHeight01 = 0.02f, maxHeight01 = 1f,
                    allowedZones = null,
                    densityFrequency = 4.0f, densityThreshold = 0.62f,
                    minScale = 0.60f, maxScale = 1.60f,
                },
            };
        }
    }
}
