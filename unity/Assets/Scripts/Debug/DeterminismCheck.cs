using UnityEngine;
using Jardin.Generation;

/// <summary>
/// REGLA DE CONTROL 2 — Ninguna mejora puede romper el determinismo.
///
/// Genera el mismo mundo varias veces y compara su checksum. Si en algún momento
/// dos ejecuciones con la misma semilla dan resultados distintos, hay una fuente
/// de aleatoriedad no controlada y la fase NO está terminada.
///
/// Causas habituales cuando esto falla:
///   - Una llamada a UnityEngine.Random dentro de la generación.
///   - Usar string.GetHashCode(), que está aleatorizado por proceso.
///   - Iterar un Dictionary o HashSet asumiendo un orden estable.
///   - Depender de Time.time, Time.frameCount o del orden de ejecución de scripts.
///   - Paralelizar sin garantizar el orden de escritura de los resultados.
///
/// Ejecutar después de CADA fase, no solo cuando algo se vea raro.
/// </summary>
public class DeterminismCheck : MonoBehaviour
{
    [Header("Configuración a probar")]
    public GenerationSettings settings = new GenerationSettings();
    public ZoneSet zones = ZoneSet.Default();

    [Tooltip("Cuántas veces se regenera el mismo mundo para comparar.")]
    public int repetitions = 3;

    [ContextMenu("Probar determinismo")]
    public void Run()
    {
        if (zones == null || zones.rules == null || zones.rules.Length == 0)
            zones = ZoneSet.Default();

        if (repetitions < 2) repetitions = 2;

        uint reference = 0;
        bool ok = true;

        for (int i = 0; i < repetitions; i++)
        {
            var world = WorldGenerator.Generate(settings.Clone(), zones);
            uint checksum = world.Checksum();

            if (i == 0)
            {
                reference = checksum;
                Debug.Log($"[Determinismo] Ejecución 1 -> {checksum:X8} (referencia)");
            }
            else if (checksum == reference)
            {
                Debug.Log($"[Determinismo] Ejecución {i + 1} -> {checksum:X8} coincide");
            }
            else
            {
                ok = false;
                Debug.LogError($"[Determinismo] Ejecución {i + 1} -> {checksum:X8} " +
                               $"NO coincide con {reference:X8}");
            }
        }

        if (ok)
            Debug.Log($"<color=#4CAF50><b>[Determinismo] CORRECTO</b></color> — " +
                      $"la semilla {settings.seed} produjo el mismo mundo {repetitions} veces.");
        else
            Debug.LogError("[Determinismo] FALLÓ. Revisa las causas habituales " +
                           "documentadas en este archivo antes de continuar.");
    }

    [ContextMenu("Probar que semillas distintas dan mundos distintos")]
    public void RunVariety()
    {
        // La prueba complementaria: un generador que siempre devuelve lo mismo
        // también pasaría la prueba anterior.
        var a = settings.Clone();
        var b = settings.Clone();
        b.seed = a.seed + 1;

        uint ca = WorldGenerator.Generate(a, zones).Checksum();
        uint cb = WorldGenerator.Generate(b, zones).Checksum();

        if (ca != cb)
            Debug.Log($"<color=#4CAF50><b>[Variedad] CORRECTO</b></color> — " +
                      $"semilla {a.seed} -> {ca:X8}, semilla {b.seed} -> {cb:X8}");
        else
            Debug.LogError($"[Variedad] FALLÓ: dos semillas distintas dieron {ca:X8}. " +
                           "La semilla no está llegando al generador.");
    }
}
