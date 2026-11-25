// JardinWater.cpp — Priority-Flood, regimenes hidricos y distancia al agua.

#include "JardinWater.h"
#include <queue>
#include <algorithm>
#include <cmath>

namespace jardin
{

const char* WaterProfileName(WaterProfile P)
{
    switch (P)
    {
    case WaterProfile::None:         return "Sin agua";
    case WaterProfile::Seco:         return "Seco";
    case WaterProfile::Estanques:    return "Estanques";
    case WaterProfile::Lagos:        return "Lagos";
    case WaterProfile::GranLago:     return "Gran lago";   // mar interior
    case WaterProfile::Archipielago: return "Archipielago";
    case WaterProfile::Isla:         return "Isla";
    default:                         return "?";
    }
}

namespace
{
    struct ProfileParams
    {
        float Fill = 1.0f;          // fraccion de la hondonada que se llena (1 = hasta el desborde)
        float MinAreaScale = 1.0f;  // multiplica el area minima de lago
        int32_t MaxLakes = 1 << 30; // se conservan los N lagos de mas volumen
        float SeaMeters = 0.0f;     // nivel del mar; 0 = sin mar
        float RimQuantile = 0.0f;   // > 0: levantar el anillo del borde (gran lago)
    };

    /** Sorteo del regimen. Pesos: comunes los normales, raros los exagerados. */
    WaterProfile Pick(DeterministicRandom& Rng)
    {
        const float r = Rng.NextFloat();
        if (r < 0.10f) return WaterProfile::Seco;          // 10%
        if (r < 0.35f) return WaterProfile::Estanques;     // 25%
        if (r < 0.65f) return WaterProfile::Lagos;         // 30%
        if (r < 0.80f) return WaterProfile::GranLago;      // 15%
        if (r < 0.90f) return WaterProfile::Archipielago;  // 10%
        return WaterProfile::Isla;                         // 10%
    }

    /** Cada regimen ademas varia su intensidad con la semilla: no hay dos
     *  "Lagos" iguales. Rangos calibrados midiendo cuencas reales del terreno. */
    ProfileParams ParamsFor(WaterProfile P, DeterministicRandom& Rng)
    {
        ProfileParams Q;
        switch (P)
        {
        case WaterProfile::Seco:
            Q.Fill = Rng.Range(0.25f, 0.45f); Q.MinAreaScale = 5.0f; Q.MaxLakes = 3;
            break;
        case WaterProfile::Estanques:
            Q.Fill = Rng.Range(0.50f, 0.70f); Q.MaxLakes = Rng.Range(4, 10);
            break;
        case WaterProfile::Lagos:
            Q.Fill = Rng.Range(0.85f, 0.97f);
            break;
        case WaterProfile::GranLago:
            // Entre un 20 y un 32% del mapa queda por debajo del dique.
            // El lago queda unos metros por debajo de la cresta: orilla seca.
            Q.Fill = 0.88f; Q.RimQuantile = Rng.Range(0.20f, 0.32f);
            break;
        case WaterProfile::Archipielago:
            // Con el mar a ~20 m queda bajo el agua un 35-40% del mapa.
            Q.SeaMeters = Rng.Range(17.0f, 24.0f); Q.Fill = 0.92f;
            break;
        case WaterProfile::Isla:
            // A 5-9 m el anillo bajo del borde se vuelve mar: el jardin es una isla.
            Q.SeaMeters = Rng.Range(5.0f, 9.0f); Q.Fill = 0.92f;
            break;
        default:
            break;
        }
        return Q;
    }

    constexpr int32_t NbrDX[4] = { 1, -1, 0, 0 };
    constexpr int32_t NbrDY[4] = { 0, 0, 1, -1 };

    struct Basin
    {
        std::vector<int32_t> Cells;
        float MinH = 1e30f;
        float Spill = 0.0f;
        double Volume = 0.0;
        bool TouchesBorder = false;
    };

    /** Componentes conexas (4-vecinos) de celdas con agua estancada. */
    std::vector<Basin> FindBasins(const WorldData& W, const std::vector<float>& Filled, float Eps)
    {
        const int32_t N = W.Side;
        std::vector<int32_t> Label(W.Height.size(), -1);
        std::vector<Basin> Out;
        std::vector<int32_t> Stack;

        for (int32_t i = 0; i < static_cast<int32_t>(W.Height.size()); ++i)
        {
            if (Label[i] >= 0 || Filled[i] - W.Height[i] <= Eps) continue;

            Basin B;
            B.Spill = Filled[i];
            const int32_t Id = static_cast<int32_t>(Out.size());
            Label[i] = Id;
            Stack.assign(1, i);
            while (!Stack.empty())
            {
                const int32_t c = Stack.back(); Stack.pop_back();
                B.Cells.push_back(c);
                B.MinH = std::min(B.MinH, W.Height[c]);
                B.Volume += Filled[c] - W.Height[c];
                const int32_t cx = c % N, cy = c / N;
                if (cx == 0 || cy == 0 || cx == N - 1 || cy == N - 1) B.TouchesBorder = true;
                for (int32_t k = 0; k < 4; ++k)
                {
                    const int32_t nx = cx + NbrDX[k], ny = cy + NbrDY[k];
                    if (nx < 0 || ny < 0 || nx >= N || ny >= N) continue;
                    const int32_t n = W.Index(nx, ny);
                    if (Label[n] < 0 && Filled[n] - W.Height[n] > Eps) { Label[n] = Id; Stack.push_back(n); }
                }
            }
            Out.push_back(std::move(B));
        }
        return Out;
    }

