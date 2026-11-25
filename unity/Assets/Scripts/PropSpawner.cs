using System;
using System.Collections.Generic;
using UnityEngine;
using Jardin.Core;
using Jardin.Generation;

/// <summary>
/// Instancia en la escena los objetos que decidió el Scatterer.
///
/// Separación deliberada: el Scatterer decide DÓNDE va cada cosa y es C# puro;
/// este componente solo traduce esa lista a GameObjects. Si algún día se cambia
/// la forma de dibujar (por ejemplo Graphics.DrawMeshInstanced en la Fase 2 para
/// el césped), se reescribe este archivo y el algoritmo no se toca.
/// </summary>
public class PropSpawner : MonoBehaviour
{
    [Serializable]
    public class LayerBinding
    {
        public GameObject prefab;
        public PropLayer settings = new PropLayer();
    }

    [Header("Capas")]
    public LayerBinding[] layers;

    [Header("Estado (solo lectura)")]
    [SerializeField] private int[] _counts;
    [SerializeField] private int _total;
    [SerializeField] private double _scatterMs;

    public int Total => _total;
    public int[] Counts => _counts;
    public string[] LayerNames
    {
        get
        {
            var n = new string[layers != null ? layers.Length : 0];
            for (int i = 0; i < n.Length; i++) n[i] = layers[i].settings.name;
            return n;
        }
    }

    private Transform _container;

    public void Rebuild(WorldData world)
    {
        if (world == null) return;

        EnsureLayers();
        ClearContainer();

        var settings = new PropLayer[layers.Length];
        for (int i = 0; i < layers.Length; i++) settings[i] = layers[i].settings;

        var seeds = new SeedBook(world.MasterSeed);
        var result = Scatterer.Scatter(world, settings, seeds);

        _counts = result.CountPerLayer;
        _total = result.Total;
        _scatterMs = result.ElapsedMs;

        // Los objetos se agrupan por capa para que la jerarquía sea legible
        // y para poder ocultar una familia entera al depurar.
        var parents = new Transform[layers.Length];
        for (int i = 0; i < layers.Length; i++)
        {
            var go = new GameObject(layers[i].settings.name);
            go.transform.SetParent(_container, false);
            parents[i] = go.transform;
        }

        int skippedNoPrefab = 0;

        foreach (var p in result.Placements)
        {
            var prefab = layers[p.LayerIndex].prefab;
            if (prefab == null) { skippedNoPrefab++; continue; }

            var instance = Instantiate(
                prefab,
                new Vector3(p.X, p.Y, p.Z),
                Quaternion.Euler(0f, p.RotationY, 0f),
                parents[p.LayerIndex]);

            instance.transform.localScale = Vector3.one * p.Scale;

            // Marcar como estático permite a Unity aplicar batching y occlusion:
            // el mundo se genera una vez y no se mueve.
            instance.isStatic = true;
        }

        if (skippedNoPrefab > 0)
            Debug.LogWarning($"[Jardin] {skippedNoPrefab} objetos sin prefab asignado. " +
                             "Asigna un prefab a cada capa del PropSpawner.");

        Debug.Log($"[Jardin] Colocados {_total} objetos en {_scatterMs:F1} ms " +
                  $"({result.CandidatesTried} candidatos probados).");
    }

    private void EnsureLayers()
    {
        if (layers != null && layers.Length > 0) return;

        // Arranque con las capas del protocolo: dos árboles y una roca.
        var defaults = Scatterer.DefaultLayers();
        layers = new LayerBinding[defaults.Length];
        for (int i = 0; i < defaults.Length; i++)
            layers[i] = new LayerBinding { settings = defaults[i] };
    }

    private void ClearContainer()
    {
        if (_container == null)
        {
            var existing = transform.Find("Props");
            if (existing != null) _container = existing;
        }

        if (_container != null)
        {
            if (Application.isPlaying) Destroy(_container.gameObject);
            else DestroyImmediate(_container.gameObject);
        }

        var go = new GameObject("Props");
        go.transform.SetParent(transform, false);
        _container = go.transform;
    }
}
