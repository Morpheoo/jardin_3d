using UnityEngine;
using Jardin.Generation;
using Jardin.Debugging;

/// <summary>
/// Interfaz y modo de depuración. Cumple el objetivo específico 4 del protocolo:
/// mostrar la semilla, los conteos y alternar entre vista normal y los mapas.
///
/// Está hecho con IMGUI (OnGUI) a propósito. Es la forma más rápida de tener una
/// interfaz funcional sin montar Canvas, TextMeshPro ni anclajes, y en la Fase 0
/// la interfaz no es el objetivo. Si en la Fase 1 hace falta que se vea bonita,
/// se reemplaza por UI Toolkit sin tocar ningún algoritmo.
///
/// Controles:
///   TAB  siguiente vista        R   regenerar
///   N    semilla nueva          F1  mostrar u ocultar el panel
/// </summary>
public class WorldHud : MonoBehaviour
{
    public enum View { Normal, Altura, Humedad, Pendiente, Zonas, Mezcla }

    [Header("Referencias")]
    public WorldBootstrap world;
    public PropSpawner props;

    [Header("Presentación")]
    public int mapSize = 320;
    public bool visible = true;

    private View _view = View.Normal;
    private Texture2D _texture;
    private GUIStyle _box, _label, _title;

    private void Update()
    {
        if (Input.GetKeyDown(KeyCode.F1)) visible = !visible;

        if (Input.GetKeyDown(KeyCode.Tab))
        {
            _view = (View)(((int)_view + 1) % System.Enum.GetValues(typeof(View)).Length);
            RefreshTexture();
        }

        if (Input.GetKeyDown(KeyCode.R) && world != null)
        {
            world.Regenerate();
            if (props != null) props.Rebuild(world.World);
            RefreshTexture();
        }

        if (Input.GetKeyDown(KeyCode.N) && world != null)
        {
            world.RandomSeedAndRegenerate();
            if (props != null) props.Rebuild(world.World);
            RefreshTexture();
        }
    }

    private void RefreshTexture()
    {
        var data = world != null ? world.World : null;

        if (_texture != null) Destroy(_texture);
        _texture = null;

        if (data == null || _view == View.Normal) return;

        var zoneColors = MapVisualizer.DefaultZoneColors();

        // COSTURA 3.7 en acción: cada vista nueva es una línea.
        switch (_view)
        {
            case View.Altura:
                _texture = MapVisualizer.FromField(data, data.Height, MapVisualizer.TerrainGradient());
                break;
            case View.Humedad:
                _texture = MapVisualizer.FromField(data, data.Moisture);
                break;
            case View.Pendiente:
                _texture = MapVisualizer.FromField(data, data.Slope);
                break;
            case View.Zonas:
                _texture = MapVisualizer.FromZones(data, zoneColors);
                break;
            case View.Mezcla:
                _texture = MapVisualizer.FromZoneBlend(data, zoneColors);
                break;
        }
    }

    private void EnsureStyles()
    {
        if (_box != null) return;

        _box = new GUIStyle(GUI.skin.box) { padding = new RectOffset(12, 12, 10, 10) };
        _label = new GUIStyle(GUI.skin.label) { fontSize = 13, richText = true };
        _title = new GUIStyle(GUI.skin.label) { fontSize = 16, fontStyle = FontStyle.Bold };
    }

    private void OnGUI()
    {
        if (!visible) return;

        EnsureStyles();

        var data = world != null ? world.World : null;
        if (data == null)
        {
            GUI.Box(new Rect(12, 12, 300, 60), "Sin mundo generado.\nAsigna WorldBootstrap.", _box);
            return;
        }

        float panelWidth = _view == View.Normal ? 300f : mapSize + 40f;
        float panelHeight = _view == View.Normal ? 210f : mapSize + 250f;

        GUILayout.BeginArea(new Rect(12, 12, panelWidth, panelHeight), GUIContent.none, _box);

        GUILayout.Label("WORLD DEBUG", _title);
        GUILayout.Space(4);

        GUILayout.Label($"Semilla: <b>{world.settings.seed}</b>", _label);
        GUILayout.Label($"Vista: <b>{_view}</b>", _label);
        GUILayout.Label($"Mapa: {data.WorldSize:F0} × {data.WorldSize:F0} m " +
                        $"({data.Side}×{data.Side} muestras)", _label);

        if (props != null && props.Counts != null)
        {
            GUILayout.Space(6);
            var names = props.LayerNames;
            for (int i = 0; i < props.Counts.Length && i < names.Length; i++)
                GUILayout.Label($"{names[i]}: <b>{props.Counts[i]}</b>", _label);
            GUILayout.Label($"Total: <b>{props.Total}</b>", _label);
        }

        if (_texture != null)
        {
            GUILayout.Space(8);
            var rect = GUILayoutUtility.GetRect(mapSize, mapSize, GUILayout.ExpandWidth(false));
            GUI.DrawTexture(rect, _texture, ScaleMode.ScaleToFit);
        }

        GUILayout.Space(8);
        GUILayout.Label("<color=#aaaaaa>TAB vista · R regenerar · " +
                        "N semilla nueva · F1 ocultar</color>", _label);

        GUILayout.EndArea();
    }

    private void OnDestroy()
    {
        if (_texture != null) Destroy(_texture);
    }
}
