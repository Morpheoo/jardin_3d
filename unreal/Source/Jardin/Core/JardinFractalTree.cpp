// JardinFractalTree.cpp — Construccion recursiva del arbol.

#include "JardinFractalTree.h"

namespace jardin
{

namespace
{
    constexpr float Pi = 3.14159265f;
    constexpr float DegToRad = Pi / 180.0f;

    template <typename T>
    T ClampT(T V, T Lo, T Hi) { return V < Lo ? Lo : (V > Hi ? Hi : V); }

    int32_t IntPow(int32_t Base, int32_t Exp)
    {
        int32_t R = 1;
        for (int32_t i = 0; i < Exp; ++i) R *= Base;
        return R;
    }
}

// ---------------------------------------------------------------- params

void FractalTreeParams::Validate()
{
    Branches = ClampT(Branches, 1, 5);
    Depth = ClampT(Depth, 1, 7);

    // Presupuesto: no mas de ~2000 ramas por arbol. Con 3 hijos y 7 niveles
    // serian 1093; con 5 hijos y 7 niveles, 19531. Se recorta la profundidad.
    while (Depth > 1 && FractalTree::ExpectedBranchCount(*this) > 2000)
    {
        --Depth;
    }

    RadialSegments = ClampT(RadialSegments, 3, 16);

    SegmentsPerBranch = ClampT(SegmentsPerBranch, 1, 6);   // Branch() guarda 7 puntos
    BendDeg = ClampT(BendDeg, 0.0f, 45.0f);
    BranchStart = ClampT(BranchStart, 0.0f, 1.0f);
    TrunkFlare = ClampT(TrunkFlare, 0.0f, 2.0f);
    LeafShape = ClampT(LeafShape, 0, 1);
    LeafLevels = ClampT(LeafLevels, 1, Depth);
    LeavesPerTip = ClampT(LeavesPerTip, 1, 6);
    LeafJitter = ClampT(LeafJitter, 0.0f, 0.5f);

    TrunkLength = ClampT(TrunkLength, 0.1f, 50.0f);
    TrunkRadius = ClampT(TrunkRadius, 0.01f, 5.0f);
    LengthDecay = ClampT(LengthDecay, 0.1f, 0.95f);
    RadiusDecay = ClampT(RadiusDecay, 0.1f, 0.95f);
    BranchAngleDeg = ClampT(BranchAngleDeg, 0.0f, 89.0f);
    AngleJitterDeg = ClampT(AngleJitterDeg, 0.0f, 45.0f);
    LengthJitter = ClampT(LengthJitter, 0.0f, 0.5f);
    UpBias = ClampT(UpBias, 0.0f, 2.0f);
    LeafSize = ClampT(LeafSize, 0.0f, 10.0f);
}

FractalTreeParams FractalTree::Vary(const FractalTreeParams& Base, uint32_t Seed, float Amount)
{
    FractalTreeParams P = Base;
    if (Amount <= 0.0f) return P;
    if (Amount > 1.0f) Amount = 1.0f;

    // Un numero en [-0.5, 0.5) por rasgo, cada uno con su propia clave: los
    // rasgos varian de forma independiente entre si.
    auto H = [Seed](uint32_t Key) { return hash32::ToUnit(hash32::Combine(Seed, Key)) - 0.5f; };

    P.BranchAngleDeg *= 1.0f + H(1) * 0.40f * Amount;   // +-20%: copas mas cerradas o abiertas
    P.LengthDecay    += H(2) * 0.08f * Amount;          // +-0.04: mas o menos alargado
    P.UpBias         += H(3) * 0.40f * Amount;          // mas erguido o mas extendido
    P.TrunkLength    *= 1.0f + H(4) * 0.30f * Amount;   // +-15% de altura del tronco
    P.LeafSize       *= 1.0f + H(6) * 0.30f * Amount;   // racimos mas grandes o mas chicos

    // Algunos ejemplares ramifican en una mas por nudo: copa mas tupida.
    // Solo en arboles de hasta 5 niveles: con 6, pasar de 3 a 4 ramas por nudo
    // multiplica la geometria por casi 4 (de 364 a 1365 ramas).
    if (P.Depth <= 5 && H(5) + 0.5f < 0.35f * Amount) P.Branches += 1;

    // Build() llama a Validate(): cualquier exceso queda acotado ahi.
    return P;
}

int32_t FractalTree::ExpectedBranchCount(const FractalTreeParams& P)
{
    int32_t Total = 0;
    for (int32_t k = 0; k < P.Depth; ++k) Total += IntPow(P.Branches, k);
    return Total;
}

int32_t FractalTree::ExpectedLeafCount(const FractalTreeParams& P)
{
    int32_t Tips = 0;
    for (int32_t k = P.Depth - P.LeafLevels; k < P.Depth; ++k) Tips += IntPow(P.Branches, k);
    return Tips * P.LeavesPerTip;
}

// ---------------------------------------------------------------- mesh

uint32_t TreeMesh::Checksum() const
{
    uint32_t h = 2166136261u;
    for (const Vec3& V : Positions)
    {
        // Cuantizado al milimetro, desplazado para que no haya negativos.
        h = hash32::Combine(h, static_cast<uint32_t>(static_cast<int32_t>(V.X * 1000.0f) + 1000000));
        h = hash32::Combine(h, static_cast<uint32_t>(static_cast<int32_t>(V.Y * 1000.0f) + 1000000));
        h = hash32::Combine(h, static_cast<uint32_t>(static_cast<int32_t>(V.Z * 1000.0f) + 1000000));
    }
    for (int32_t I : Triangles) h = hash32::Combine(h, static_cast<uint32_t>(I));
    return h;
}

// ---------------------------------------------------------------- builder

namespace
{
    class Builder
    {
    public:
        Builder(const FractalTreeParams& InP, uint32_t Seed, TreeMesh& InM)
            : P(InP), Rng(Seed), M(InM) {}

