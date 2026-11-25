// JardinPath.cpp — Ruta por Dijkstra, curva Catmull-Rom y aplanado del terreno.

#include "JardinPath.h"

#include <cmath>
#include <functional>
#include <queue>
#include <utility>

namespace jardin
{

// ---------------------------------------------------------------- Catmull-Rom

CatmullRom::Vec2 CatmullRom::Evaluate(const Vec2& P0, const Vec2& P1, const Vec2& P2, const Vec2& P3,
                                      float t, float Alpha)
{
    // Nudo siguiente: distancia entre puntos elevada a Alpha. Un minimo evita
    // dividir entre cero si dos puntos de control coinciden.
    auto Knot = [Alpha](float ti, const Vec2& A, const Vec2& B)
    {
        const double dx = B.X - A.X, dz = B.Z - A.Z;
        const double d = std::pow(dx * dx + dz * dz, 0.5 * Alpha);
        return ti + static_cast<float>(d < 1e-4 ? 1e-4 : d);
    };
    const float t0 = 0.0f;
    const float t1 = Knot(t0, P0, P1);
    const float t2 = Knot(t1, P1, P2);
    const float t3 = Knot(t2, P2, P3);
    const float tt = t1 + (t2 - t1) * t;

    // Interpolacion lineal de A hacia B, con el parametro tt entre los nudos a y b.
    auto L = [tt](const Vec2& A, const Vec2& B, float a, float b)
    {
        const float w = (tt - a) / (b - a);
        return Vec2{ A.X + (B.X - A.X) * w, A.Z + (B.Z - A.Z) * w };
    };

    // Piramide de Barry-Goldman: tres niveles de interpolaciones lineales.
    const Vec2 A1 = L(P0, P1, t0, t1);
    const Vec2 A2 = L(P1, P2, t1, t2);
    const Vec2 A3 = L(P2, P3, t2, t3);
    const Vec2 B1 = L(A1, A2, t0, t2);
    const Vec2 B2 = L(A2, A3, t1, t3);
    return L(B1, B2, t1, t2);
}

// ---------------------------------------------------------------- ruta

namespace
{
    constexpr float kInf = 1e30f;

    // Rejilla gruesa sobre la que se busca la ruta. Las celdas de ~8 m bastan
    // para un sendero y hacen que Dijkstra cueste milisegundos.
    struct CoarseGrid
    {
        int32_t N = 0;          // celdas por lado; nodos = (N+1)^2
        float Step = 0.0f;      // metros
        float Half = 0.0f;
        std::vector<float> H;           // altura en metros
        std::vector<float> Wander;      // multiplicador de costo (ruido): hace serpentear la ruta
        std::vector<uint8_t> Blocked;   // agua, orilla o borde del mapa

        int32_t Side() const { return N + 1; }
        float X(int32_t Node) const { return -Half + static_cast<float>(Node % Side()) * Step; }
        float Z(int32_t Node) const { return -Half + static_cast<float>(Node / Side()) * Step; }
    };

    CoarseGrid BuildGrid(const WorldData& W, const GenerationSettings& S, uint32_t Seed)
    {
        CoarseGrid G;
        int32_t N = static_cast<int32_t>(W.WorldSize / 8.0f + 0.5f);
        G.N = N < 16 ? 16 : (N > 160 ? 160 : N);
        G.Step = W.WorldSize / static_cast<float>(G.N);
        G.Half = W.WorldSize * 0.5f;

        const int32_t Side = G.Side();
        const size_t Count = static_cast<size_t>(Side) * Side;
        G.H.resize(Count); G.Wander.resize(Count); G.Blocked.resize(Count);

        // Lejos del borde: alli el terreno cae (EdgeFalloff) y el camino
        // terminaria en un barranco.
        const int32_t Margin = 2 + G.N / 20;
        for (int32_t j = 0; j < Side; ++j)
        {
            for (int32_t i = 0; i < Side; ++i)
            {
                const int32_t n = j * Side + i;
                const float X = G.X(n), Z = G.Z(n);
                G.H[n] = W.SampleHeightMeters(X, Z);

                const float u = static_cast<float>(i) / static_cast<float>(G.N);
                const float v = static_cast<float>(j) / static_cast<float>(G.N);
                G.Wander[n] = 1.0f + 0.8f * noise::Fbm01(u, v, Seed, 2, 5.0f, 2.0f, 0.5f);

                const bool bEdge = i < Margin || j < Margin || i > G.N - Margin || j > G.N - Margin;
                // Margen al agua: media calzada + casi una celda, porque el suavizado
                // de la ruta puede desplazar los nodos hasta ~media celda.
                const bool bWet = W.HasWater() && W.SampleField(W.WaterDistance, X, Z) < S.PathWidth + G.Step * 0.75f;
                G.Blocked[n] = (bEdge || bWet) ? 1 : 0;
            }
        }
        return G;
    }

