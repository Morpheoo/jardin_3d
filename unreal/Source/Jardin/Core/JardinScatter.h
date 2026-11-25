// JardinScatter.h — Rejilla espacial, reglas de colocacion y repartidor.
//
// C++ plano. Sin tipos de Unreal. Unidades en METROS.

#pragma once

#include "JardinWorld.h"
#include <vector>
#include <string>

namespace jardin
{

// ---------------------------------------------------------------- spatial grid

// COSTURA 5 — Rejilla espacial.
//
// Responde "hay algo ya colocado demasiado cerca de este punto?" sin recorrer
// la lista completa de objetos.
//
// Sin esto, colocar N objetos comparando cada candidato contra todos los
// anteriores cuesta O(N^2): con 200 objetos no se nota y con 5000 la generacion
// pasa de milisegundos a decenas de segundos. La Fase 2 mete cesped instanciado
// justamente en ese orden de magnitud.
//
// Cada entrada guarda su propio radio de exclusion, de modo que un arbol
// reserva mas espacio que una flor sin necesidad de rejillas separadas.
class SpatialGrid
{
public:
    SpatialGrid(float WorldSize, float CellSize);

    /** Group es una etiqueta libre: el repartidor usa 1 para "dosel", los
     *  arboles que dan sombra, y 0 para todo lo demas. */
    void Add(float X, float Z, float Radius, int32_t Group = 0);

    // True si ya existe una entrada cuya distancia al punto es menor que la
    // suma de ambos radios.
    bool HasNeighborWithin(float X, float Z, float Radius) const;

    // True si hay una entrada del grupo dado a menos de Distance (de centro a
    // centro). Es la consulta de la ecologia: "hay un arbol cerca?"
    bool HasGroupWithin(float X, float Z, float Distance, int32_t Group) const;

    int32_t Count() const { return static_cast<int32_t>(Xs.size()); }

    void Clear();

private:
    int32_t ColOf(float X) const;
    int32_t RowOf(float Z) const;

    float CellSize = 1.0f;
    int32_t Cols = 0;
    int32_t Rows = 0;
    float MinX = 0.0f;
    float MinZ = 0.0f;

    std::vector<std::vector<int32_t>> Cells;
    std::vector<float> Xs;
    std::vector<float> Zs;
    std::vector<float> Radii;
    std::vector<int32_t> Groups;

    // Radio mas grande almacenado. Define cuantas celdas hay que revisar.
    float MaxRadius = 0.0f;
};

// ---------------------------------------------------------------- placement

// Un punto propuesto, ya resuelto contra los campos del mundo.
struct PlacementCandidate
{
    float X = 0.0f;
    float Z = 0.0f;
    float HeightMeters = 0.0f;
    float Height01 = 0.0f;
    float Slope = 0.0f;
    float Moisture = 0.0f;
    uint8_t Zone = 0;
};

// Un objeto aceptado y listo para instanciar. Coordenadas en METROS.
struct Placement
{
    int32_t LayerIndex = 0;
    float X = 0.0f;
    float Z = 0.0f;        // segunda coordenada horizontal
    float HeightMeters = 0.0f;
    float RotationDegrees = 0.0f;
    float Scale = 1.0f;

