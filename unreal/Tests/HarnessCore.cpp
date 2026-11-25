// Arnes de pruebas del nucleo C++. Se compila con cl.exe, sin Unreal.

#include "JardinScatter.h"
#include "JardinWater.h"
#include "JardinPath.h"
#include "JardinGrass.h"
#include <algorithm>
#include <cstdio>
#include <cmath>

using namespace jardin;

static int failures = 0;

static void Check(bool ok, const char* label, const char* detail = nullptr)
{
    printf("  %s  %s", ok ? "PASA " : "FALLA", label);
    if (detail) printf("  -> %s", detail);
    printf("\n");
    if (!ok) ++failures;
}

static uint32_t Fingerprint(const ScatterResult& R)
{
    uint32_t h = 2166136261u;
    for (const Placement& P : R.Placements)
    {
        h = hash32::Combine(h, static_cast<uint32_t>(P.X * 1000.0f + 100000.0f));
        h = hash32::Combine(h, static_cast<uint32_t>(P.Z * 1000.0f + 100000.0f));
        h = hash32::Combine(h, static_cast<uint32_t>(P.RotationDegrees * 100.0f));
        h = hash32::Combine(h, static_cast<uint32_t>(P.Scale * 1000.0f));
        h = hash32::Combine(h, static_cast<uint32_t>(P.LayerIndex));
    }
    return h;
}

