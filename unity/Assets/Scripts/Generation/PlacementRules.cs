using System;
using Jardin.Core;

namespace Jardin.Generation
{
    /// <summary>Un punto propuesto, ya resuelto contra los campos del mundo.</summary>
    public struct PlacementCandidate
    {
        public float X;
        public float Z;
        public float HeightMeters;
        public float Height01;
        public float Slope;
        public float Moisture;
        public byte Zone;
    }

    /// <summary>Un objeto aceptado y listo para instanciar.</summary>
    public struct Placement
    {
        public int LayerIndex;
        public float X, Y, Z;
        public float RotationY;   // grados
        public float Scale;
    }

    /// <summary>
    /// COSTURA 3.4 — Colocación como cadena de reglas.
    ///
    /// Cada restricción es una clase independiente. El colocador no sabe qué reglas
    /// existen: solo las recorre y descarta el candidato en la primera que falle.
    ///
    /// Agregar una restricción nueva en una fase posterior es escribir una clase
    /// de ~10 líneas y meterla en la lista. No se toca el colocador:
    ///   Fase 3 -> ReglaNoEnAgua      (rechazar por debajo del nivel del agua)
    ///   Fase 4 -> ReglaNoSobreCamino (rechazar dentro de la máscara del camino)
    ///
    /// Comparar con la alternativa: un if gigante dentro del bucle de colocación,
    /// que habría que reabrir y volver a probar en cada fase.
    /// </summary>
    public interface IPlacementRule
    {
        string Name { get; }
        bool Allows(ref PlacementCandidate c, WorldData world);
    }

    /// <summary>Rechaza terreno demasiado inclinado. Evita árboles clavados en un talud.</summary>
    public sealed class SlopeRule : IPlacementRule
    {
        private readonly float _maxSlope;
        public SlopeRule(float maxSlope) { _maxSlope = maxSlope; }
        public string Name => "Pendiente";
        public bool Allows(ref PlacementCandidate c, WorldData world) => c.Slope <= _maxSlope;
    }

    /// <summary>Restringe la capa a ciertas zonas. Null o vacío significa "todas".</summary>
    public sealed class AllowedZonesRule : IPlacementRule
    {
        private readonly byte[] _allowed;
        public AllowedZonesRule(byte[] allowed) { _allowed = allowed; }
        public string Name => "Zona";

        public bool Allows(ref PlacementCandidate c, WorldData world)
        {
            if (_allowed == null || _allowed.Length == 0) return true;
            for (int i = 0; i < _allowed.Length; i++)
                if (_allowed[i] == c.Zone) return true;
            return false;
        }
    }

    /// <summary>
    /// Acota la altura normalizada. Hoy sirve para mantener objetos fuera de las
    /// cimas o de las hondonadas; en la Fase 3 el mismo mecanismo, con el nivel
    /// del agua como mínimo, es lo que impide árboles dentro del estanque.
    /// </summary>
    public sealed class HeightRule : IPlacementRule
    {
        private readonly float _min, _max;
        public HeightRule(float min, float max) { _min = min; _max = max; }
        public string Name => "Altura";
        public bool Allows(ref PlacementCandidate c, WorldData world)
            => c.Height01 >= _min && c.Height01 <= _max;
    }

    /// <summary>Mantiene los objetos lejos del borde para que no queden cortados.</summary>
    public sealed class BoundsRule : IPlacementRule
    {
        private readonly float _margin;
        public BoundsRule(float margin) { _margin = margin; }
        public string Name => "Límites";
        public bool Allows(ref PlacementCandidate c, WorldData world)
            => world.InsideBounds(c.X, c.Z, _margin);
    }

    /// <summary>
    /// Distancia mínima contra todo lo ya colocado, consultando la rejilla espacial.
    ///
    /// Es la única regla con estado: lee una rejilla que crece conforme se colocan
    /// objetos. Por eso la rejilla se comparte entre todas las capas, y es lo que
    /// hace que las rocas esquiven a los árboles colocados antes que ellas.
    /// </summary>
    public sealed class MinDistanceRule : IPlacementRule
    {
        private readonly SpatialGrid _grid;
        private readonly float _radius;
        public MinDistanceRule(SpatialGrid grid, float radius) { _grid = grid; _radius = radius; }
        public string Name => "Distancia mínima";
        public bool Allows(ref PlacementCandidate c, WorldData world)
            => !_grid.HasNeighborWithin(c.X, c.Z, _radius);
    }

    /// <summary>
    /// Máscara de densidad por ruido: en vez de repartir uniformemente, agrupa.
    ///
    /// Es la diferencia entre un bosque que parece bosque y árboles espolvoreados
    /// con Math.random(). El ruido crea claros y espesuras sin reglas adicionales.
    /// </summary>
    public sealed class DensityMaskRule : IPlacementRule
    {
        private readonly uint _seed;
        private readonly float _frequency;
        private readonly float _threshold;
        private readonly DeterministicRandom _rng;

        public DensityMaskRule(uint seed, float frequency, float threshold, DeterministicRandom rng)
        {
            _seed = seed;
            _frequency = frequency;
            _threshold = threshold;
            _rng = rng;
        }

        public string Name => "Máscara de densidad";

        public bool Allows(ref PlacementCandidate c, WorldData world)
        {
            float u = (c.X + world.WorldSize * 0.5f) / world.WorldSize;
            float v = (c.Z + world.WorldSize * 0.5f) / world.WorldSize;
            float mask = Noise.Fbm01(u, v, _seed, 3, _frequency, 2f, 0.5f);

            // El umbral desplaza la máscara: 0 acepta casi todo, 1 casi nada.
            return _rng.NextFloat() < mask * (1f - _threshold) + (1f - _threshold) * 0.25f;
        }
    }
}