        void Grow()
        {
            // El tronco arranca un poco bajo el suelo: en una ladera, la base
            // no debe quedar flotando por el lado de abajo.
            Branch(Vec3(0.0f, 0.0f, -0.15f), Vec3(0.0f, 0.0f, 1.0f),
                   P.TrunkLength, P.TrunkRadius, 0);
        }

    private:
        const FractalTreeParams& P;
        DeterministicRandom Rng;
        TreeMesh& M;

        /** Dos ejes perpendiculares a Dir que forman con el una base de mano
         *  derecha: U x V = Dir. */
        static void Basis(const Vec3& Dir, Vec3& U, Vec3& V)
        {
            const Vec3 Helper = std::fabs(Dir.Z) < 0.9f ? Vec3(0.0f, 0.0f, 1.0f) : Vec3(1.0f, 0.0f, 0.0f);
            U = Dir.Cross(Helper).Normalized();
            V = Dir.Cross(U);
        }

        float FlexOf(int32_t Level) const
        {
            return static_cast<float>(Level) / static_cast<float>(P.Depth);
        }

        /** Inclina Dir un angulo aleatorio de hasta MaxDeg en una direccion aleatoria. */
        Vec3 Bend(const Vec3& Dir, float MaxDeg)
        {
            Vec3 U, V;
            Basis(Dir, U, V);
            const float A = Rng.Range(0.0f, MaxDeg) * DegToRad;
            const float Phi = Rng.Range(0.0f, 2.0f * Pi);
            return (Dir * std::cos(A) + (U * std::cos(Phi) + V * std::sin(Phi)) * std::sin(A)).Normalized();
        }