    /** Cual de las variantes de malla de la capa le toca, en [0, VariantCount). */
    int32_t Variant = 0;
};

// COSTURA 4 — Colocacion como cadena de reglas.
//
// Cada restriccion es una clase independiente. El colocador no sabe que reglas
// existen: solo las recorre y descarta el candidato en la primera que falle.
//
// Agregar una restriccion nueva en una fase posterior es escribir una clase de
// ~10 lineas y meterla en la lista. No se toca el colocador:
//   Fase 3 -> NoWaterRule  (rechazar por debajo del nivel del agua)
//   Fase 4 -> NoPathRule   (rechazar dentro de la mascara del camino)
class IPlacementRule
{
public:
    virtual ~IPlacementRule() = default;
    virtual const char* Name() const = 0;
    virtual bool Allows(const PlacementCandidate& C, const WorldData& World) const = 0;
};

// Rechaza terreno demasiado inclinado. Evita arboles clavados en un talud.
class SlopeRule : public IPlacementRule
{
public:
    explicit SlopeRule(float InMaxSlope) : MaxSlope(InMaxSlope) {}
    const char* Name() const override { return "Pendiente"; }
    bool Allows(const PlacementCandidate& C, const WorldData&) const override
    { return C.Slope <= MaxSlope; }
private:
    float MaxSlope;
};

// Restringe la capa a ciertas zonas. Vacio significa "todas".
class AllowedZonesRule : public IPlacementRule
{
public:
    explicit AllowedZonesRule(const std::vector<uint8_t>& InAllowed) : Allowed(InAllowed) {}
    const char* Name() const override { return "Zona"; }
    bool Allows(const PlacementCandidate& C, const WorldData&) const override
    {
        if (Allowed.empty()) return true;
        for (uint8_t Z : Allowed) if (Z == C.Zone) return true;
        return false;
    }
private:
    const std::vector<uint8_t>& Allowed;
};

// Acota la altura normalizada. En la Fase 3 el mismo mecanismo, con el nivel
// del agua como minimo, es lo que impide arboles dentro del estanque.
class HeightRule : public IPlacementRule
{
public:
    HeightRule(float InMin, float InMax) : Min(InMin), Max(InMax) {}
    const char* Name() const override { return "Altura"; }
    bool Allows(const PlacementCandidate& C, const WorldData&) const override
    { return C.Height01 >= Min && C.Height01 <= Max; }
private:
    float Min, Max;
};

// Mantiene los objetos lejos del borde para que no queden cortados.
class BoundsRule : public IPlacementRule
{
public:
    explicit BoundsRule(float InMargin) : Margin(InMargin) {}
    const char* Name() const override { return "Limites"; }
    bool Allows(const PlacementCandidate& C, const WorldData& World) const override
    { return World.InsideBounds(C.X, C.Z, Margin); }
private:
    float Margin;
};

// Distancia minima contra todo lo ya colocado, consultando la rejilla.
//
// Es la unica regla con estado: lee una rejilla que crece conforme se colocan
// objetos. Por eso la rejilla se comparte entre capas, y es lo que hace que las
// rocas esquiven a los arboles colocados antes que ellas.
class MinDistanceRule : public IPlacementRule
{
public:
    MinDistanceRule(const SpatialGrid& InGrid, float InRadius) : Grid(InGrid), Radius(InRadius) {}
    const char* Name() const override { return "Distancia minima"; }
    bool Allows(const PlacementCandidate& C, const WorldData&) const override
    { return !Grid.HasNeighborWithin(C.X, C.Z, Radius); }
private:
    const SpatialGrid& Grid;
    float Radius;
};

// Mascara de densidad por ruido: en vez de repartir uniformemente, agrupa.
// Es la diferencia entre un bosque que parece bosque y arboles espolvoreados
// al azar. El ruido crea claros y espesuras sin reglas adicionales.
//
// Clustering mezcla dos comportamientos:
//   0 (Fase 0)  probabilidad proporcional a la mascara MAS un piso constante.
//               Ese piso pone objetos sueltos en todo el mapa: es el
//               "salpicado" que hace que todo se vea aleatorio.
//   1           manchas con borde definido: dentro de la mancha se acepta
//               todo, fuera casi nada. Asi se ven arboledas y praderas.
// En ambos casos se consume exactamente un numero aleatorio por consulta.
class DensityMaskRule : public IPlacementRule
{
public:
    DensityMaskRule(uint32_t InSeed, float InFrequency, float InThreshold, float InClustering,
                    DeterministicRandom& InRng)
        : Seed(InSeed), Frequency(InFrequency), Threshold(InThreshold), Clustering(InClustering), Rng(InRng) {}

    const char* Name() const override { return "Mascara de densidad"; }