int main()
{
    const ZoneSet Zones = ZoneSet::Default();
    GenerationSettings S;
    S.Seed = 827461;

    char buf[256];

    // ---- LA PRUEBA QUE IMPORTA ----
    // Valores obtenidos de la version C# ya verificada.
    // Si coinciden, el port a C++ es exacto, bit a bit.
    printf("=== A. Paridad con la version C# ===\n");
    const uint32_t CsWorldChecksum = 0x64C68E3Du;
    const uint32_t CsScatterPrint  = 0x427FD6EDu;

    WorldData W = WorldGenerator::Generate(S, Zones);
    const uint32_t CppChecksum = W.Checksum();

    snprintf(buf, sizeof(buf), "C# %08X   C++ %08X", CsWorldChecksum, CppChecksum);
    Check(CppChecksum == CsWorldChecksum, "el mundo generado es identico al de C#", buf);

    std::vector<PropLayer> Layers = Scatterer::DefaultLayers();
    const SeedBook Seeds(W.MasterSeed);
    ScatterResult R = Scatterer::Scatter(W, Layers, Seeds);
    const uint32_t CppPrint = Fingerprint(R);

    snprintf(buf, sizeof(buf), "C# %08X   C++ %08X", CsScatterPrint, CppPrint);
    Check(CppPrint == CsScatterPrint, "la colocacion es identica a la de C#", buf);

    snprintf(buf, sizeof(buf), "C# 67/71/285   C++ %d/%d/%d",
             R.CountPerLayer[0], R.CountPerLayer[1], R.CountPerLayer[2]);
    Check(R.CountPerLayer[0] == 67 && R.CountPerLayer[1] == 71 && R.CountPerLayer[2] == 285,
          "los conteos por capa coinciden", buf);

    // ---- determinismo interno ----
    printf("=== B. Determinismo ===\n");
    const uint32_t c2 = WorldGenerator::Generate(S, Zones).Checksum();
    const uint32_t c3 = WorldGenerator::Generate(S, Zones).Checksum();
    Check(CppChecksum == c2 && c2 == c3, "misma semilla -> mismo mundo, 3 veces");

    GenerationSettings S2 = S; S2.Seed = 827462;
    WorldData W2 = WorldGenerator::Generate(S2, Zones);
    Check(W2.Checksum() != CppChecksum, "semillas distintas -> mundos distintos");

    // ---- rangos ----
    printf("=== C. Rangos de los campos ===\n");
    float hMin = 2.0f, hMax = -1.0f;
    int outOfRange = 0;
    for (size_t i = 0; i < W.Height.size(); ++i)
    {
        const float h = W.Height[i], sl = W.Slope[i], m = W.Moisture[i];
        if (h < 0 || h > 1 || sl < 0 || sl > 1 || m < 0 || m > 1) ++outOfRange;
        if (h < hMin) hMin = h;
        if (h > hMax) hMax = h;
    }
    snprintf(buf, sizeof(buf), "%d fuera de rango", outOfRange);
    Check(outOfRange == 0, "todos los campos en [0,1]", buf);
    snprintf(buf, sizeof(buf), "altura %.3f .. %.3f", hMin, hMax);
    Check(hMax - hMin > 0.25f, "el relieve tiene rango util", buf);

    // ---- reparto de zonas ----
    printf("=== D. Zonas ===\n");
    int pradera = 0, bosque = 0, otras = 0;
    for (uint8_t z : W.Zone)
    {
        if (z == ZoneSet::Pradera) ++pradera;
        else if (z == ZoneSet::Bosque) ++bosque;
        else ++otras;
    }
    const float total = static_cast<float>(W.Zone.size());
    snprintf(buf, sizeof(buf), "pradera %.1f%%  bosque %.1f%%",
             100.0f * pradera / total, 100.0f * bosque / total);
    Check(otras == 0 && pradera > total * 0.1f && bosque > total * 0.1f,
          "reparto de zonas razonable", buf);

    // ---- solapamientos ----
    printf("=== E. Sin solapamientos ===\n");
    int overlaps = 0;
    for (size_t i = 0; i < R.Placements.size(); ++i)
    {
        for (size_t j = i + 1; j < R.Placements.size(); ++j)
        {
            const float dx = R.Placements[i].X - R.Placements[j].X;
            const float dz = R.Placements[i].Z - R.Placements[j].Z;
            const float need = Layers[R.Placements[i].LayerIndex].ExclusionRadius
                             + Layers[R.Placements[j].LayerIndex].ExclusionRadius;
            if (dx * dx + dz * dz < need * need) ++overlaps;
        }
    }
    snprintf(buf, sizeof(buf), "%d solapes", overlaps);
    Check(overlaps == 0, "ningun par de objetos se solapa", buf);

    // ---- reglas ----
    printf("=== F. Las reglas se respetan ===\n");
    int badSlope = 0, badZone = 0, badBounds = 0;
    for (const Placement& P : R.Placements)
    {
        const PropLayer& L = Layers[P.LayerIndex];
        if (W.SampleSlope(P.X, P.Z) > L.MaxSlope + 0.0001f) ++badSlope;
        if (!W.InsideBounds(P.X, P.Z, L.BoundsMargin)) ++badBounds;
        if (!L.AllowedZones.empty())
        {
            bool ok = false;
            const uint8_t z = W.SampleZone(P.X, P.Z);
            for (uint8_t az : L.AllowedZones) if (az == z) ok = true;
            if (!ok) ++badZone;
        }
    }
    Check(badSlope == 0, "ninguno en pendiente excesiva");
    Check(badZone == 0, "ninguno fuera de su zona permitida");
    Check(badBounds == 0, "ninguno fuera de los limites");

    // ---- escala independiente de resolucion ----
    printf("=== G. Escala independiente de la resolucion ===\n");
    GenerationSettings Lo = S; Lo.Resolution = 64;
    GenerationSettings Hi = S; Hi.Resolution = 192;
    WorldData WLo = WorldGenerator::Generate(Lo, Zones);
    WorldData WHi = WorldGenerator::Generate(Hi, Zones);
    const float d = std::fabs(WLo.SampleHeightMeters(10.0f, -20.0f)
                            - WHi.SampleHeightMeters(10.0f, -20.0f));
    snprintf(buf, sizeof(buf), "diferencia %.3f m", d);
    Check(d < 1.0f, "el mismo seed da el mismo paisaje a otra resolucion", buf);

    // ---- estres ----
    printf("=== H. Estres: densidad de Fase 2 ===\n");
    std::vector<PropLayer> Dense = Scatterer::DefaultLayers();
    PropLayer Cesped;
    Cesped.Name = "Cesped";
    Cesped.Spacing = 1.0f; Cesped.ExclusionRadius = 0.25f;
    Cesped.MaxSlope = 0.5f; Cesped.MinHeight01 = 0.02f; Cesped.MaxHeight01 = 1.0f;
    Cesped.DensityFrequency = 3.0f; Cesped.DensityThreshold = 0.1f; Cesped.MaxCount = 60000;
    Dense.push_back(Cesped);

    ScatterResult Big = Scatterer::Scatter(W, Dense, Seeds);
    snprintf(buf, sizeof(buf), "%d objetos en %.1f ms", Big.Total(), Big.ElapsedMs);
    Check(Big.Total() > 3000 && Big.ElapsedMs < 2000.0, "la rejilla aguanta la Fase 2", buf);

    printf("=== J. Variantes de malla ===\n");
    {
        std::vector<PropLayer> One = Scatterer::DefaultLayers();
        std::vector<PropLayer> Five = Scatterer::DefaultLayers();
        Five[0].VariantCount = 5;
        const ScatterResult R1 = Scatterer::Scatter(W, One, Seeds);
        const ScatterResult R5 = Scatterer::Scatter(W, Five, Seeds);

        bool same = R1.Placements.size() == R5.Placements.size();
        int counts[5] = { 0, 0, 0, 0, 0 };
        int badRange = 0;
        for (size_t i = 0; same && i < R1.Placements.size(); ++i)
        {
            const Placement& a = R1.Placements[i];
            const Placement& b = R5.Placements[i];
            if (a.X != b.X || a.Z != b.Z || a.RotationDegrees != b.RotationDegrees
                || a.Scale != b.Scale || a.LayerIndex != b.LayerIndex) same = false;
            if (b.LayerIndex == 0)
            {
                if (b.Variant < 0 || b.Variant >= 5) ++badRange; else ++counts[b.Variant];
            }
            else if (b.Variant != 0) ++badRange;
        }
        Check(same, "agregar variantes no mueve ni gira ningun objeto");
        Check(badRange == 0, "cada objeto recibe una variante valida");
        snprintf(buf, sizeof(buf), "%d / %d / %d / %d / %d", counts[0], counts[1], counts[2], counts[3], counts[4]);
        Check(counts[0] > 0 && counts[1] > 0 && counts[2] > 0 && counts[3] > 0 && counts[4] > 0,
              "las 5 variantes aparecen", buf);
        Check(Fingerprint(R1) == CsScatterPrint, "con 1 variante sigue coincidiendo con C#");
    }

    printf("=== K. Relieve: domain warping y crestas ===\n");
    {
        GenerationSettings Rel = S;
        Rel.WarpStrength = 0.05f; Rel.WarpFrequency = 3.0f;
        Rel.RidgeWeight = 0.75f;  Rel.MountainFrequency = 2.0f;
        Rel.MaxHeight = 30.0f;

        const WorldData WR1 = WorldGenerator::Generate(Rel, Zones);
        const WorldData WR2 = WorldGenerator::Generate(Rel, Zones);
        snprintf(buf, sizeof(buf), "%08X / %08X", WR1.Checksum(), WR2.Checksum());
        Check(WR1.Checksum() == WR2.Checksum(), "el relieve nuevo tambien es determinista", buf);
        Check(WR1.Checksum() != CsWorldChecksum, "y produce un terreno distinto al de la Fase 0");

        int out = 0; float slopeMax0 = 0.0f, slopeMaxR = 0.0f;
        for (size_t i = 0; i < WR1.Height.size(); ++i)
        {
            if (WR1.Height[i] < 0 || WR1.Height[i] > 1) ++out;
            if (W.Slope[i] > slopeMax0) slopeMax0 = W.Slope[i];
            if (WR1.Slope[i] > slopeMaxR) slopeMaxR = WR1.Slope[i];
        }
        Check(out == 0, "la altura sigue en [0,1]");
        snprintf(buf, sizeof(buf), "pendiente maxima %.3f -> %.3f", slopeMax0, slopeMaxR);
        Check(slopeMaxR > slopeMax0 * 1.2f, "las crestas producen laderas mas empinadas", buf);

        GenerationSettings Mes = Rel;
        Mes.TerraceWeight = 0.7f; Mes.TerraceSteps = 6.0f; Mes.TerraceFrequency = 2.5f;
        const uint32_t M1 = WorldGenerator::Generate(Mes, Zones).Checksum();
        const uint32_t M2 = WorldGenerator::Generate(Mes, Zones).Checksum();
        snprintf(buf, sizeof(buf), "%08X / %08X", M1, M2);
        Check(M1 == M2 && M1 != WR1.Checksum(), "las mesetas son deterministas y cambian el terreno", buf);

        // Con todo en cero, el camino de codigo nuevo no altera nada.
        GenerationSettings Z0 = S;
        Z0.WarpFrequency = 7.0f; Z0.MountainFrequency = 5.0f;   // sin efecto si fuerza y peso son 0
        Check(WorldGenerator::Generate(Z0, Zones).Checksum() == CsWorldChecksum,
              "con fuerza y peso en 0, sigue dando 64C68E3D");
    }

    printf("=== L. Ecologia: dosel, sotobosque, claros, humedad, manchas ===\n");
    {
        std::vector<PropLayer> Eco;

        PropLayer Arbol;
        Arbol.Name = "EcoArbol"; Arbol.Spacing = 5.0f; Arbol.ExclusionRadius = 2.0f;
        Arbol.MaxSlope = 0.5f; Arbol.DensityFrequency = 3.0f; Arbol.DensityThreshold = 0.30f;
        Arbol.Group = 1;
        Eco.push_back(Arbol);

        PropLayer Helecho;   // solo bajo los arboles
        Helecho.Name = "EcoHelecho"; Helecho.Spacing = 2.0f; Helecho.ExclusionRadius = 0.4f;
        Helecho.MaxSlope = 0.5f; Helecho.DensityThreshold = 0.1f;
        Helecho.NearGroup = 1; Helecho.NearDistance = 4.0f;
        Eco.push_back(Helecho);

        PropLayer Flor;      // solo en los claros, en manchas, con humedad media
        Flor.Name = "EcoFlor"; Flor.Spacing = 1.5f; Flor.ExclusionRadius = 0.3f;
        Flor.MaxSlope = 0.5f; Flor.DensityFrequency = 6.0f; Flor.DensityThreshold = 0.5f;
        Flor.Clustering = 1.0f; Flor.AvoidGroup = 1; Flor.AvoidDistance = 5.0f;
        Flor.MinMoisture = 0.40f; Flor.MaxMoisture = 0.65f;
        Eco.push_back(Flor);

        const ScatterResult E1 = Scatterer::Scatter(W, Eco, Seeds);
        const ScatterResult E2 = Scatterer::Scatter(W, Eco, Seeds);
        Check(Fingerprint(E1) == Fingerprint(E2), "la ecologia es determinista");
        snprintf(buf, sizeof(buf), "arboles %d, helechos %d, flores %d",
                 E1.CountPerLayer[0], E1.CountPerLayer[1], E1.CountPerLayer[2]);
        Check(E1.CountPerLayer[0] > 0 && E1.CountPerLayer[1] > 0 && E1.CountPerLayer[2] > 0,
              "las tres capas colocan algo", buf);

        std::vector<const Placement*> trees;
        for (const Placement& P : E1.Placements) if (P.LayerIndex == 0) trees.push_back(&P);

        auto nearestTree = [&](float X, float Z) {
            float best = 1e9f;
            for (const Placement* T : trees)
            {
                const float dx = T->X - X, dz = T->Z - Z;
                const float d = std::sqrt(dx * dx + dz * dz);
                if (d < best) best = d;
            }
            return best;
        };

        int fernFar = 0, flowerShade = 0, flowerDry = 0, flowerOutside = 0;
        // Se reconstruye la mascara de las flores para comprobar que ninguna
        // quedo fuera de sus manchas. El generador no se usa: solo MaskAt.
        DeterministicRandom Unused(1);
        const DensityMaskRule FlorMask(
            hash32::Combine(Seeds.For(SeedBook::Scatter), hash32::Text("EcoFlor")),
            Flor.DensityFrequency, Flor.DensityThreshold, 1.0f, Unused);
        for (const Placement& P : E1.Placements)
        {
            if (P.LayerIndex == 1 && nearestTree(P.X, P.Z) >= Helecho.NearDistance) ++fernFar;
            if (P.LayerIndex == 2)
            {
                if (nearestTree(P.X, P.Z) < Flor.AvoidDistance) ++flowerShade;
                const float m = W.SampleMoisture(P.X, P.Z);
                if (m < Flor.MinMoisture || m > Flor.MaxMoisture) ++flowerDry;
                if (FlorMask.MaskAt(P.X, P.Z, W) < Flor.DensityThreshold - DensityMaskRule::EdgeHalfWidth)
                    ++flowerOutside;
            }
        }
        snprintf(buf, sizeof(buf), "%d lejos de un arbol", fernFar);
        Check(fernFar == 0, "todos los helechos estan bajo un arbol", buf);
        snprintf(buf, sizeof(buf), "%d a la sombra", flowerShade);
        Check(flowerShade == 0, "ninguna flor a la sombra de un arbol", buf);
        snprintf(buf, sizeof(buf), "%d fuera de rango", flowerDry);
        Check(flowerDry == 0, "las flores respetan su rango de humedad", buf);
        snprintf(buf, sizeof(buf), "%d fuera de sus manchas", flowerOutside);
        Check(flowerOutside == 0, "con clustering = 1 no hay flores sueltas fuera de las manchas", buf);
    }

    printf("=== M. Agua: Priority-Flood y regimenes hidricos ===\n");
    {
        // La configuracion real del mapa de 1024 m.
        GenerationSettings A = S;
        A.WorldSize = 1024; A.Resolution = 512; A.Frequency = 4.5f; A.MoistureFrequency = 8.8f;
        A.MaxHeight = 150; A.HeightExponent = 1.6f; A.WarpStrength = 0.05f; A.WarpFrequency = 3.0f;
        A.RidgeWeight = 0.85f; A.MountainFrequency = 2.0f;
        A.TerraceWeight = 0.7f; A.TerraceSteps = 6.0f; A.TerraceFrequency = 2.5f;
        const WorldData WN = WorldGenerator::Generate(A, Zones);
        const uint32_t Dry = WN.Checksum();

        // Priority-Flood: lo rellenado nunca queda por debajo del terreno.
        {
            const std::vector<float> F = WaterGenerator::PriorityFlood(WN, 0.0f);
            int below = 0, raised = 0;
            for (size_t i = 0; i < F.size(); ++i)
            {
                if (F[i] < WN.Height[i]) ++below;
                if (F[i] > WN.Height[i]) ++raised;
            }
            snprintf(buf, sizeof(buf), "%d celdas rellenadas", raised);
            Check(below == 0 && raised > 0, "el relleno es >= el terreno y encuentra hondonadas", buf);
        }

        for (int32_t mode = 2; mode <= 7; ++mode)
        {
            GenerationSettings M = A;
            M.WaterMode = mode;
            const WorldData W1 = WorldGenerator::Generate(M, Zones);
            const WorldData W2 = WorldGenerator::Generate(M, Zones);

            int wet = 0, badLevel = 0, badDist = 0;
            float maxDepth = 0.0f;
            for (size_t i = 0; i < W1.Height.size(); ++i)
            {
                if (W1.WaterLevel[i] < 0.0f) { if (W1.WaterDistance[i] <= 0.0f) ++badDist; continue; }
                ++wet;
                if (W1.WaterLevel[i] < W1.Height[i]) ++badLevel;
                if (W1.WaterDistance[i] != 0.0f) ++badDist;
                maxDepth = std::max(maxDepth, (W1.WaterLevel[i] - W1.Height[i]) * W1.MaxHeight);
            }
            const char* name = WaterProfileName(static_cast<WaterProfile>(W1.WaterProfileId));
            snprintf(buf, sizeof(buf), "%-12s agua %5.1f%%  profundidad max %5.1f m  %08X",
                     name, 100.0f * wet / W1.Height.size(), maxDepth, W1.Checksum());
            Check(W1.Checksum() == W2.Checksum() && W1.Checksum() != Dry && wet > 0 &&
                  badLevel == 0 && badDist == 0 && W1.WaterProfileId == mode - 1,
                  "regimen determinista, con agua coherente", buf);
        }

        // Modo automatico: la semilla sortea el regimen.
        int hist[8] = { 0 };
        for (int32_t seed = 1; seed <= 40; ++seed)
        {
            GenerationSettings M = A;
            M.Seed = seed; M.WaterMode = 1; M.Resolution = 128;   // rapido: solo cuenta perfiles
            ++hist[WorldGenerator::Generate(M, Zones).WaterProfileId];
        }
        int distinct = 0;
        for (int k = 1; k < 7; ++k) if (hist[k] > 0) ++distinct;
        snprintf(buf, sizeof(buf), "seco %d, estanques %d, lagos %d, gran lago %d, archipielago %d, isla %d",
                 hist[1], hist[2], hist[3], hist[4], hist[5], hist[6]);
        Check(distinct >= 5, "40 semillas reparten casi todos los regimenes", buf);

        // El agua sube la humedad de la orilla, y nada se coloca dentro.
        GenerationSettings M = A;
        M.WaterMode = 3;   // Lagos
        const WorldData WW = WorldGenerator::Generate(M, Zones);
        double shoreWet = 0, shoreDry = 0; int shoreN = 0;
        for (size_t i = 0; i < WW.Height.size(); ++i)
            if (WW.WaterLevel[i] < 0.0f && WW.WaterDistance[i] < 10.0f)
            { shoreWet += WW.Moisture[i]; shoreDry += WN.Moisture[i]; ++shoreN; }
        snprintf(buf, sizeof(buf), "humedad media en la orilla %.2f -> %.2f",
                 shoreN ? shoreDry / shoreN : 0.0, shoreN ? shoreWet / shoreN : 0.0);
        Check(shoreN > 0 && shoreWet > shoreDry, "cerca del agua hay mas humedad", buf);

        std::vector<PropLayer> L(1);
        L[0].Name = "Orilla"; L[0].Spacing = 6.0f; L[0].ExclusionRadius = 2.0f;
        L[0].MaxSlope = 0.6f; L[0].DensityThreshold = 0.0f; L[0].MinHeight01 = 0.0f;
        L[0].MinWaterDistance = 1.5f;
        const SeedBook SB(WW.MasterSeed);
        const ScatterResult RW = Scatterer::Scatter(WW, L, SB);
        int wetProps = 0;
        for (const Placement& P : RW.Placements)
            if (WW.SampleField(WW.WaterDistance, P.X, P.Z) < 1.5f) ++wetProps;
        snprintf(buf, sizeof(buf), "%d objetos, %d en el agua", RW.Total(), wetProps);
        Check(RW.Total() > 0 && wetProps == 0, "ningun objeto dentro del agua", buf);

        Check(WorldGenerator::Generate(S, Zones).Checksum() == CsWorldChecksum,
              "con el agua apagada, sigue dando 64C68E3D");
    }

    printf("=== N. Caracter del relieve por semilla ===\n");
    {
        // Configuracion del editor (RELIEVE en construir_jardin.py), a menor
        // resolucion para que sea rapido. El paisaje no depende de ella.
        GenerationSettings E = S;
        E.WorldSize = 1024.0f; E.Resolution = 128;
        E.MaxHeight = 150.0f; E.Frequency = 4.5f; E.HeightExponent = 1.6f;
        E.WarpStrength = 0.05f; E.WarpFrequency = 3.0f;
        E.RidgeWeight = 0.85f; E.MountainFrequency = 2.0f;
        E.TerraceWeight = 0.7f; E.TerraceSteps = 6.0f; E.TerraceFrequency = 2.5f;
        E.WaterMode = 1;

        const WorldData Base = WorldGenerator::Generate(E, Zones);
        Check(Base.ReliefStyleId == -1, "con ReliefVariation = 0 no se sortea caracter");

        GenerationSettings V = E;
        V.ReliefVariation = 1.0f;
        const WorldData V1 = WorldGenerator::Generate(V, Zones);
        const WorldData V2 = WorldGenerator::Generate(V, Zones);
        snprintf(buf, sizeof(buf), "%s  %08X / %08X", ReliefStyleName(V1.ReliefStyleId), V1.Checksum(), V2.Checksum());
        Check(V1.Checksum() == V2.Checksum() && V1.ReliefStyleId == V2.ReliefStyleId,
              "el caracter es determinista", buf);

        // El sorteo con pesos respeta los pesos y los ceros.
        {
            const float Wt[4] = { 1.0f, 0.0f, 3.0f, -2.0f };
            DeterministicRandom Rng(12345u);
            int h[4] = { 0 };
            for (int i = 0; i < 20000; ++i) ++h[PickWeighted(Rng, Wt, 4)];
            snprintf(buf, sizeof(buf), "%d / %d / %d / %d (esperado ~5000 / 0 / ~15000 / 0)", h[0], h[1], h[2], h[3]);
            Check(h[1] == 0 && h[3] == 0 && h[0] > 4500 && h[0] < 5500, "PickWeighted respeta los pesos", buf);
        }

        // 60 semillas: cuantos caracteres salen y como es cada uno, medido.
        constexpr int K = static_cast<int>(ReliefStyle::Count);
        int count[K] = { 0 };
        double p90[K] = { 0 }, steep[K] = { 0 }, wet[K] = { 0 };
        int outOfRange = 0;
        for (int32_t seed = 1; seed <= 60; ++seed)
        {
            GenerationSettings M = V;
            M.Seed = seed;
            const WorldData WS = WorldGenerator::Generate(M, Zones);
            const int id = WS.ReliefStyleId;
            if (id < 0 || id >= K) { ++outOfRange; continue; }

            std::vector<float> H(WS.Height);
            int nSteep = 0, nWet = 0;
            for (size_t i = 0; i < H.size(); ++i)
            {
                if (H[i] < 0.0f || H[i] > 1.0f) ++outOfRange;
                if (WS.Slope[i] > 0.30f) ++nSteep;
                if (WS.HasWater() && WS.WaterLevel[i] >= 0.0f) ++nWet;
            }
            std::nth_element(H.begin(), H.begin() + H.size() * 9 / 10, H.end());
            ++count[id];
            p90[id] += H[H.size() * 9 / 10] * WS.MaxHeight;
            steep[id] += 100.0 * nSteep / H.size();
            wet[id] += 100.0 * nWet / H.size();
        }
        int distinct = 0;
        for (int k = 0; k < K; ++k)
        {
            if (count[k] == 0) continue;
            ++distinct;
            printf("        %-8s %2d semillas | p90 altura %5.1f m | roca %4.1f%% | agua %4.1f%%\n",
                   ReliefStyleName(k), count[k], p90[k] / count[k], steep[k] / count[k], wet[k] / count[k]);
        }
        Check(outOfRange == 0, "todos los caracteres dejan la altura en [0,1]");
        snprintf(buf, sizeof(buf), "%d de %d", distinct, K);
        Check(distinct == K, "60 semillas reparten todos los caracteres", buf);

        Check(WorldGenerator::Generate(S, Zones).Checksum() == CsWorldChecksum,
              "con ReliefVariation = 0, sigue dando 64C68E3D");
    }

    printf("=== O. Camino: Dijkstra, Catmull-Rom y cinta ===\n");
    {
        // Catmull-Rom es interpolante: pasa exactamente por P1 (t=0) y P2 (t=1).
        {
            using CatmullRom::Vec2;
            const Vec2 P0{ 0, 0 }, P1{ 10, 2 }, P2{ 13, 15 }, P3{ 30, 16 };
            const Vec2 a = CatmullRom::Evaluate(P0, P1, P2, P3, 0.0f);
            const Vec2 b = CatmullRom::Evaluate(P0, P1, P2, P3, 1.0f);
            const float ea = std::hypot(a.X - P1.X, a.Z - P1.Z), eb = std::hypot(b.X - P2.X, b.Z - P2.Z);
            snprintf(buf, sizeof(buf), "error en t=0: %.2e m, en t=1: %.2e m", ea, eb);
            Check(ea < 1e-3f && eb < 1e-3f, "Catmull-Rom pasa por sus puntos de control", buf);

            // Centripeta: con un punto de control casi encima de otro, la
            // uniforme hace un bucle; la centripeta no se sale del casco.
            const Vec2 Q0{ 0, 0 }, Q1{ 10, 0 }, Q2{ 10.5f, 0.2f }, Q3{ 20, 0 };
            float maxZ = 0.0f;
            for (int i = 0; i <= 100; ++i)
                maxZ = std::max(maxZ, std::fabs(CatmullRom::Evaluate(Q0, Q1, Q2, Q3, i / 100.0f).Z));
            snprintf(buf, sizeof(buf), "desvio maximo %.2f m (puntos a 0.2 m)", maxZ);
            Check(maxZ < 0.5f, "la parametrizacion centripeta no hace bucles", buf);
        }

        GenerationSettings E = S;
        E.WorldSize = 1024.0f; E.Resolution = 256;
        E.MaxHeight = 150.0f; E.Frequency = 4.5f; E.HeightExponent = 1.6f;
        E.WarpStrength = 0.05f; E.WarpFrequency = 3.0f;
        E.RidgeWeight = 0.85f; E.MountainFrequency = 2.0f;
        E.TerraceWeight = 0.7f; E.TerraceSteps = 6.0f; E.TerraceFrequency = 2.5f;
        E.WaterMode = 1; E.ReliefVariation = 1.0f;

        const uint32_t Off = WorldGenerator::Generate(E, Zones).Checksum();
        E.PathMode = 1;
        const WorldData P1 = WorldGenerator::Generate(E, Zones);
        const WorldData P2 = WorldGenerator::Generate(E, Zones);
        snprintf(buf, sizeof(buf), "%08X / %08X, %d muestras, %.0f m", P1.Checksum(), P2.Checksum(),
                 static_cast<int>(P1.Path.Samples.size()), P1.Path.Length);
        Check(P1.HasPath() && P1.Checksum() == P2.Checksum() && P1.Path.Samples.size() == P2.Path.Samples.size()
              && P1.Checksum() != Off, "el camino es determinista y cambia el terreno", buf);

        // 30 semillas: geometria del camino y sus garantias.
        int withPath = 0, wetSamples = 0, badStep = 0, missCtrl = 0, badWidth = 0, buried = 0;
        double sumLen = 0.0; float maxGrade = 0.0f, maxCross = 0.0f;
        for (int32_t seed = 1; seed <= 30; ++seed)
        {
            GenerationSettings M = E;
            M.Seed = seed;
            const WorldData WP = WorldGenerator::Generate(M, Zones);
            if (!WP.HasPath()) continue;
            ++withPath;
            const PathData& Pa = WP.Path;
            sumLen += Pa.Length;

            for (size_t i = 0; i < Pa.Samples.size(); ++i)
            {
                const PathSample& A = Pa.Samples[i];
                if (WP.HasWater() && WP.SampleField(WP.WaterDistance, A.X, A.Z) <= 0.0f) ++wetSamples;
                if (i + 1 < Pa.Samples.size())
                {
                    const PathSample& B = Pa.Samples[i + 1];
                    const float d = std::hypot(B.X - A.X, B.Z - A.Z);
                    if (i + 2 < Pa.Samples.size() && std::fabs(d - 1.0f) > 0.05f) ++badStep;
                    if (d > 0.5f) maxGrade = std::max(maxGrade, std::fabs(B.H - A.H) / d);
                }
                // Calzada plana: el terreno a 1 m a cada lado del eje, a la altura del eje.
                for (float side : { -1.0f, 1.0f })
                {
                    const float h = WP.SampleHeightMeters(A.X + A.TZ * side, A.Z - A.TX * side);
                    maxCross = std::max(maxCross, std::fabs(h - A.H));
                }
            }
            // La curva pasa por cada punto de control.
            for (size_t c = 0; c < Pa.ControlX.size(); ++c)
            {
                float best = 1e9f;
                for (const PathSample& A : Pa.Samples)
                    best = std::min(best, std::hypot(A.X - Pa.ControlX[c], A.Z - Pa.ControlZ[c]));
                if (best > 0.6f) ++missCtrl;
            }
            // Cinta de ancho constante y nunca bajo el terreno.
            const RibbonMesh R = PathRibbon::Build(WP);
            for (int32_t r = 0; r < R.Rows; ++r)
            {
                const int32_t l = R.Index(0, r), d = R.Index(1, r);
                if (std::fabs(std::hypot(R.PX[d] - R.PX[l], R.PZ[d] - R.PZ[l]) - Pa.Width) > 1e-3f) ++badWidth;
                for (int32_t v : { l, d })
                    if (R.PY[v] < WP.SampleHeightMeters(R.PX[v], R.PZ[v])) ++buried;
            }
        }
        snprintf(buf, sizeof(buf), "%d de 30, longitud media %.0f m", withPath, withPath ? sumLen / withPath : 0.0);
        Check(withPath >= 28, "casi todas las semillas tienen camino", buf);
        snprintf(buf, sizeof(buf), "%d muestras en el agua", wetSamples);
        Check(wetSamples == 0, "el camino nunca entra en el agua", buf);
        snprintf(buf, sizeof(buf), "%d pasos fuera de 1 m +- 5 cm", badStep);
        Check(badStep == 0, "muestras equiespaciadas por longitud de arco", buf);
        snprintf(buf, sizeof(buf), "%d puntos de control a mas de 0.6 m de la curva", missCtrl);
        Check(missCtrl == 0, "la curva pasa por todos los puntos de control", buf);
        snprintf(buf, sizeof(buf), "pendiente maxima de la calzada %.0f%%", maxGrade * 100.0f);
        Check(maxGrade < 0.35f, "la calzada es transitable", buf);
        snprintf(buf, sizeof(buf), "desnivel maximo de lado a lado %.2f m", maxCross);
        // Con celdas de 4 m (resolucion 256) la interpolacion bilineal del terreno
        // deja ~0.3 m en curvas; en el editor (512, celdas de 2 m) es menos.
        Check(maxCross < 0.35f, "el terreno queda plano bajo la calzada", buf);
        snprintf(buf, sizeof(buf), "%d filas con otro ancho, %d vertices bajo el terreno", badWidth, buried);
        Check(badWidth == 0 && buried == 0, "la cinta tiene ancho constante y no se entierra", buf);

        // Nada se coloca sobre la calzada.
        std::vector<PropLayer> L(1);
        L[0].Name = "Borde"; L[0].Spacing = 2.0f; L[0].ExclusionRadius = 0.5f;
        L[0].MaxSlope = 0.8f; L[0].DensityThreshold = 0.0f; L[0].MinHeight01 = 0.0f;
        L[0].MinWaterDistance = 0.5f; L[0].MinPathDistance = 0.5f;
        const SeedBook SB(P1.MasterSeed);
        const ScatterResult RP = Scatterer::Scatter(P1, L, SB);
        int onPath = 0;
        for (const Placement& Pl : RP.Placements)
            if (P1.SampleField(P1.PathDistance, Pl.X, Pl.Z) < P1.Path.Width * 0.5f + 0.5f) ++onPath;
        snprintf(buf, sizeof(buf), "%d objetos, %d sobre el camino", RP.Total(), onPath);
        Check(RP.Total() > 0 && onPath == 0, "ningun objeto sobre el camino", buf);

        Check(WorldGenerator::Generate(S, Zones).Checksum() == CsWorldChecksum,
              "con el camino apagado, sigue dando 64C68E3D");
    }

    printf("=== P. Pasto denso por trozos ===\n");
    {
        GenerationSettings E = S;
        E.WorldSize = 1024.0f; E.Resolution = 256;
        E.MaxHeight = 150.0f; E.Frequency = 4.5f; E.HeightExponent = 1.6f;
        E.WarpStrength = 0.05f; E.RidgeWeight = 0.85f; E.TerraceWeight = 0.7f;
        E.WaterMode = 1; E.ReliefVariation = 1.0f; E.PathMode = 1;
        E.Seed = 3;   // con lagos y camino
        const WorldData WG = WorldGenerator::Generate(E, Zones);
        const SeedBook SB(WG.MasterSeed);
        GrassParams GP; GP.Variants = 2;
        constexpr float CS = 16.0f;

        // Determinismo e independencia: el trozo da lo mismo solo, despues de
        // otros, o generado dos veces.
        std::vector<GrassInstance> A, B, Tmp;
        GrassField::BuildChunk(WG, SB, 3, -5, CS, GP, A);
        GrassField::BuildChunk(WG, SB, 4, -5, CS, GP, Tmp);
        GrassField::BuildChunk(WG, SB, -10, 7, CS, GP, Tmp);
        GrassField::BuildChunk(WG, SB, 3, -5, CS, GP, B);
        bool same = A.size() == B.size();
        for (size_t i = 0; same && i < A.size(); ++i)
            same = A[i].X == B[i].X && A[i].Z == B[i].Z && A[i].Scale == B[i].Scale && A[i].Variant == B[i].Variant;
        snprintf(buf, sizeof(buf), "%d matas", static_cast<int>(A.size()));
        Check(same && !A.empty(), "cada trozo es determinista e independiente del orden", buf);

        // Recorrer el mapa entero por trozos: reglas y densidad.
        const int32_t NC = static_cast<int32_t>(WG.WorldSize / CS);
        long total = 0; int outside = 0, wet = 0, onPath = 0, steep = 0, badVar = 0, offGround = 0;
        double meadowN = 0, forestN = 0, meadowA = 0, forestA = 0;
        for (int32_t cz = -NC / 2; cz < NC / 2; ++cz)
            for (int32_t cx = -NC / 2; cx < NC / 2; ++cx)
            {
                GrassField::BuildChunk(WG, SB, cx, cz, CS, GP, Tmp);
                total += static_cast<long>(Tmp.size());
                for (const GrassInstance& G : Tmp)
                {
                    if (GrassField::ChunkCoord(G.X, CS) != cx || GrassField::ChunkCoord(G.Z, CS) != cz) ++outside;
                    if (WG.HasWater() && WG.SampleField(WG.WaterDistance, G.X, G.Z) < GP.WaterClear) ++wet;
                    if (WG.SampleField(WG.PathDistance, G.X, G.Z) < WG.Path.Width * 0.5f + GP.PathClear) ++onPath;
                    if (WG.SampleSlope(G.X, G.Z) > GP.MaxSlope) ++steep;
                    if (G.Variant < 0 || G.Variant >= GP.Variants) ++badVar;
                    if (std::fabs(G.H - WG.SampleHeightMeters(G.X, G.Z)) > 1e-3f) ++offGround;
                    (WG.SampleZone(G.X, G.Z) == 0 ? meadowN : forestN) += 1.0;
                }
                // Area de cada zona en el trozo (muestreo de 4 x 4).
                for (int k = 0; k < 16; ++k)
                {
                    const float X = (cx + (k % 4 + 0.5f) / 4.0f) * CS, Z = (cz + (k / 4 + 0.5f) / 4.0f) * CS;
                    (WG.SampleZone(X, Z) == 0 ? meadowA : forestA) += CS * CS / 16.0;
                }
            }
        snprintf(buf, sizeof(buf), "%ld matas en el mapa, %d fuera de su trozo", total, outside);
        Check(total > 0 && outside == 0, "cada mata cae dentro de su trozo (sin huecos ni repetidos)", buf);
        snprintf(buf, sizeof(buf), "%d en el agua, %d en el camino, %d en pendiente, %d variantes malas, %d despegadas",
                 wet, onPath, steep, badVar, offGround);
        Check(wet + onPath + steep + badVar + offGround == 0, "el pasto respeta agua, camino y pendiente", buf);
        const double dM = meadowA > 0 ? meadowN / meadowA : 0, dF = forestA > 0 ? forestN / forestA : 0;
        snprintf(buf, sizeof(buf), "%.2f matas/m2 en pradera, %.2f en bosque", dM, dF);
        Check(dM > dF * 1.5 && dM > 0.5, "la pradera tiene mas pasto que el bosque", buf);

        // La mata procedural.
        {
            GrassClumpParams CP;
            const GrassClumpMesh M1 = GrassClump::Build(CP, 77u);
            const GrassClumpMesh M2 = GrassClump::Build(CP, 77u);
            const GrassClumpMesh M3 = GrassClump::Build(CP, 78u);
            const int tris = static_cast<int>(M1.Indices.size() / 3);
            const int expect = CP.Blades * (2 * (CP.Segments - 1) + 1) * 2;
            bool same = M1.PX == M2.PX && M1.PZ == M2.PZ && M1.Indices == M2.Indices;
            snprintf(buf, sizeof(buf), "%d triangulos (esperados %d), %d vertices", tris, expect, static_cast<int>(M1.PX.size()));
            Check(same && M1.PX != M3.PX && tris == expect, "la mata es determinista y tiene la topologia esperada", buf);

            int badIdx = 0, badV = 0, badRoot = 0; float maxZ = 0.0f, maxR = 0.0f;
            for (int32_t i : M1.Indices) if (i < 0 || i >= static_cast<int32_t>(M1.PX.size())) ++badIdx;
            for (size_t i = 0; i < M1.PX.size(); ++i)
            {
                if (M1.V[i] < 0.0f || M1.V[i] > 1.0f) ++badV;
                if (M1.V[i] == 0.0f && std::fabs(M1.PZ[i]) > 1e-6f) ++badRoot;
                maxZ = std::max(maxZ, M1.PZ[i]);
                maxR = std::max(maxR, std::hypot(M1.PX[i], M1.PY[i]));
            }
            snprintf(buf, sizeof(buf), "altura maxima %.2f m, radio maximo %.2f m", maxZ, maxR);
            Check(badIdx == 0 && badV == 0 && badRoot == 0 && maxZ <= CP.Height * (1.0f + CP.HeightJitter) + 1e-4f
                  && maxZ > CP.Height * 0.5f && maxR < 0.4f, "briznas con raiz en el suelo y del tamano pedido", buf);
        }

        // Lo que se dibuja: un circulo de 45 m alrededor de la camara.
        const double perCircle = (total / (WG.WorldSize * WG.WorldSize)) * 3.14159 * 45.0 * 45.0;
        snprintf(buf, sizeof(buf), "~%.0f matas en un radio de 45 m (media del mapa)", perCircle);
        Check(perCircle < 40000.0, "el circulo visible cabe en el presupuesto", buf);
    }

    printf("=== I. Rendimiento ===\n");
    printf("        mundo: %.1f ms   colocacion: %.1f ms\n",
           WorldGenerator::GetLastGenerationMs(), R.ElapsedMs);

    printf("\n%s\n", failures == 0 ? "TODAS LAS PRUEBAS PASAN" : "HUBO FALLAS");
    return failures == 0 ? 0 : 1;
}