        // La regla recursiva del fractal.
        //
        // Con las opciones nuevas en sus valores por defecto (1 tramo, sin
        // curvatura, hojas solo al final, todas las hijas en la punta) esta
        // funcion consume los numeros aleatorios en el MISMO orden que la
        // version original, y produce el mismo arbol vertice por vertice.
        void Branch(const Vec3& Base, const Vec3& Dir, float Length, float Radius, int32_t Level)
        {
            const float TipRadius = Radius * P.RadiusDecay;
            const int32_t Segs = P.SegmentsPerBranch;

            // Puntos de la rama, de la base a la punta. Con varios tramos cada
            // uno se desvia un poco del anterior: ramas curvas, algo nudosas.
            Vec3 Pts[8];
            Pts[0] = Base;
            Vec3 SegDir = Dir;
            for (int32_t s = 0; s < Segs; ++s)
            {
                if (s > 0 && P.BendDeg > 0.0f) SegDir = Bend(SegDir, P.BendDeg);

                Pts[s + 1] = Pts[s] + SegDir * (Length / static_cast<float>(Segs));

                const float t0 = static_cast<float>(s) / static_cast<float>(Segs);
                const float t1 = static_cast<float>(s + 1) / static_cast<float>(Segs);
                float R0 = (s == 0) ? Radius : Radius + (TipRadius - Radius) * t0;
                const float R1 = (s == Segs - 1) ? TipRadius : Radius + (TipRadius - Radius) * t1;

                // Base del tronco ensanchada, como las raices de un arbol real.
                if (Level == 0 && s == 0) R0 *= 1.0f + P.TrunkFlare;

                const float F0 = FlexOf(Level), F1 = FlexOf(Level + 1);
                AddSegment(Pts[s], Pts[s + 1], SegDir, R0,  R1,
                           (s == 0) ? F0 : F0 + (F1 - F0) * t0,
                           (s == Segs - 1) ? F1 : F0 + (F1 - F0) * t1);
            }
            const Vec3 Tip = Pts[Segs];
            const Vec3 TipDir = SegDir;
            ++M.BranchCount;

            // Hojas en los ultimos LeafLevels niveles: con mas de uno, la copa
            // se rellena hacia dentro y deja de verse hueca.
            if (Level >= P.Depth - P.LeafLevels)
            {
                for (int32_t k = 0; k < P.LeavesPerTip; ++k)
                {
                    Vec3 C = Tip;
                    float Scale = 1.0f;
                    if (k > 0)
                    {
                        // Racimos extra alrededor de la punta: copa mas densa.
                        const float Az = Rng.Range(0.0f, 2.0f * Pi);
                        const float El = Rng.Range(-0.4f, 0.9f);
                        C = Tip + Vec3(std::cos(Az) * std::cos(El), std::sin(Az) * std::cos(El), std::sin(El))
                                  * (P.LeafSize * 0.7f);
                        Scale = 0.8f;
                    }
                    AddLeaf(C, P.LeafSize * Scale * Rng.Range(0.8f, 1.2f));
                    ++M.LeafCount;
                }
            }

            // Caso base: ultimo nivel.
            if (Level >= P.Depth - 1)
            {
                return;
            }

            Vec3 U, V;
            Basis(TipDir, U, V);

            const float BaseAzimuth = Rng.Range(0.0f, 2.0f * Pi);
            const float Step = 2.0f * Pi / static_cast<float>(P.Branches);

            for (int32_t i = 0; i < P.Branches; ++i)
            {
                // La primera hija prolonga el eje casi recta; las demas se abren.
                // Sin esto sale un "brocoli" sin tronco dominante.
                const bool bLeader = (i == 0);
                const float Theta = ((bLeader ? P.BranchAngleDeg * 0.35f : P.BranchAngleDeg)
                                   + Rng.Range(-P.AngleJitterDeg, P.AngleJitterDeg)) * DegToRad;
                const float Phi = BaseAzimuth + Step * static_cast<float>(i)
                                + Rng.Range(-0.25f, 0.25f) * Step;

                // Direccion de la hija: inclinada Theta respecto a la madre,
                // girada Phi alrededor de ella. Es una rotacion en coordenadas
                // esfericas sobre la base local (U, V, Dir).
                Vec3 Child = TipDir * std::cos(Theta)
                           + (U * std::cos(Phi) + V * std::sin(Phi)) * std::sin(Theta);

                // Fototropismo: sesgo hacia arriba.
                Child = (Child + Vec3(0.0f, 0.0f, P.UpBias)).Normalized();

                const float ChildLength = Length * P.LengthDecay
                                        * (1.0f + Rng.Range(-P.LengthJitter, P.LengthJitter));

                // La hija nace un poco dentro de la madre para tapar la union.
                Vec3 ChildBase = Tip - TipDir * (TipRadius * 0.5f);

                // Ramas laterales: las hijas que no son el eje nacen a lo largo
                // de la madre, no todas en la punta. Sin esto la copa es un paraguas.
                if (!bLeader && P.BranchStart < 1.0f)
                {
                    const float t = Rng.Range(P.BranchStart, 1.0f) * static_cast<float>(Segs);
                    int32_t Seg = static_cast<int32_t>(t);
                    if (Seg >= Segs) Seg = Segs - 1;
                    ChildBase = Pts[Seg] + (Pts[Seg + 1] - Pts[Seg]) * (t - static_cast<float>(Seg));
                }

                Branch(ChildBase, Child, ChildLength, TipRadius, Level + 1);
            }
        }

