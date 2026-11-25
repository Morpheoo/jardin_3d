using UnityEngine;
using UnityEngine.Rendering;
using Jardin.Generation;

namespace Jardin.MeshBuilding
{
    /// <summary>
    /// Construye la malla del terreno a partir de WorldData.
    ///
    /// Esta es la parte que el protocolo compromete explícitamente: una malla de
    /// vértices y triángulos construida por código, con sus normales.
    /// Unity Terrain queda descartado precisamente porque resolvería esto solo.
    ///
    /// La malla lleva datos extra por vértice que la Fase 1 necesitará para
    /// colorear el terreno por altura y pendiente sin pintar ninguna textura:
    ///   color.rgb = (altura normalizada, pendiente, humedad)
    ///   uv2       = (id de zona, mezcla de zona)
    /// Calcularlos ahora no cuesta nada; recalcularlos después obligaría a
    /// reconstruir la malla entera.
    /// </summary>
    public static class TerrainMeshBuilder
    {
        public static Mesh Build(WorldData world, string name = "TerrainMesh")
        {
            int side = world.Side;
            int vertexCount = side * side;
            int quadCount = world.Resolution * world.Resolution;

            var vertices = new Vector3[vertexCount];
            var uv        = new Vector2[vertexCount];
            var uv2       = new Vector2[vertexCount];
            var colors    = new Color[vertexCount];
            var normals   = new Vector3[vertexCount];
            var triangles = new int[quadCount * 6];

            // ---------- vértices ----------
            for (int y = 0; y < side; y++)
            {
                for (int x = 0; x < side; x++)
                {
                    int i = world.Index(x, y);

                    vertices[i] = new Vector3(
                        world.GridToWorldX(x),
                        world.HeightMeters(x, y),
                        world.GridToWorldZ(y));

                    float u = (float)x / world.Resolution;
                    float v = (float)y / world.Resolution;

                    uv[i]  = new Vector2(u, v);
                    uv2[i] = new Vector2(world.Zone[i], world.ZoneBlend[i]);

                    colors[i] = new Color(
                        world.Height[i],
                        world.Slope[i],
                        world.Moisture[i],
                        1f);
                }
            }

            // ---------- triángulos ----------
            // Cada celda de la rejilla son dos triángulos.
            // El orden de los índices define hacia dónde mira la cara: invertirlo
            // deja el terreno visible solo desde abajo, que es el error clásico aquí.
            int t = 0;
            for (int y = 0; y < world.Resolution; y++)
            {
                for (int x = 0; x < world.Resolution; x++)
                {
                    int i0 = world.Index(x,     y);
                    int i1 = world.Index(x + 1, y);
                    int i2 = world.Index(x,     y + 1);
                    int i3 = world.Index(x + 1, y + 1);

                    triangles[t++] = i0;
                    triangles[t++] = i2;
                    triangles[t++] = i1;

                    triangles[t++] = i1;
                    triangles[t++] = i2;
                    triangles[t++] = i3;
                }
            }

            // ---------- normales ----------
            ComputeNormals(world, normals);

            var mesh = new Mesh { name = name };

            // Por encima de 65535 vértices hacen falta índices de 32 bits.
            // Con resolution = 192 son 37249, pero subir a 256 ya rebasa el límite
            // y el síntoma es una malla corrupta sin mensaje de error claro.
            mesh.indexFormat = vertexCount > 65535 ? IndexFormat.UInt32 : IndexFormat.UInt16;

            mesh.vertices  = vertices;
            mesh.triangles = triangles;
            mesh.uv        = uv;
            mesh.uv2       = uv2;
            mesh.colors    = colors;
            mesh.normals   = normals;

            mesh.RecalculateBounds();

            return mesh;
        }

        /// <summary>
        /// Normales calculadas a partir del gradiente del campo de altura.
        ///
        /// Se prefiere esto a Mesh.RecalculateNormals() por dos motivos:
        /// es más rápido, y da normales continuas exactas en lugar del promedio
        /// de las caras vecinas, que produce facetado visible en pendientes suaves.
        ///
        /// Para una superficie y = f(x, z), la normal es (-df/dx, 1, -df/dz)
        /// normalizada. Las derivadas se aproximan por diferencias centrales.
        /// </summary>
        private static void ComputeNormals(WorldData world, Vector3[] normals)
        {
            int side = world.Side;
            float cell = world.CellSize;

            for (int y = 0; y < side; y++)
            {
                for (int x = 0; x < side; x++)
                {
                    int xm = x > 0 ? x - 1 : x;
                    int xp = x < side - 1 ? x + 1 : x;
                    int ym = y > 0 ? y - 1 : y;
                    int yp = y < side - 1 ? y + 1 : y;

                    float dx = (world.HeightMeters(xp, y) - world.HeightMeters(xm, y))
                               / ((xp - xm) * cell);
                    float dz = (world.HeightMeters(x, yp) - world.HeightMeters(x, ym))
                               / ((yp - ym) * cell);

                    normals[world.Index(x, y)] = new Vector3(-dx, 1f, -dz).normalized;
                }
            }
        }
    }
}
