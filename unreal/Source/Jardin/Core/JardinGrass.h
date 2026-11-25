// JardinGrass.h — Pasto denso por trozos alrededor de la camara.
//
// C++ PLANO. Sin tipos de Unreal. Unidades en METROS.
//
// Un tapiz de pasto en TODO el mapa serian millones de matas. Como en los
// juegos de mundo abierto, solo se generan los trozos (cuadros de ChunkSize
// metros) cercanos a la camara; al moverse, se generan los nuevos y se
// descartan los lejanos. Para que eso funcione, cada trozo tiene que poder
// generarse SOLO, sin depender de sus vecinos ni del orden en que se visiten:
// su contenido sale de una subsemilla derivada de (semilla del jardin, x, z).
// Ir y volver da exactamente el mismo pasto.

#pragma once

#include "JardinWorld.h"

namespace jardin
{

struct GrassInstance
{
    float X = 0.0f, Z = 0.0f;   // metros, coordenadas de mundo
    float H = 0.0f;             // altura del suelo, metros
    float Yaw = 0.0f;           // grados
    float Scale = 1.0f;
    int32_t Variant = 0;
};

struct GrassParams
{
    float Density = 4.0f;          // candidatos por m2 (rejilla perturbada)
    float ForestFactor = 0.3f;     // en el bosque crece menos (sombra)
    float MaxSlope = 0.45f;        // como el pasto de la colocacion
    float PatchFrequency = 22.0f;  // manchas: ciclos a lo ancho del mapa
    float PatchCut = 0.30f;        // por debajo de este valor de la mancha, suelo desnudo
    float MinScale = 0.7f, MaxScale = 1.3f;
    int32_t Variants = 1;
    float WaterClear = 0.4f;       // metros minimos a la orilla
    float PathClear = 0.3f;        // metros minimos al borde del camino
};

// ---------------------------------------------------------------- mata de pasto

// Una mata = Blades briznas finas, cada una un abanico de triangulos que se
// estrecha hasta la punta y se curva hacia fuera. Sistema LOCAL de la mata:
// Z hacia arriba, la base en el origen (como el arbol fractal).
struct GrassClumpParams
{
    int32_t Blades = 14;
    int32_t Segments = 3;          // tramos por brizna (curvatura)
    float Radius = 0.10f;          // metros: radio del circulo de raices
    float Height = 0.20f;          // metros: altura media de las briznas
    float HeightJitter = 0.40f;    // +-40%
    float Width = 0.014f;          // metros: ancho en la base
    float MaxLeanDeg = 32.0f;      // inclinacion maxima respecto a la vertical
};

struct GrassClumpMesh
{
    std::vector<float> PX, PY, PZ;   // posiciones, metros (Z arriba)
    std::vector<float> U, V;         // U: 0..1 a lo ancho; V: 0 raiz .. 1 punta
    std::vector<float> BladeRand;    // numero al azar de la brizna (tono propio)
    std::vector<int32_t> Indices;    // cada cara va DOS veces (una por lado)
    int32_t BladeCount = 0;
};

namespace GrassClump
{
    /**
     * Construye una mata. Las normales NO se guardan: todas apuntan hacia
     * arriba (0, 0, 1), como las del suelo. Es el truco del pasto de los
     * juegos de mundo abierto: las briznas se iluminan igual que el terreno y
     * el pasto se lee como un tapete continuo, no como laminas sueltas.
     * Por eso cada cara se emite por los dos lados con la misma normal (un
     * material de dos caras invertiria la normal por detras y la oscureceria).
     */
    GrassClumpMesh Build(const GrassClumpParams& P, uint32_t Seed);
}

namespace GrassField
{
    /** Coordenada del trozo que contiene la posicion (metros). */
    inline int32_t ChunkCoord(float Meters, float ChunkSize)
    {
        return static_cast<int32_t>(std::floor(Meters / ChunkSize));
    }

    /**
     * Pasto del trozo (Cx, Cz): el cuadro [Cx*ChunkSize, (Cx+1)*ChunkSize) x
     * [Cz*ChunkSize, ...). Determinista e independiente de los demas trozos.
     * Cada candidato consume siempre los mismos numeros aleatorios, se acepte
     * o no: cambiar una regla no desplaza a los vecinos.
     */
    void BuildChunk(const WorldData& World, const SeedBook& Seeds, int32_t Cx, int32_t Cz,
                    float ChunkSize, const GrassParams& P, std::vector<GrassInstance>& Out);
}

} // namespace jardin
