using System.Collections.Generic;

namespace Jardin.Core
{
    /// <summary>
    /// COSTURA 3.5 — Rejilla espacial.
    ///
    /// Responde "¿hay algo ya colocado demasiado cerca de este punto?" sin recorrer
    /// la lista completa de objetos.
    ///
    /// Sin esto, colocar N objetos comparando cada candidato contra todos los
    /// anteriores cuesta O(N²): con 200 objetos no se nota y con 5000 la generación
    /// pasa de milisegundos a decenas de segundos. La Fase 2 mete césped instanciado
    /// justamente en ese orden de magnitud.
    ///
    /// Cada entrada guarda su propio radio de exclusión, de modo que un árbol
    /// reserva más espacio que una flor sin necesidad de rejillas separadas.
    /// Dos entradas chocan si su distancia es menor que la suma de sus radios.
    /// </summary>
    public sealed class SpatialGrid
    {
        private readonly float _cellSize;
        private readonly int _cols;
        private readonly int _rows;
        private readonly float _minX;
        private readonly float _minZ;

        private readonly List<int>[] _cells;
        private readonly List<float> _x = new List<float>();
        private readonly List<float> _z = new List<float>();
        private readonly List<float> _radius = new List<float>();

        /// <summary>Radio más grande almacenado. Define cuántas celdas hay que revisar.</summary>
        private float _maxRadius;

        public int Count => _x.Count;

        /// <param name="worldSize">Lado del mundo en metros. El mundo está centrado en el origen.</param>
        /// <param name="cellSize">Lado de celda. Conviene que sea del orden del radio
        /// de exclusión más común: celdas muy chicas gastan memoria, muy grandes
        /// degeneran en la búsqueda lineal que se quería evitar.</param>
        public SpatialGrid(float worldSize, float cellSize)
        {
            if (cellSize < 0.1f) cellSize = 0.1f;

            _cellSize = cellSize;
            _minX = -worldSize * 0.5f;
            _minZ = -worldSize * 0.5f;

            _cols = (int)(worldSize / cellSize) + 2;
            _rows = _cols;

            _cells = new List<int>[_cols * _rows];
        }

        private int ColOf(float x)
        {
            int c = (int)((x - _minX) / _cellSize);
            return c < 0 ? 0 : (c >= _cols ? _cols - 1 : c);
        }

        private int RowOf(float z)
        {
            int r = (int)((z - _minZ) / _cellSize);
            return r < 0 ? 0 : (r >= _rows ? _rows - 1 : r);
        }

        public void Add(float x, float z, float radius)
        {
            int index = _x.Count;
            _x.Add(x);
            _z.Add(z);
            _radius.Add(radius);

            if (radius > _maxRadius) _maxRadius = radius;

            int cell = RowOf(z) * _cols + ColOf(x);
            if (_cells[cell] == null) _cells[cell] = new List<int>(4);
            _cells[cell].Add(index);
        }

        /// <summary>
        /// True si ya existe una entrada cuya distancia al punto es menor que la
        /// suma de ambos radios.
        /// </summary>
        public bool HasNeighborWithin(float x, float z, float radius)
        {
            // Hay que revisar hasta donde pueda alcanzar el objeto más grande ya
            // colocado, no solo el radio del candidato: un árbol enorme a tres
            // celdas de distancia todavía puede invadir este punto.
            float reach = radius + _maxRadius;
            int span = (int)(reach / _cellSize) + 1;

            int c0 = ColOf(x), r0 = RowOf(z);
            int cMin = c0 - span, cMax = c0 + span;
            int rMin = r0 - span, rMax = r0 + span;

            if (cMin < 0) cMin = 0;
            if (rMin < 0) rMin = 0;
            if (cMax >= _cols) cMax = _cols - 1;
            if (rMax >= _rows) rMax = _rows - 1;

            for (int r = rMin; r <= rMax; r++)
            {
                int rowBase = r * _cols;
                for (int c = cMin; c <= cMax; c++)
                {
                    var bucket = _cells[rowBase + c];
                    if (bucket == null) continue;

                    for (int i = 0; i < bucket.Count; i++)
                    {
                        int e = bucket[i];
                        float dx = _x[e] - x;
                        float dz = _z[e] - z;
                        float minDist = _radius[e] + radius;

                        // Se comparan cuadrados para no calcular raíces cuadradas.
                        if (dx * dx + dz * dz < minDist * minDist) return true;
                    }
                }
            }

            return false;
        }

        public void Clear()
        {
            for (int i = 0; i < _cells.Length; i++) _cells[i] = null;
            _x.Clear();
            _z.Clear();
            _radius.Clear();
            _maxRadius = 0f;
        }
    }
}