    /** El valor de la mascara en un punto. Publico para poder verificarlo en las pruebas. */
    float MaskAt(float X, float Z, const WorldData& World) const
    {
        const float u = (X + World.WorldSize * 0.5f) / World.WorldSize;
        const float v = (Z + World.WorldSize * 0.5f) / World.WorldSize;
        return noise::Fbm01(u, v, Seed, 3, Frequency, 2.0f, 0.5f);
    }

    /** Mitad del ancho del borde de una mancha, en unidades de mascara. */
    static constexpr float EdgeHalfWidth = 0.06f;

    bool Allows(const PlacementCandidate& C, const WorldData& World) const override
    {
        const float Mask = MaskAt(C.X, C.Z, World);

        // El umbral desplaza la mascara: 0 acepta casi todo, 1 casi nada.
        float P = Mask * (1.0f - Threshold) + (1.0f - Threshold) * 0.25f;

        if (Clustering > 0.0f)
        {
            float t = (Mask - (Threshold - EdgeHalfWidth)) / (2.0f * EdgeHalfWidth);
            t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
            const float Edge = t * t * (3.0f - 2.0f * t);
            P = P + (Edge - P) * Clustering;
        }

        return Rng.NextFloat() < P;
    }
private:
    uint32_t Seed;
    float Frequency;
    float Threshold;
    float Clustering;
    DeterministicRandom& Rng;
};

// Rango de humedad tolerado por la especie. Como la humedad es un campo
// continuo, las especies se reemplazan de forma gradual al cruzar el mapa en
// vez de mezclarse por igual en todas partes.
class MoistureRule : public IPlacementRule
{
public:
    MoistureRule(float InMin, float InMax) : Min(InMin), Max(InMax) {}
    const char* Name() const override { return "Humedad"; }
    bool Allows(const PlacementCandidate& C, const WorldData&) const override
    { return C.Moisture >= Min && C.Moisture <= Max; }
private:
    float Min, Max;
};

// Exige un vecino de cierto grupo: el sotobosque (helechos, hongos) solo
// crece a la sombra de un arbol ya colocado.
class NearGroupRule : public IPlacementRule
{
public:
    NearGroupRule(const SpatialGrid& InGrid, int32_t InGroup, float InDistance)
        : Grid(InGrid), Group(InGroup), Distance(InDistance) {}
    const char* Name() const override { return "Cerca de grupo"; }
    bool Allows(const PlacementCandidate& C, const WorldData&) const override
    { return Grid.HasGroupWithin(C.X, C.Z, Distance, Group); }
private:
    const SpatialGrid& Grid;
    int32_t Group;
    float Distance;
};

// Prohibe un vecino de cierto grupo: flores y pasto buscan el sol, asi que
// se quedan en los claros, lejos de los arboles.
class AvoidGroupRule : public IPlacementRule
{
public:
    AvoidGroupRule(const SpatialGrid& InGrid, int32_t InGroup, float InDistance)
        : Grid(InGrid), Group(InGroup), Distance(InDistance) {}
    const char* Name() const override { return "Lejos de grupo"; }
    bool Allows(const PlacementCandidate& C, const WorldData&) const override
    { return !Grid.HasGroupWithin(C.X, C.Z, Distance, Group); }
private:
    const SpatialGrid& Grid;
    int32_t Group;
    float Distance;
};

// Nada dentro del agua ni pegado a la orilla. Lee la distancia al agua que
// calculo el Priority-Flood; solo entra en la cadena si el mundo tiene agua.
class NoWaterRule : public IPlacementRule
{
public:
    explicit NoWaterRule(float InMinDistance) : MinDistance(InMinDistance) {}
    const char* Name() const override { return "Agua"; }
    bool Allows(const PlacementCandidate& C, const WorldData& World) const override
    { return World.SampleField(World.WaterDistance, C.X, C.Z) >= MinDistance; }
private:
    float MinDistance;
};

// Nada sobre el camino ni pegado a el. Lee la distancia al eje que calculo el
// generador del camino; solo entra en la cadena si el mundo tiene camino.
class NoPathRule : public IPlacementRule
{
public:
    NoPathRule(float InHalfWidth, float InMinDistance) : Clearance(InHalfWidth + InMinDistance) {}
    const char* Name() const override { return "Camino"; }
    bool Allows(const PlacementCandidate& C, const WorldData& World) const override
    { return World.SampleField(World.PathDistance, C.X, C.Z) >= Clearance; }
private:
    float Clearance;
};

// ---------------------------------------------------------------- layers

// Configuracion de una familia de objetos: arboles, rocas, arbustos...
struct PropLayer
{
    std::string Name = "Arboles";
    int32_t RuntimeIndex = 0;