        /** Tronco de cono entre Base y Tip, con normales radiales. */
        void AddSegment(const Vec3& Base, const Vec3& Tip, const Vec3& Dir,
                        float R0, float R1, float Flex0, float Flex1)
        {
            Vec3 U, V;
            Basis(Dir, U, V);

            const int32_t N = P.RadialSegments;
            const int32_t First = static_cast<int32_t>(M.Positions.size());

            for (int32_t k = 0; k < N; ++k)
            {
                const float A = 2.0f * Pi * static_cast<float>(k) / static_cast<float>(N);
                const Vec3 Radial = U * std::cos(A) + V * std::sin(A);

                M.Positions.push_back(Base + Radial * R0);
                M.Normals.push_back(Radial);
                M.Part.push_back(static_cast<uint8_t>(TreePart::Wood));
                M.Flex.push_back(Flex0);

                M.Positions.push_back(Tip + Radial * R1);
                M.Normals.push_back(Radial);
                M.Part.push_back(static_cast<uint8_t>(TreePart::Wood));
                M.Flex.push_back(Flex1);
            }

            // Con (U, V, Dir) de mano derecha y el angulo creciendo de U hacia V,
            // (b0, b1, t1) y (b0, t1, t0) quedan antihorarios vistos desde fuera.
            for (int32_t k = 0; k < N; ++k)
            {
                const int32_t kn = (k + 1) % N;
                const int32_t B0 = First + 2 * k,  T0 = B0 + 1;
                const int32_t B1 = First + 2 * kn, T1 = B1 + 1;

                M.Triangles.push_back(B0); M.Triangles.push_back(B1); M.Triangles.push_back(T1);
                M.Triangles.push_back(B0); M.Triangles.push_back(T1); M.Triangles.push_back(T0);
            }

            if (Tip.Z > M.Height) M.Height = Tip.Z;
        }

        void AddLeaf(const Vec3& Center, float Size)
        {
            if (P.LeafShape == 1) AddLeafRound(Center, Size);
            else                  AddLeafOctahedron(Center, Size);
        }

        /**
         * Racimo redondeado: icosaedro (20 caras) con cada vertice desplazado al
         * azar. A diferencia del octaedro, no tiene puntas: se lee como una mata
         * de hojas y no como un cristal. Caras planas, estetica low-poly.
         */
        void AddLeafRound(const Vec3& Center, float Size)
        {
            const float SX = Size * Rng.Range(0.85f, 1.15f);
            const float SY = Size * Rng.Range(0.85f, 1.15f);
            const float SZ = Size * Rng.Range(0.65f, 0.90f);
            const Vec3 C = Center + Vec3(0.0f, 0.0f, SZ * 0.30f);

            const float g = 1.6180340f;   // razon aurea: vertices del icosaedro
            static const float Base[12][3] = {
                {-1, g, 0}, { 1, g, 0}, {-1,-g, 0}, { 1,-g, 0},
                { 0,-1, g}, { 0, 1, g}, { 0,-1,-g}, { 0, 1,-g},
                { g, 0,-1}, { g, 0, 1}, {-g, 0,-1}, {-g, 0, 1} };
            static const int32_t Faces[20][3] = {
                {0,11,5},{0,5,1},{0,1,7},{0,7,10},{0,10,11},{1,5,9},{5,11,4},{11,10,2},{10,7,6},{7,1,8},
                {3,9,4},{3,4,2},{3,2,6},{3,6,8},{3,8,9},{4,9,5},{2,4,11},{6,2,10},{8,6,7},{9,8,1} };

            Vec3 Pv[12];
            for (int32_t i = 0; i < 12; ++i)
            {
                const Vec3 N = Vec3(Base[i][0], Base[i][1], Base[i][2]).Normalized();
                const float J = 1.0f + Rng.Range(-P.LeafJitter, P.LeafJitter);
                Pv[i] = C + Vec3(N.X * SX, N.Y * SY, N.Z * SZ) * J;
            }

            for (int32_t f = 0; f < 20; ++f)
            {
                Vec3 A = Pv[Faces[f][0]], B = Pv[Faces[f][1]], D = Pv[Faces[f][2]];
                Vec3 Normal = (B - A).Cross(D - A);
                // Se comprueba la orientacion en vez de confiar en la tabla: con
                // los vertices desplazados, la cara debe seguir mirando hacia fuera.
                const Vec3 Mid = (A + B + D) * (1.0f / 3.0f);
                if (Normal.Dot(Mid - C) < 0.0f) { const Vec3 T = B; B = D; D = T; Normal = Normal * -1.0f; }
                Normal = Normal.Normalized();

                const int32_t First = static_cast<int32_t>(M.Positions.size());
                const Vec3 Tri[3] = { A, B, D };
                for (const Vec3& Pt : Tri)
                {
                    M.Positions.push_back(Pt);
                    M.Normals.push_back(Normal);
                    M.Part.push_back(static_cast<uint8_t>(TreePart::Leaf));
                    M.Flex.push_back(1.0f);
                    if (Pt.Z > M.Height) M.Height = Pt.Z;
                }
                M.Triangles.push_back(First);
                M.Triangles.push_back(First + 1);
                M.Triangles.push_back(First + 2);
            }
        }

