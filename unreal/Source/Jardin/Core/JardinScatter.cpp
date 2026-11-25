// JardinScatter.cpp — Rejilla espacial y repartidor de objetos.

#include "JardinScatter.h"
#include <chrono>

namespace jardin
{

// ---------------------------------------------------------------- spatial grid

SpatialGrid::SpatialGrid(float WorldSize, float InCellSize)
{
    if (InCellSize < 0.1f) InCellSize = 0.1f;

    CellSize = InCellSize;
    MinX = -WorldSize * 0.5f;
    MinZ = -WorldSize * 0.5f;

    Cols = static_cast<int32_t>(WorldSize / CellSize) + 2;
    Rows = Cols;

    Cells.resize(static_cast<size_t>(Cols) * static_cast<size_t>(Rows));
}

int32_t SpatialGrid::ColOf(float X) const
{
    int32_t c = static_cast<int32_t>((X - MinX) / CellSize);
    return c < 0 ? 0 : (c >= Cols ? Cols - 1 : c);
}

int32_t SpatialGrid::RowOf(float Z) const
{
    int32_t r = static_cast<int32_t>((Z - MinZ) / CellSize);
    return r < 0 ? 0 : (r >= Rows ? Rows - 1 : r);
}

void SpatialGrid::Add(float X, float Z, float Radius, int32_t Group)
{
    const int32_t Index = static_cast<int32_t>(Xs.size());
    Xs.push_back(X);
    Zs.push_back(Z);
    Radii.push_back(Radius);
    Groups.push_back(Group);

    if (Radius > MaxRadius) MaxRadius = Radius;

    const size_t Cell = static_cast<size_t>(RowOf(Z)) * static_cast<size_t>(Cols)
                      + static_cast<size_t>(ColOf(X));
    Cells[Cell].push_back(Index);
}

bool SpatialGrid::HasNeighborWithin(float X, float Z, float Radius) const
{
    // Hay que revisar hasta donde pueda alcanzar el objeto mas grande ya
    // colocado, no solo el radio del candidato: un arbol enorme a tres celdas
    // de distancia todavia puede invadir este punto.
    const float Reach = Radius + MaxRadius;
    const int32_t Span = static_cast<int32_t>(Reach / CellSize) + 1;

    const int32_t C0 = ColOf(X);
    const int32_t R0 = RowOf(Z);

    int32_t CMin = C0 - Span, CMax = C0 + Span;
    int32_t RMin = R0 - Span, RMax = R0 + Span;

    if (CMin < 0) CMin = 0;
    if (RMin < 0) RMin = 0;
    if (CMax >= Cols) CMax = Cols - 1;
    if (RMax >= Rows) RMax = Rows - 1;

    for (int32_t r = RMin; r <= RMax; ++r)
    {
        const size_t RowBase = static_cast<size_t>(r) * static_cast<size_t>(Cols);
        for (int32_t c = CMin; c <= CMax; ++c)
        {
            const std::vector<int32_t>& Bucket = Cells[RowBase + static_cast<size_t>(c)];
            for (int32_t e : Bucket)
            {
                const float dx = Xs[e] - X;
                const float dz = Zs[e] - Z;
                const float MinDist = Radii[e] + Radius;

                // Se comparan cuadrados para no calcular raices cuadradas.
                if (dx * dx + dz * dz < MinDist * MinDist) return true;
            }
        }
    }

    return false;
}

bool SpatialGrid::HasGroupWithin(float X, float Z, float Distance, int32_t Group) const
{
    const int32_t Span = static_cast<int32_t>(Distance / CellSize) + 1;

    const int32_t C0 = ColOf(X);
    const int32_t R0 = RowOf(Z);

    int32_t CMin = C0 - Span, CMax = C0 + Span;
    int32_t RMin = R0 - Span, RMax = R0 + Span;

    if (CMin < 0) CMin = 0;
    if (RMin < 0) RMin = 0;
    if (CMax >= Cols) CMax = Cols - 1;
    if (RMax >= Rows) RMax = Rows - 1;

    const float D2 = Distance * Distance;

    for (int32_t r = RMin; r <= RMax; ++r)
    {
        const size_t RowBase = static_cast<size_t>(r) * static_cast<size_t>(Cols);
        for (int32_t c = CMin; c <= CMax; ++c)
        {
            for (int32_t e : Cells[RowBase + static_cast<size_t>(c)])
            {
                if (Groups[e] != Group) continue;
                const float dx = Xs[e] - X;
                const float dz = Zs[e] - Z;
                if (dx * dx + dz * dz < D2) return true;
            }
        }
    }

    return false;
}

void SpatialGrid::Clear()
{
    for (auto& Bucket : Cells) Bucket.clear();
    Xs.clear();
    Zs.clear();
    Radii.clear();
    Groups.clear();
    MaxRadius = 0.0f;
}

// ---------------------------------------------------------------- scatterer

namespace
{
    // Ordena por radio descendente con insercion, que es estable.
    // std::sort NO es estable: dos capas con el mismo radio podrian
    // intercambiarse entre ejecuciones y cambiar el mundo.
    std::vector<PropLayer*> OrderByRadiusDescending(std::vector<PropLayer>& Layers)
    {
        std::vector<PropLayer*> A;
        A.reserve(Layers.size());
        for (PropLayer& L : Layers) A.push_back(&L);

        for (size_t i = 1; i < A.size(); ++i)
        {
            PropLayer* Key = A[i];
            size_t j = i;
            while (j > 0 && A[j - 1]->ExclusionRadius < Key->ExclusionRadius)
            {
                A[j] = A[j - 1];
                --j;
            }
            A[j] = Key;
        }

        return A;
    }