    // Separacion nominal de la rejilla de candidatos, en metros.
    // Es el techo de densidad: nunca habra mas de un objeto por celda.
    float Spacing = 5.0f;

    // Espacio que reserva el objeto.
    float ExclusionRadius = 2.2f;

    float MaxSlope = 0.35f;
    float MinHeight01 = 0.02f;
    float MaxHeight01 = 1.0f;

    // Zonas donde puede aparecer. Vacio significa todas.
    std::vector<uint8_t> AllowedZones;

    // Frecuencia de la mascara de densidad. Baja agrupa en manchas grandes;
    // alta produce un moteado fino.
    float DensityFrequency = 2.5f;

    // 0 acepta casi todo, 1 casi nada.
    float DensityThreshold = 0.35f;

    float BoundsMargin = 2.0f;

    float MinScale = 0.85f;
    float MaxScale = 1.25f;

    // Tope duro. Red de seguridad contra una configuracion accidental.
    int32_t MaxCount = 20000;

    /** Cuantas mallas distintas puede usar la capa (los 5 pinos del kit, por
     *  ejemplo). El nucleo no sabe que mallas son: solo reparte indices, y la
     *  capa del motor traduce cada indice a su malla. */
    int32_t VariantCount = 1;

    // --- ecologia (valores por defecto = comportamiento de la Fase 0) ---

    /** Rango de humedad tolerado. [0, 1] no restringe nada. */
    float MinMoisture = 0.0f;
    float MaxMoisture = 1.0f;

    /** 0 = salpicado de la Fase 0; 1 = manchas con borde definido. */
    float Clustering = 0.0f;

    /** Etiqueta que dejan los objetos de esta capa en la rejilla. 1 = dosel. */
    int32_t Group = 0;

    /** Si es >= 0, cada objeto necesita un vecino de ese grupo a menos de NearDistance. */
    int32_t NearGroup = -1;
    float NearDistance = 0.0f;

    /** Si es >= 0, ningun objeto puede tener un vecino de ese grupo a menos de AvoidDistance. */
    int32_t AvoidGroup = -1;
    float AvoidDistance = 0.0f;

    /** Distancia minima a la orilla, en metros. Solo cuenta si hay agua. */
    float MinWaterDistance = 1.0f;

    // Distancia minima al BORDE del camino, en metros (Fase 4).
    float MinPathDistance = 0.5f;
};

struct ScatterResult
{
    std::vector<Placement> Placements;
    std::vector<int32_t> CountPerLayer;
    int32_t CandidatesTried = 0;
    double ElapsedMs = 0.0;

    int32_t Total() const { return static_cast<int32_t>(Placements.size()); }
};

// Coloca objetos sobre el terreno respetando las reglas.
//
// Muestreo: rejilla con perturbacion (jittered grid) mas rechazo. Se recorre
// una rejilla regular y dentro de cada celda se propone un punto desplazado al
// azar. Es mas barato que un Poisson disk completo y, combinado con la regla de
// distancia minima, produce una distribucion sin patron reconocible.
//
// ORDEN: las capas se procesan de mayor a menor radio de exclusion. Los objetos
// grandes se colocan primero y los pequenos esquivan lo ya puesto. Al reves,
// las flores ocuparian el terreno y los arboles no cabrian en ninguna parte.
namespace Scatterer
{
    ScatterResult Scatter(const WorldData& World, std::vector<PropLayer>& Layers, const SeedBook& Seeds);

    // Las capas comprometidas en el protocolo: dos arboles y una roca.
    std::vector<PropLayer> DefaultLayers();
}

} // namespace jardin
