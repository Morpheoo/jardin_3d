using UnityEngine;
using Jardin.Generation;
using Jardin.MeshBuilding;

/// <summary>
/// Punto de entrada de la escena. Genera el mundo y construye su malla.
///
/// Uso:
///   1. Crea un GameObject vacío en la escena y llámalo "World".
///   2. Agrégale este componente.
///   3. Asigna un material al campo terrainMaterial (cualquiera de URP sirve por ahora).
///   4. Play. O bien, sin entrar en Play: clic derecho en el componente -> Regenerar.
///
/// El botón de contexto permite iterar sobre los parámetros sin entrar en modo Play,
/// que es lo que hace tolerable ajustar el ruido: los cambios se ven al instante.
/// </summary>
[ExecuteAlways]
public class WorldBootstrap : MonoBehaviour
{
    [Header("Generación")]
    public GenerationSettings settings = new GenerationSettings();

    [Header("Zonas")]
    public ZoneSet zones = ZoneSet.Default();

    [Header("Render")]
    public Material terrainMaterial;

    [Header("Objetos")]
    [Tooltip("Opcional. Si se asigna, los objetos se recolocan al regenerar.")]
    public PropSpawner props;

    [Header("Estado (solo lectura)")]
    [SerializeField] private int _vertexCount;
    [SerializeField] private double _generationMs;
    [SerializeField] private string _checksum;

    /// <summary>El mundo generado. Lo consultan la colocación, el jugador y el modo debug.</summary>
    public WorldData World { get; private set; }

    private GameObject _terrainObject;
    private Mesh _mesh;

    private void Start()
    {
        if (Application.isPlaying) Regenerate();
    }

    [ContextMenu("Regenerar")]
    public void Regenerate()
    {
        if (zones == null || zones.rules == null || zones.rules.Length == 0)
            zones = ZoneSet.Default();

        World = WorldGenerator.Generate(settings, zones);

        BuildTerrainObject();

        if (props != null) props.Rebuild(World);

        _vertexCount = World.Side * World.Side;
        _generationMs = WorldGenerator.LastGenerationMs;
        _checksum = World.Checksum().ToString("X8");

        Debug.Log($"[Jardin] Mundo generado. Semilla {settings.seed} | " +
                  $"{_vertexCount} vértices | {_generationMs:F1} ms | checksum {_checksum}");
    }

    [ContextMenu("Semilla aleatoria y regenerar")]
    public void RandomSeedAndRegenerate()
    {
        // Aquí sí es legítimo usar Random: elegir la semilla es una acción del
        // usuario, no parte de la generación. A partir de ese número, todo lo
        // demás es determinista.
        settings.seed = Random.Range(int.MinValue, int.MaxValue);
        Regenerate();
    }

    private void BuildTerrainObject()
    {
        if (_terrainObject == null)
        {
            var existing = transform.Find("Terrain");
            if (existing != null) _terrainObject = existing.gameObject;
        }

        if (_terrainObject == null)
        {
            _terrainObject = new GameObject("Terrain");
            _terrainObject.transform.SetParent(transform, false);
            _terrainObject.AddComponent<MeshFilter>();
            _terrainObject.AddComponent<MeshRenderer>();
            _terrainObject.AddComponent<MeshCollider>();
        }

        var filter = _terrainObject.GetComponent<MeshFilter>();
        var renderer = _terrainObject.GetComponent<MeshRenderer>();
        var collider = _terrainObject.GetComponent<MeshCollider>();

        // Liberar la malla anterior. Sin esto, cada regeneración en el editor deja
        // una malla huérfana en memoria y tras un rato de iterar se nota.
        if (_mesh != null)
        {
            if (Application.isPlaying) Destroy(_mesh);
            else DestroyImmediate(_mesh);
        }

        _mesh = TerrainMeshBuilder.Build(World);

        filter.sharedMesh = _mesh;
        collider.sharedMesh = null;      // forzar que reconstruya su árbol de colisión
        collider.sharedMesh = _mesh;

        if (terrainMaterial != null)
            renderer.sharedMaterial = terrainMaterial;
    }
}