        /** Racimo de hojas low-poly: un octaedro deformado, con caras planas. */
        void AddLeafOctahedron(const Vec3& Center, float Size)
        {
            // Semiejes distintos para que ningun racimo sea identico.
            const float SX = Size * Rng.Range(0.8f, 1.2f);
            const float SY = Size * Rng.Range(0.8f, 1.2f);
            const float SZ = Size * Rng.Range(0.6f, 0.9f);   // algo achatado

            // El racimo se desplaza un poco hacia arriba: cuelga menos de la rama.
            const Vec3 C = Center + Vec3(0.0f, 0.0f, SZ * 0.35f);

            for (int32_t sx = -1; sx <= 1; sx += 2)
            for (int32_t sy = -1; sy <= 1; sy += 2)
            for (int32_t sz = -1; sz <= 1; sz += 2)
            {
                Vec3 A = C + Vec3(SX * static_cast<float>(sx), 0.0f, 0.0f);
                Vec3 B = C + Vec3(0.0f, SY * static_cast<float>(sy), 0.0f);
                Vec3 D = C + Vec3(0.0f, 0.0f, SZ * static_cast<float>(sz));

                // Para la cara del octante (sx, sy, sz), el orden (A, B, D) es
                // antihorario desde fuera solo si sx*sy*sz > 0. Si no, se invierte.
                if (sx * sy * sz < 0) { const Vec3 Tmp = B; B = D; D = Tmp; }

                const Vec3 Normal = (B - A).Cross(D - A).Normalized();
                const int32_t First = static_cast<int32_t>(M.Positions.size());

                const Vec3 Tri[3] = { A, B, D };
                for (const Vec3& Pt : Tri)
                {
                    M.Positions.push_back(Pt);
                    M.Normals.push_back(Normal);   // caras planas: estetica low-poly
                    M.Part.push_back(static_cast<uint8_t>(TreePart::Leaf));
                    M.Flex.push_back(1.0f);
                }

                M.Triangles.push_back(First);
                M.Triangles.push_back(First + 1);
                M.Triangles.push_back(First + 2);

                if (MaxZ(A, B, D) > M.Height) M.Height = MaxZ(A, B, D);
            }
        }

        static float MaxZ(const Vec3& A, const Vec3& B, const Vec3& C)
        {
            float H = A.Z;
            if (B.Z > H) H = B.Z;
            if (C.Z > H) H = C.Z;
            return H;
        }
    };
}

TreeMesh FractalTree::Build(FractalTreeParams Params, uint32_t Seed)
{
    Params.Validate();

    TreeMesh M;

    // Reserva aproximada: evita realojar los vectores cientos de veces.
    const int32_t Branches = ExpectedBranchCount(Params);
    const int32_t Leaves = ExpectedLeafCount(Params);
    // Por racimo: 24 vertices el octaedro, 60 el icosaedro (caras planas).
    const size_t LeafVerts = Params.LeafShape == 1 ? 60 : 24;
    const size_t Segs = static_cast<size_t>(Params.SegmentsPerBranch);
    const size_t Verts = static_cast<size_t>(Branches) * Segs * Params.RadialSegments * 2
                       + static_cast<size_t>(Leaves) * LeafVerts;
    M.Positions.reserve(Verts);
    M.Normals.reserve(Verts);
    M.Part.reserve(Verts);
    M.Flex.reserve(Verts);
    M.Triangles.reserve(static_cast<size_t>(Branches) * Segs * Params.RadialSegments * 6
                        + static_cast<size_t>(Leaves) * LeafVerts);

    Builder B(Params, Seed, M);
    B.Grow();

    return M;
}

} // namespace jardin