    // Dijkstra con 8 vecinos. Costo de un paso = longitud x ruido x castigo por
    // pendiente: un sendero rodea las cuestas en vez de subirlas de frente.
    // La cola desempata por indice de nodo, asi el resultado es determinista.
    void Dijkstra(const CoarseGrid& G, int32_t Source, std::vector<float>& Dist, std::vector<int32_t>& Prev)
    {
        const int32_t Side = G.Side();
        Dist.assign(G.H.size(), kInf);
        Prev.assign(G.H.size(), -1);

        using Item = std::pair<float, int32_t>;
        std::priority_queue<Item, std::vector<Item>, std::greater<Item>> Open;
        Dist[Source] = 0.0f;
        Open.push({ 0.0f, Source });

        static const int32_t DI[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
        static const int32_t DJ[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };

        while (!Open.empty())
        {
            const Item Top = Open.top();
            Open.pop();
            const int32_t n = Top.second;
            if (Top.first > Dist[n]) continue;   // entrada vieja

            const int32_t i = n % Side, j = n / Side;
            for (int32_t k = 0; k < 8; ++k)
            {
                const int32_t ni = i + DI[k], nj = j + DJ[k];
                if (ni < 0 || nj < 0 || ni >= Side || nj >= Side) continue;
                const int32_t m = nj * Side + ni;
                if (G.Blocked[m]) continue;

                const float Len = (k < 4 ? 1.0f : 1.41421356f) * G.Step;
                const float Grade = std::fabs(G.H[m] - G.H[n]) / Len;
                float Cost = Len * 0.5f * (G.Wander[n] + G.Wander[m]) * (1.0f + 30.0f * Grade * Grade);
                if (Grade > 0.30f) Cost += Len * 15.0f;   // mas de ~17 grados: casi prohibido

                const float D = Dist[n] + Cost;
                if (D < Dist[m])
                {
                    Dist[m] = D;
                    Prev[m] = n;
                    Open.push({ D, m });
                }
            }
        }
    }

    // De los nodos alcanzables, uno al azar entre los mas alejados (en linea
    // recta) de From. Asi el camino cruza el mapa y no se queda en un rincon.
    int32_t PickFar(const CoarseGrid& G, int32_t From, const std::vector<float>& Dist, DeterministicRandom& Rng)
    {
        float Best = 0.0f;
        for (size_t n = 0; n < Dist.size(); ++n)
        {
            if (Dist[n] >= kInf) continue;
            const float dx = G.X(static_cast<int32_t>(n)) - G.X(From), dz = G.Z(static_cast<int32_t>(n)) - G.Z(From);
            Best = std::max(Best, dx * dx + dz * dz);
        }
        std::vector<int32_t> Far;
        for (size_t n = 0; n < Dist.size(); ++n)
        {
            if (Dist[n] >= kInf) continue;
            const float dx = G.X(static_cast<int32_t>(n)) - G.X(From), dz = G.Z(static_cast<int32_t>(n)) - G.Z(From);
            if (dx * dx + dz * dz >= Best * 0.80f) Far.push_back(static_cast<int32_t>(n));   // >= ~90% de la distancia
        }
        return Far.empty() ? From : Far[Rng.Range(0, static_cast<int32_t>(Far.size()))];
    }

    float SmoothStep01(float a, float b, float x)
    {
        float t = (x - a) / (b - a);
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        return t * t * (3.0f - 2.0f * t);
    }
}

void PathGenerator::Generate(WorldData& W, const GenerationSettings& S, const SeedBook& Seeds)
{
    if (S.PathMode <= 0) return;

    DeterministicRandom Rng(Seeds.For(SeedBook::Path));
    const CoarseGrid G = BuildGrid(W, S, Rng.NextUInt());

    std::vector<int32_t> Land;
    for (size_t n = 0; n < G.Blocked.size(); ++n)
        if (!G.Blocked[n]) Land.push_back(static_cast<int32_t>(n));
    if (Land.size() < 16) return;   // casi todo agua: no hay donde trazarlo

    // Extremos: desde un punto al azar, el mas lejano alcanzable (A); desde A,
    // el mas lejano alcanzable (B). Es una aproximacion del "diametro" de la
    // tierra firme: el camino la recorre de punta a punta.
    std::vector<float> Dist;
    std::vector<int32_t> Prev;
    const int32_t Seed0 = Land[Rng.Range(0, static_cast<int32_t>(Land.size()))];
    Dijkstra(G, Seed0, Dist, Prev);
    const int32_t A = PickFar(G, Seed0, Dist, Rng);
    Dijkstra(G, A, Dist, Prev);
    const int32_t B = PickFar(G, A, Dist, Rng);

    const float ABx = G.X(B) - G.X(A), ABz = G.Z(B) - G.Z(A);
    if (std::sqrt(ABx * ABx + ABz * ABz) < W.WorldSize * 0.2f) return;   // isla demasiado pequena

    std::vector<int32_t> Route;
    for (int32_t n = B; n != -1; n = Prev[n]) Route.push_back(n);
    // Route va de B a A; el sentido da igual.

    // --- la ruta de Dijkstra va en zigzag: solo conoce 8 direcciones. Se
    // suaviza con medias moviles (+-2 nodos, tres pasadas; los extremos no se
    // mueven). Sin esto, al refinar, la curva copiaria cada zigzag y haria
    // codos de menos de un metro de radio.
    const int32_t NR = static_cast<int32_t>(Route.size());
    std::vector<float> RX(NR), RZ(NR);
    for (int32_t k = 0; k < NR; ++k) { RX[k] = G.X(Route[k]); RZ[k] = G.Z(Route[k]); }
    for (int32_t Pass = 0; Pass < 3; ++Pass)
    {
        std::vector<float> TX(RX), TZ(RZ);
        for (int32_t k = 1; k + 1 < NR; ++k)
        {
            float sx = 0.0f, sz = 0.0f; int32_t c = 0;
            for (int32_t j = k - 2; j <= k + 2; ++j)
                if (j >= 0 && j < NR) { sx += RX[j]; sz += RZ[j]; ++c; }
            TX[k] = sx / c; TZ[k] = sz / c;
        }
        RX.swap(TX); RZ.swap(TZ);
    }

    // --- puntos de control: uno cada PathControlSpacing metros de ruta. Se
    // guardan como indices de la ruta para poder insertar mas despues.
    const int32_t Every = std::max(1, static_cast<int32_t>(S.PathControlSpacing / G.Step + 0.5f));
    std::vector<int32_t> CtrlIdx;
    for (int32_t k = 0; k < NR; k += Every) CtrlIdx.push_back(k);
    if (CtrlIdx.back() != NR - 1)
    {
        // El ultimo tramo quedo corto: si es menos de medio tramo, el ultimo
        // punto se mueve al final en vez de anadir uno muy cerca.
        if (NR - 1 - CtrlIdx.back() < Every / 2 && CtrlIdx.size() > 1) CtrlIdx.back() = NR - 1;
        else CtrlIdx.push_back(NR - 1);
    }
    if (CtrlIdx.size() < 2) return;

    PathData& P = W.Path;
    P = PathData();
    P.Width = S.PathWidth;

    // Curva por los puntos de control, remuestreada cada metro. SegOf dice de
    // que tramo (entre el control c y el c+1) sale cada muestra.
    std::vector<int32_t> SegOf;
    auto BuildCurve = [&]()
    {
        P.Samples.clear(); SegOf.clear();
        const int32_t NC = static_cast<int32_t>(CtrlIdx.size());
        auto At = [&](int32_t k) { return CatmullRom::Vec2{ RX[CtrlIdx[k]], RZ[CtrlIdx[k]] }; };
        auto CtrlAt = [&](int32_t i) -> CatmullRom::Vec2
        {
            // En los extremos se inventa un punto reflejado (P_-1 = 2 P0 - P1)
            // para que la curva llegue hasta ellos con la direccion de la ruta.
            if (i < 0)   { const auto a = At(0), b = At(1); return CatmullRom::Vec2{ 2 * a.X - b.X, 2 * a.Z - b.Z }; }
            if (i >= NC) { const auto a = At(NC - 1), b = At(NC - 2); return CatmullRom::Vec2{ 2 * a.X - b.X, 2 * a.Z - b.Z }; }
            return At(i);
        };

        // Curva densa: Catmull-Rom por tramos.
        constexpr int32_t Sub = 32;
        std::vector<CatmullRom::Vec2> Dense;
        std::vector<int32_t> DenseSeg;
        for (int32_t i = 0; i < NC - 1; ++i)
            for (int32_t s = 0; s < Sub; ++s)
            {
                Dense.push_back(CatmullRom::Evaluate(CtrlAt(i - 1), CtrlAt(i), CtrlAt(i + 1), CtrlAt(i + 2),
                                                     static_cast<float>(s) / Sub));
                DenseSeg.push_back(i);
            }
        Dense.push_back(At(NC - 1));
        DenseSeg.push_back(NC - 2);

        // Remuestreo por longitud de arco: el parametro t NO avanza a
        // velocidad constante a lo largo de la curva; la longitud de arco si.
        constexpr float Step = 1.0f;
        P.Samples.push_back({ Dense[0].X, Dense[0].Z });
        SegOf.push_back(0);
        float Acc = 0.0f, Next = Step;
        for (size_t i = 1; i < Dense.size(); ++i)
        {
            const float dx = Dense[i].X - Dense[i - 1].X, dz = Dense[i].Z - Dense[i - 1].Z;
            const float Seg = std::sqrt(dx * dx + dz * dz);
            while (Seg > 0.0f && Acc + Seg >= Next)
            {
                const float f = (Next - Acc) / Seg;
                PathSample Smp;
                Smp.X = Dense[i - 1].X + dx * f;
                Smp.Z = Dense[i - 1].Z + dz * f;
                Smp.S = Next;
                P.Samples.push_back(Smp);
                SegOf.push_back(DenseSeg[i - 1]);
                Next += Step;
            }
            Acc += Seg;
        }
        if (Acc - P.Samples.back().S > 0.25f)
        {
            P.Samples.push_back({ Dense.back().X, Dense.back().Z, 0.0f, 1.0f, 0.0f, Acc });
            SegOf.push_back(NC - 2);
        }
        P.Length = Acc;
    };

    // Altura del terreno bajo cada muestra, suavizada +-6 m.
    std::vector<float> Ground;
    auto SampleGround = [&]()
    {
        const int32_t NS = static_cast<int32_t>(P.Samples.size());
        std::vector<float> Raw(NS);
        for (int32_t i = 0; i < NS; ++i) Raw[i] = W.SampleHeightMeters(P.Samples[i].X, P.Samples[i].Z);
        Ground.assign(NS, 0.0f);
        constexpr int32_t Window = 6;
        for (int32_t i = 0; i < NS; ++i)
        {
            float Sum = 0.0f; int32_t Cnt = 0;
            for (int32_t k = i - Window; k <= i + Window; ++k)
                if (k >= 0 && k < NS) { Sum += Raw[k]; ++Cnt; }
            Ground[i] = Sum / static_cast<float>(Cnt);
        }
    };

    // --- refinamiento adaptativo. Entre dos puntos de control lejanos la
    // curva corta en linea recta lo que la ruta rodeaba: sube laderas de
    // frente o roza un lago. Donde pasa eso se inserta el punto de la ruta que
    // queda a mitad del tramo y se recalcula. Termina cuando ningun tramo
    // viola nada o cuando los tramos ya no se pueden partir.
    constexpr float MaxGrade = 0.20f;   // 20% (~11 grados): pendiente maxima de la calzada
    const float WaterClear = S.PathWidth * 0.5f + 1.0f;
    for (int32_t Pass = 0; Pass < 12; ++Pass)
    {
        BuildCurve();
        SampleGround();
        const int32_t NS = static_cast<int32_t>(P.Samples.size());
        std::vector<uint8_t> Bad(CtrlIdx.size(), 0);
        for (int32_t i = 0; i < NS; ++i)
        {
            const PathSample& Smp = P.Samples[i];
            bool bBad = W.HasWater() && W.SampleField(W.WaterDistance, Smp.X, Smp.Z) < WaterClear;
            if (i + 1 < NS)
            {
                const float ds = P.Samples[i + 1].S - Smp.S;
                bBad = bBad || (ds > 0.5f && std::fabs(Ground[i + 1] - Ground[i]) / ds > MaxGrade);
            }
            if (bBad) Bad[SegOf[i]] = 1;
        }

        std::vector<int32_t> Refined;
        bool bChanged = false;
        for (size_t c = 0; c < CtrlIdx.size(); ++c)
        {
            Refined.push_back(CtrlIdx[c]);
            if (c + 1 < CtrlIdx.size() && Bad[c] && CtrlIdx[c + 1] - CtrlIdx[c] > 2)
            {
                Refined.push_back((CtrlIdx[c] + CtrlIdx[c + 1]) / 2);
                bChanged = true;
            }
        }
        if (!bChanged) break;
        CtrlIdx.swap(Refined);
    }
    if (P.Samples.size() < 2) { W.Path = PathData(); return; }

    for (int32_t k : CtrlIdx) { P.ControlX.push_back(RX[k]); P.ControlZ.push_back(RZ[k]); }

    // --- tangentes por diferencias centrales.
    const int32_t NS = static_cast<int32_t>(P.Samples.size());
    for (int32_t i = 0; i < NS; ++i)
    {
        PathSample& Smp = P.Samples[i];
        const PathSample& Pm = P.Samples[i > 0 ? i - 1 : i];
        const PathSample& Pp = P.Samples[i < NS - 1 ? i + 1 : i];
        const float tx = Pp.X - Pm.X, tz = Pp.Z - Pm.Z;
        const float tl = std::sqrt(tx * tx + tz * tz);
        Smp.TX = tl > 0.0f ? tx / tl : 1.0f;
        Smp.TZ = tl > 0.0f ? tz / tl : 0.0f;
    }

    // --- perfil de la calzada con pendiente maxima. Donde ni la ruta pudo
    // evitar la cuesta, se hace como en una carretera: desmonte y terraplen.
    //   Bajo = min_j (Ground_j + g |s_i - s_j|)   la envolvente que solo excava
    //   Alto = max_j (Ground_j - g |s_i - s_j|)   la que solo rellena
    // Las dos tienen pendiente <= g, y su media tambien: la mitad del desnivel
    // se excava y la otra mitad se rellena. Dos pasadas por envolvente, O(n).
    std::vector<float> Lo(Ground), Hi(Ground);
    for (int32_t i = 1; i < NS; ++i)
    {
        const float ds = P.Samples[i].S - P.Samples[i - 1].S;
        Lo[i] = std::min(Lo[i], Lo[i - 1] + MaxGrade * ds);
        Hi[i] = std::max(Hi[i], Hi[i - 1] - MaxGrade * ds);
    }
    for (int32_t i = NS - 2; i >= 0; --i)
    {
        const float ds = P.Samples[i + 1].S - P.Samples[i].S;
        Lo[i] = std::min(Lo[i], Lo[i + 1] + MaxGrade * ds);
        Hi[i] = std::max(Hi[i], Hi[i + 1] - MaxGrade * ds);
    }
    for (int32_t i = 0; i < NS; ++i) P.Samples[i].H = 0.5f * (Lo[i] + Hi[i]);

    // --- aplanado y campo PathDistance. Cada tramo de 1 m solo toca los
    // vertices de su entorno; el resto del mapa ni se mira.
    const float Cell = W.CellSize();
    const float Flat = S.PathWidth * 0.5f + Cell;          // calzada + una celda: la cinta nunca queda enterrada
    const float Reach = Flat + S.PathShoulder;
    const size_t N = W.Height.size();
    W.PathDistance.assign(N, 1e6f);
    std::vector<float> Target(N, 0.0f);

    for (int32_t k = 0; k + 1 < NS; ++k)
    {
        const PathSample& P0 = P.Samples[k];
        const PathSample& P1 = P.Samples[k + 1];
        const float minX = std::min(P0.X, P1.X) - Reach, maxX = std::max(P0.X, P1.X) + Reach;
        const float minZ = std::min(P0.Z, P1.Z) - Reach, maxZ = std::max(P0.Z, P1.Z) + Reach;
        const float Half = W.WorldSize * 0.5f;
        const int32_t x0 = std::max(0, static_cast<int32_t>((minX + Half) / Cell));
        const int32_t x1 = std::min(W.Side - 1, static_cast<int32_t>((maxX + Half) / Cell) + 1);
        const int32_t y0 = std::max(0, static_cast<int32_t>((minZ + Half) / Cell));
        const int32_t y1 = std::min(W.Side - 1, static_cast<int32_t>((maxZ + Half) / Cell) + 1);

        const float sx = P1.X - P0.X, sz = P1.Z - P0.Z;
        const float L2 = sx * sx + sz * sz;
        for (int32_t y = y0; y <= y1; ++y)
        {
            for (int32_t x = x0; x <= x1; ++x)
            {
                const float X = W.GridToWorldX(x), Z = W.GridToWorldZ(y);
                float t = L2 > 0.0f ? ((X - P0.X) * sx + (Z - P0.Z) * sz) / L2 : 0.0f;
                t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
                const float dx = X - (P0.X + sx * t), dz = Z - (P0.Z + sz * t);
                const float d = std::sqrt(dx * dx + dz * dz);
                const int32_t i = W.Index(x, y);
                if (d < W.PathDistance[i])
                {
                    W.PathDistance[i] = d;
                    Target[i] = P0.H + (P1.H - P0.H) * t;
                }
            }
        }
    }

    for (size_t i = 0; i < N; ++i)
    {
        const float d = W.PathDistance[i];
        if (d >= Reach) continue;
        const float w = 1.0f - SmoothStep01(Flat, Reach, d);   // 1 en la calzada, 0 al final del arcen
        const float h = W.Height[i] * W.MaxHeight;
        float hn = (h + (Target[i] - h) * w) / W.MaxHeight;
        W.Height[i] = hn < 0.0f ? 0.0f : (hn > 1.0f ? 1.0f : hn);
    }
}

// ---------------------------------------------------------------- cinta

RibbonMesh PathRibbon::Build(const WorldData& W, float Lift)
{
    RibbonMesh M;
    if (!W.HasPath()) return M;

    const PathData& P = W.Path;
    const float HalfW = P.Width * 0.5f;
    M.Rows = static_cast<int32_t>(P.Samples.size());
    const size_t NV = static_cast<size_t>(M.Rows) * 2;
    M.PX.resize(NV); M.PZ.resize(NV); M.PY.resize(NV); M.U.resize(NV); M.V.resize(NV);

    for (int32_t r = 0; r < M.Rows; ++r)
    {
        const PathSample& S = P.Samples[r];
        // Perpendicular a la tangente, hacia la derecha: (tz, -tx). Con ella,
        // (derecha - izquierda) x tangente apunta hacia arriba, igual que
        // (+X) x (+Z) en la rejilla del terreno.
        const float AX = S.TZ, AZ = -S.TX;
        for (int32_t c = 0; c < 2; ++c)
        {
            const float Side = c == 0 ? -1.0f : 1.0f;
            const int32_t i = M.Index(c, r);
            M.PX[i] = S.X + AX * HalfW * Side;
            M.PZ[i] = S.Z + AZ * HalfW * Side;
            // Sobre el terreno aplanado, y nunca por debajo de la calzada.
            M.PY[i] = std::max(W.SampleHeightMeters(M.PX[i], M.PZ[i]), S.H) + Lift;
            M.U[i] = static_cast<float>(c);
            M.V[i] = S.S / P.Width;   // textura sin estirar: cuadrada
        }
    }
    return M;
}

} // namespace jardin