    void ScatterLayer(const WorldData& World, PropLayer& Layer, const SeedBook& Seeds,
                      SpatialGrid& Grid, ScatterResult& Result)
    {
        const float Spacing = Layer.Spacing < 0.25f ? 0.25f : Layer.Spacing;

        // Una subsemilla por capa: agregar una capa nueva no desplaza a las demas.
        const uint32_t LayerSeed = hash32::Combine(Seeds.For(SeedBook::Scatter),
                                                   hash32::Text(Layer.Name.c_str()));
        const uint32_t VarSeed = hash32::Combine(Seeds.For(SeedBook::Variation),
                                                 hash32::Text(Layer.Name.c_str()));

        DeterministicRandom PlaceRng(LayerSeed);
        DeterministicRandom VarRng(VarSeed);

        // Las reglas van de la mas barata a la mas cara: la mayoria de
        // candidatos muere en pendiente o zona y nunca consulta la rejilla.
        //
        // El orden forma parte de la definicion del mundo: cambiarlo cambia que
        // candidatos consumen numeros aleatorios, y por tanto el resultado.
        const BoundsRule        RBounds(Layer.BoundsMargin);
        const SlopeRule         RSlope(Layer.MaxSlope);
        const HeightRule        RHeight(Layer.MinHeight01, Layer.MaxHeight01);
        const MoistureRule      RMoisture(Layer.MinMoisture, Layer.MaxMoisture);
        const AllowedZonesRule  RZones(Layer.AllowedZones);
        const DensityMaskRule   RDensity(LayerSeed, Layer.DensityFrequency,
                                         Layer.DensityThreshold, Layer.Clustering, PlaceRng);
        const NearGroupRule     RNear(Grid, Layer.NearGroup, Layer.NearDistance);
        const AvoidGroupRule    RAvoid(Grid, Layer.AvoidGroup, Layer.AvoidDistance);
        const MinDistanceRule   RDistance(Grid, Layer.ExclusionRadius);
        const NoWaterRule       RWater(Layer.MinWaterDistance);
        const NoPathRule        RPath(World.Path.Width * 0.5f, Layer.MinPathDistance);

        // Las reglas de ecologia solo entran en la cadena si estan activas. Una
        // capa configurada como en la Fase 0 recorre exactamente la misma
        // cadena que entonces y produce el mismo resultado.
        const bool bMoisture = Layer.MinMoisture > 0.0f || Layer.MaxMoisture < 1.0f;

        const IPlacementRule* Rules[12];
        int32_t RuleCount = 0;
        Rules[RuleCount++] = &RBounds;
        Rules[RuleCount++] = &RSlope;
        Rules[RuleCount++] = &RHeight;
        // Antes de la mascara de densidad, que consume numeros aleatorios: sin
        // agua la cadena es la misma de siempre.
        if (World.HasWater())       Rules[RuleCount++] = &RWater;
        if (World.HasPath())        Rules[RuleCount++] = &RPath;
        if (bMoisture)              Rules[RuleCount++] = &RMoisture;
        Rules[RuleCount++] = &RZones;
        Rules[RuleCount++] = &RDensity;
        if (Layer.NearGroup >= 0)   Rules[RuleCount++] = &RNear;
        if (Layer.AvoidGroup >= 0)  Rules[RuleCount++] = &RAvoid;
        Rules[RuleCount++] = &RDistance;

        const float Half = World.WorldSize * 0.5f;
        const int32_t CellsPerSide = static_cast<int32_t>(World.WorldSize / Spacing);
        int32_t Placed = 0;

        for (int32_t cz = 0; cz < CellsPerSide; ++cz)
        {
            for (int32_t cx = 0; cx < CellsPerSide; ++cx)
            {
                if (Placed >= Layer.MaxCount) return;

                // Punto perturbado dentro de la celda.
                const float X = -Half + (static_cast<float>(cx) + PlaceRng.NextFloat()) * Spacing;
                const float Z = -Half + (static_cast<float>(cz) + PlaceRng.NextFloat()) * Spacing;

                PlacementCandidate C;
                C.X = X;
                C.Z = Z;
                C.Height01 = World.SampleField(World.Height, X, Z);
                C.Slope = World.SampleSlope(X, Z);
                C.Moisture = World.SampleMoisture(X, Z);
                C.Zone = World.SampleZone(X, Z);
                C.HeightMeters = C.Height01 * World.MaxHeight;

                ++Result.CandidatesTried;

                bool bAllowed = true;
                for (int32_t r = 0; r < RuleCount; ++r)
                {
                    if (!Rules[r]->Allows(C, World)) { bAllowed = false; break; }
                }
                if (!bAllowed) continue;

                Grid.Add(X, Z, Layer.ExclusionRadius, Layer.Group);

                Placement P;
                P.LayerIndex = Layer.RuntimeIndex;
                P.X = X;
                P.Z = Z;
                P.HeightMeters = C.HeightMeters;
                P.RotationDegrees = VarRng.Range(0.0f, 360.0f);
                P.Scale = VarRng.Range(Layer.MinScale, Layer.MaxScale);

                // La variante sale de un hash y NO de VarRng. Si consumiera
                // numeros de VarRng, agregar variantes a una capa cambiaria los
                // giros y escalas de todos sus objetos. Asi, pasar de 1 a 5
                // pinos solo cambia que pino aparece en cada sitio.
                if (Layer.VariantCount > 1)
                {
                    const uint32_t H = hash32::Combine(VarSeed ^ 0x51ED270Bu, static_cast<uint32_t>(Placed));
                    P.Variant = static_cast<int32_t>(H % static_cast<uint32_t>(Layer.VariantCount));
                }

                Result.Placements.push_back(P);
                Result.CountPerLayer[Layer.RuntimeIndex]++;
                ++Placed;
            }
        }
    }
}

ScatterResult Scatterer::Scatter(const WorldData& World, std::vector<PropLayer>& Layers,
                                 const SeedBook& Seeds)
{
    ScatterResult Result;

    if (Layers.empty()) return Result;

    const auto Start = std::chrono::high_resolution_clock::now();

    Result.CountPerLayer.assign(Layers.size(), 0);

    for (size_t i = 0; i < Layers.size(); ++i)
        Layers[i].RuntimeIndex = static_cast<int32_t>(i);

    // La rejilla se comparte entre capas: asi las rocas ven a los arboles.
    // Su celda se dimensiona con el radio mas grande declarado.
    float MaxRadius = 0.5f;
    for (const PropLayer& L : Layers)
        if (L.ExclusionRadius > MaxRadius) MaxRadius = L.ExclusionRadius;

    SpatialGrid Grid(World.WorldSize, MaxRadius * 2.0f);

    std::vector<PropLayer*> Ordered = OrderByRadiusDescending(Layers);

    for (PropLayer* L : Ordered)
        ScatterLayer(World, *L, Seeds, Grid, Result);

    const auto End = std::chrono::high_resolution_clock::now();
    Result.ElapsedMs = std::chrono::duration<double, std::milli>(End - Start).count();

    return Result;
}

std::vector<PropLayer> Scatterer::DefaultLayers()
{
    std::vector<PropLayer> Layers;

    PropLayer ArbolA;
    ArbolA.Name = "ArbolA";
    ArbolA.Spacing = 4.5f;  ArbolA.ExclusionRadius = 2.4f;
    ArbolA.MaxSlope = 0.32f; ArbolA.MinHeight01 = 0.05f; ArbolA.MaxHeight01 = 0.85f;
    ArbolA.AllowedZones = { ZoneSet::Bosque };
    ArbolA.DensityFrequency = 2.2f; ArbolA.DensityThreshold = 0.20f;
    ArbolA.MinScale = 0.85f; ArbolA.MaxScale = 1.30f;

    PropLayer ArbolB;
    ArbolB.Name = "ArbolB";
    ArbolB.Spacing = 7.0f;  ArbolB.ExclusionRadius = 2.0f;
    ArbolB.MaxSlope = 0.30f; ArbolB.MinHeight01 = 0.05f; ArbolB.MaxHeight01 = 0.75f;
    ArbolB.AllowedZones = { ZoneSet::Bosque, ZoneSet::Pradera };
    ArbolB.DensityFrequency = 3.0f; ArbolB.DensityThreshold = 0.55f;
    ArbolB.MinScale = 0.80f; ArbolB.MaxScale = 1.20f;

    PropLayer Roca;
    Roca.Name = "Roca";
    Roca.Spacing = 3.5f;   Roca.ExclusionRadius = 0.9f;
    Roca.MaxSlope = 0.60f; Roca.MinHeight01 = 0.02f; Roca.MaxHeight01 = 1.0f;
    Roca.DensityFrequency = 4.0f; Roca.DensityThreshold = 0.62f;
    Roca.MinScale = 0.60f; Roca.MaxScale = 1.60f;

    Layers.push_back(ArbolA);
    Layers.push_back(ArbolB);
    Layers.push_back(Roca);
    return Layers;
}

} // namespace jardin