    /** Gran lago: el borde del mapa se levanta como un anillo de montes y
     *  las tierras bajas del interior quedan encerradas. Al inundar, se
     *  llenan hasta el paso mas bajo del anillo: un mar interior.
     *
     *  Excavar un cuenco no sirve: el cuenco tambien rebaja el paso por donde
     *  la hondonada se desborda, y el lago casi no crece. Hay que levantar el
     *  dique, no cavar el fondo.
     *
     *  Quantile = fraccion del mapa por debajo del nivel buscado. */
    void RaiseRim(WorldData& W, float Quantile, uint32_t HillSeed)
    {
        const int32_t N = W.Side;
        const float BandCells = 0.12f * static_cast<float>(W.Resolution);
        auto BorderDistance = [N](int32_t x, int32_t y)
        { return std::min(std::min(x, y), std::min(N - 1 - x, N - 1 - y)); };

        // El cuantil se mide solo en el interior: la franja del borde ya esta
        // hundida por la caida del terreno y falsearia el nivel.
        std::vector<float> Sorted;
        for (int32_t y = 0; y < N; ++y)
            for (int32_t x = 0; x < N; ++x)
                if (static_cast<float>(BorderDistance(x, y)) >= BandCells) Sorted.push_back(W.Height[W.Index(x, y)]);
        std::sort(Sorted.begin(), Sorted.end());
        const size_t q = static_cast<size_t>(Quantile * static_cast<float>(Sorted.size() - 1));
        const float Wall01 = Sorted[q] + 5.0f / W.MaxHeight;   // 5 m por encima del nivel

        for (int32_t y = 0; y < N; ++y)
            for (int32_t x = 0; x < N; ++x)
            {
                const int32_t b = BorderDistance(x, y);
                // Talud en la mitad interior de la franja; meseta hasta el borde.
                const float t = 2.0f * (1.0f - static_cast<float>(b) / BandCells);
                if (t <= 0.0f) continue;
                const float u = std::min(t, 1.0f);
                const float s = u * u * (3.0f - 2.0f * u);           // smoothstep
                // Colinas sobre la meseta para que el dique no parezca una
                // muralla. Solo suman: el paso mas bajo sigue en Wall01.
                const float u01 = static_cast<float>(x) / static_cast<float>(N - 1);
                const float v01 = static_cast<float>(y) / static_cast<float>(N - 1);
                const float Hills = noise::Fbm01(u01, v01, HillSeed, 4, 9.0f, 2.0f, 0.5f) * 30.0f / W.MaxHeight;
                float& h = W.Height[W.Index(x, y)];
                h = std::min(1.0f, std::max(h, (Wall01 + Hills) * s));
            }
    }

    /** Distancia aproximada (chamfer 1 / raiz de 2) a la celda con agua mas cercana. */
    void DistanceTransform(WorldData& W)
    {
        const int32_t N = W.Side;
        const float Inf = 1e9f, D1 = 1.0f, D2 = 1.41421356f;
        std::vector<float>& D = W.WaterDistance;
        D.assign(W.Height.size(), Inf);
        for (size_t i = 0; i < D.size(); ++i) if (W.WaterLevel[i] >= 0.0f) D[i] = 0.0f;

        auto Relax = [&](int32_t x, int32_t y, int32_t nx, int32_t ny, float Cost)
        {
            if (nx < 0 || ny < 0 || nx >= N || ny >= N) return;
            float& d = D[W.Index(x, y)];
            const float c = D[W.Index(nx, ny)] + Cost;
            if (c < d) d = c;
        };

        for (int32_t y = 0; y < N; ++y)
            for (int32_t x = 0; x < N; ++x)
            {
                Relax(x, y, x - 1, y, D1); Relax(x, y, x, y - 1, D1);
                Relax(x, y, x - 1, y - 1, D2); Relax(x, y, x + 1, y - 1, D2);
            }
        for (int32_t y = N - 1; y >= 0; --y)
            for (int32_t x = N - 1; x >= 0; --x)
            {
                Relax(x, y, x + 1, y, D1); Relax(x, y, x, y + 1, D1);
                Relax(x, y, x + 1, y + 1, D2); Relax(x, y, x - 1, y + 1, D2);
            }

        const float Cell = W.CellSize();
        for (float& d : D) d = std::min(d * Cell, 1e6f);
    }
}

std::vector<float> WaterGenerator::PriorityFlood(const WorldData& W, float SeaLevel01)
{
    const int32_t N = W.Side;
    std::vector<float> Filled(W.Height);
    std::vector<uint8_t> Closed(W.Height.size(), 0);

    // Cola de minimos por (altura, indice): el indice desempata de forma
    // determinista cuando dos celdas tienen la misma altura.
    typedef std::pair<float, int32_t> Item;
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> Open;

    // El borde es la salida del agua. Con mar, arranca a su nivel.
    for (int32_t y = 0; y < N; ++y)
        for (int32_t x = 0; x < N; ++x)
        {
            if (x != 0 && y != 0 && x != N - 1 && y != N - 1) continue;
            const int32_t i = W.Index(x, y);
            Filled[i] = std::max(W.Height[i], SeaLevel01);
            Closed[i] = 1;
            Open.push(Item(Filled[i], i));
        }

    while (!Open.empty())
    {
        const Item c = Open.top();
        Open.pop();
        const int32_t cx = c.second % N, cy = c.second / N;
        for (int32_t k = 0; k < 4; ++k)
        {
            const int32_t nx = cx + NbrDX[k], ny = cy + NbrDY[k];
            if (nx < 0 || ny < 0 || nx >= N || ny >= N) continue;
            const int32_t n = W.Index(nx, ny);
            if (Closed[n]) continue;
            Closed[n] = 1;
            // La vecina queda al menos a la altura de quien la alcanzo: si esta
            // mas baja, es agua estancada que se desborda por este punto.
            Filled[n] = std::max(W.Height[n], c.first);
            Open.push(Item(Filled[n], n));
        }
    }
    return Filled;
}

void WaterGenerator::Generate(WorldData& W, const GenerationSettings& S, const SeedBook& Seeds)
{
    if (S.WaterMode <= 0) return;

    DeterministicRandom Rng(Seeds.For(SeedBook::Water));
    const WaterProfile P = (S.WaterMode == 1)
        ? Pick(Rng)
        : static_cast<WaterProfile>(std::min(S.WaterMode - 1, static_cast<int32_t>(WaterProfile::Count) - 1));
    const ProfileParams Q = ParamsFor(P, Rng);

    if (Q.RimQuantile > 0.0f) RaiseRim(W, Q.RimQuantile, Rng.NextUInt());

    const float Sea01 = Q.SeaMeters > 0.0f ? Q.SeaMeters / W.MaxHeight : 0.0f;
    const std::vector<float> Filled = PriorityFlood(W, Sea01);
    std::vector<Basin> Basins = FindBasins(W, Filled, 0.05f / W.MaxHeight);

    // Lagos (lo que no es mar): filtrar por area y quedarse con los de mas volumen.
    const float CellArea = W.CellSize() * W.CellSize();
    const float MinCells = S.WaterMinAreaM2 * Q.MinAreaScale / CellArea;
    std::vector<const Basin*> Lakes;
    for (const Basin& B : Basins)
    {
        const bool bSea = Sea01 > 0.0f && B.TouchesBorder;
        if (!bSea && static_cast<float>(B.Cells.size()) >= MinCells) Lakes.push_back(&B);
    }
    // stable_sort: lagos con el mismo volumen conservan su orden, y el
    // resultado no depende de la implementacion de la ordenacion.
    std::stable_sort(Lakes.begin(), Lakes.end(),
                     [](const Basin* a, const Basin* b) { return a->Volume > b->Volume; });
    if (static_cast<int32_t>(Lakes.size()) > Q.MaxLakes) Lakes.resize(Q.MaxLakes);

    W.WaterLevel.assign(W.Height.size(), -1.0f);

    // Mar: todo lo conectado al borde que queda bajo su nivel.
    if (Sea01 > 0.0f)
        for (const Basin& B : Basins)
            if (B.TouchesBorder)
                for (int32_t c : B.Cells) W.WaterLevel[c] = Filled[c];

    // Lagos: llenos hasta una fraccion de su profundidad. Con Fill < 1 solo
    // queda agua en la parte honda y la orilla se retira del desborde.
    for (const Basin* B : Lakes)
    {
        const float Level = B->MinH + Q.Fill * (B->Spill - B->MinH);
        for (int32_t c : B->Cells)
            if (W.Height[c] < Level) W.WaterLevel[c] = Level;
    }

    W.WaterProfileId = static_cast<uint8_t>(P);
    DistanceTransform(W);
}

} // namespace jardin
