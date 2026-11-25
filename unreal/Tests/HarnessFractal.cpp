// Pruebas del arbol fractal. Se compila con cl.exe, sin Unreal.

#include "JardinFractalTree.h"
#include <cstdio>
#include <cmath>
#include <chrono>

using namespace jardin;

static int failures = 0;

static void Check(bool ok, const char* label, const char* detail = nullptr)
{
    printf("  %s  %s", ok ? "PASA " : "FALLA", label);
    if (detail) printf("  -> %s", detail);
    printf("\n");
    if (!ok) ++failures;
}

int main()
{
    char buf[256];
    FractalTreeParams P;   // valores por defecto: 5 niveles, 3 hijas

    printf("=== F1. Determinismo ===\n");
    const TreeMesh A = FractalTree::Build(P, 1234u);
    const TreeMesh B = FractalTree::Build(P, 1234u);
    const TreeMesh C = FractalTree::Build(P, 1235u);
    snprintf(buf, sizeof(buf), "%08X / %08X", A.Checksum(), B.Checksum());
    Check(A.Checksum() == B.Checksum() && A.Positions.size() == B.Positions.size(),
          "misma semilla -> mismo arbol, vertice por vertice", buf);
    snprintf(buf, sizeof(buf), "%08X vs %08X", A.Checksum(), C.Checksum());
    Check(A.Checksum() != C.Checksum(), "semilla vecina -> arbol distinto", buf);

    printf("=== F2. La regla recursiva produce la estructura esperada ===\n");
    snprintf(buf, sizeof(buf), "ramas %d (esperadas %d), hojas %d (esperadas %d)",
             A.BranchCount, FractalTree::ExpectedBranchCount(P),
             A.LeafCount, FractalTree::ExpectedLeafCount(P));
    Check(A.BranchCount == FractalTree::ExpectedBranchCount(P)
          && A.LeafCount == FractalTree::ExpectedLeafCount(P),
          "ramas = 1+B+B^2+..., hojas = B^(n-1)", buf);

    // Otra configuracion, para no probar solo el caso por defecto.
    FractalTreeParams P2; P2.Depth = 4; P2.Branches = 2;
    const TreeMesh A2 = FractalTree::Build(P2, 99u);
    snprintf(buf, sizeof(buf), "ramas %d / hojas %d", A2.BranchCount, A2.LeafCount);
    Check(A2.BranchCount == 15 && A2.LeafCount == 8, "con 4 niveles y 2 hijas: 15 ramas, 8 hojas", buf);

    printf("=== F3. Geometria valida ===\n");
    const int32_t V = static_cast<int32_t>(A.Positions.size());
    bool indicesOk = A.Triangles.size() % 3 == 0;
    for (int32_t I : A.Triangles) if (I < 0 || I >= V) indicesOk = false;
    Check(indicesOk, "todos los indices apuntan a vertices existentes");

    const bool parallel = A.Normals.size() == A.Positions.size()
                       && A.Part.size() == A.Positions.size()
                       && A.Flex.size() == A.Positions.size();
    Check(parallel, "normales, partes y flex tienen un valor por vertice");

    int badNormals = 0, nonFinite = 0;
    for (size_t i = 0; i < A.Positions.size(); ++i)
    {
        const float L = A.Normals[i].Length();
        if (std::fabs(L - 1.0f) > 1e-3f) ++badNormals;
        const Vec3& p = A.Positions[i];
        if (!std::isfinite(p.X) || !std::isfinite(p.Y) || !std::isfinite(p.Z)) ++nonFinite;
    }
    snprintf(buf, sizeof(buf), "%d normales no unitarias, %d posiciones no finitas", badNormals, nonFinite);
    Check(badNormals == 0 && nonFinite == 0, "normales unitarias y sin NaN", buf);

    printf("=== F4. Convencion de caras (antihorario desde fuera) ===\n");
    // Para cada triangulo, (b-a)x(c-a) debe apuntar al mismo lado que la
    // normal del vertice. Si falla, en Unreal las caras se verian del reves.
    int wrong = 0, degenerate = 0;
    for (size_t t = 0; t < A.Triangles.size(); t += 3)
    {
        const Vec3& a = A.Positions[A.Triangles[t]];
        const Vec3& b = A.Positions[A.Triangles[t + 1]];
        const Vec3& c = A.Positions[A.Triangles[t + 2]];
        const Vec3 face = (b - a).Cross(c - a);
        if (face.Length() < 1e-9f) { ++degenerate; continue; }
        const Vec3 n = (A.Normals[A.Triangles[t]] + A.Normals[A.Triangles[t + 1]]
                      + A.Normals[A.Triangles[t + 2]]);
        if (face.Dot(n) <= 0.0f) ++wrong;
    }
    snprintf(buf, sizeof(buf), "%d caras invertidas, %d degeneradas de %d",
             wrong, degenerate, A.TriangleCount());
    Check(wrong == 0, "ninguna cara invertida", buf);

    printf("=== F5. Forma del arbol ===\n");
    float minZ = 1e9f, maxZ = -1e9f;
    for (const Vec3& p : A.Positions) { if (p.Z < minZ) minZ = p.Z; if (p.Z > maxZ) maxZ = p.Z; }
    snprintf(buf, sizeof(buf), "Z de %.2f a %.2f m (altura reportada %.2f)", minZ, maxZ, A.Height);
    Check(minZ > -0.6f && maxZ > P.TrunkLength, "nace en el suelo y crece hacia arriba", buf);
    Check(std::fabs(maxZ - A.Height) < 1e-3f, "Height coincide con el vertice mas alto");

    // Flex: 0 en la base del tronco, 1 en las hojas.
    float flexMin = 2.0f, flexLeaf = 2.0f;
    for (size_t i = 0; i < A.Flex.size(); ++i)
    {
        if (A.Flex[i] < flexMin) flexMin = A.Flex[i];
        if (A.Part[i] == static_cast<uint8_t>(TreePart::Leaf) && A.Flex[i] < flexLeaf) flexLeaf = A.Flex[i];
    }
    Check(flexMin == 0.0f && flexLeaf == 1.0f, "peso del viento: 0 en la base, 1 en las hojas");

    printf("=== F6. Presupuesto ===\n");
    FractalTreeParams Huge; Huge.Depth = 7; Huge.Branches = 5;
    const TreeMesh H = FractalTree::Build(Huge, 7u);
    snprintf(buf, sizeof(buf), "pedidas 19531 ramas, construidas %d", H.BranchCount);
    Check(H.BranchCount <= 2000, "Validate recorta arboles absurdos", buf);

    printf("=== F8. Estilo cerezo: ramas curvas, laterales, copa redondeada ===\n");
    {
        Check(A.Checksum() == 0x855A0CEEu, "el arbol de la primera version sigue identico (855A0CEE)");

        FractalTreeParams Z;
        Z.TrunkLength = 3.0f; Z.TrunkRadius = 0.42f; Z.LengthDecay = 0.74f; Z.RadiusDecay = 0.68f;
        Z.BranchAngleDeg = 38.0f; Z.AngleJitterDeg = 12.0f; Z.UpBias = 0.25f; Z.LeafSize = 1.1f;
        Z.SegmentsPerBranch = 3; Z.BendDeg = 12.0f; Z.BranchStart = 0.55f; Z.TrunkFlare = 0.5f;
        Z.LeafShape = 1; Z.LeafLevels = 2; Z.LeavesPerTip = 2; Z.LeafJitter = 0.25f;

        const TreeMesh C1 = FractalTree::Build(Z, 1234u);
        const TreeMesh C2 = FractalTree::Build(Z, 1234u);
        Check(C1.Checksum() == C2.Checksum(), "el estilo nuevo tambien es determinista");

        snprintf(buf, sizeof(buf), "ramas %d (esperadas %d), racimos %d (esperados %d)",
                 C1.BranchCount, FractalTree::ExpectedBranchCount(Z),
                 C1.LeafCount, FractalTree::ExpectedLeafCount(Z));
        Check(C1.BranchCount == FractalTree::ExpectedBranchCount(Z)
              && C1.LeafCount == FractalTree::ExpectedLeafCount(Z),
              "la regla recursiva sigue dando la estructura esperada", buf);

        int wrongZ = 0, badN = 0, badIdx = 0;
        const int32_t VZ = static_cast<int32_t>(C1.Positions.size());
        for (int32_t I : C1.Triangles) if (I < 0 || I >= VZ) ++badIdx;
        for (const Vec3& n : C1.Normals) if (std::fabs(n.Length() - 1.0f) > 1e-3f) ++badN;
        for (size_t t = 0; t + 2 < C1.Triangles.size(); t += 3)
        {
            const Vec3& a = C1.Positions[C1.Triangles[t]];
            const Vec3& b = C1.Positions[C1.Triangles[t + 1]];
            const Vec3& c = C1.Positions[C1.Triangles[t + 2]];
            const Vec3 face = (b - a).Cross(c - a);
            if (face.Length() < 1e-9f) continue;
            const Vec3 n = C1.Normals[C1.Triangles[t]] + C1.Normals[C1.Triangles[t + 1]] + C1.Normals[C1.Triangles[t + 2]];
            if (face.Dot(n) <= 0.0f) ++wrongZ;
        }
        snprintf(buf, sizeof(buf), "%d invertidas, %d normales malas, %d indices malos de %d caras",
                 wrongZ, badN, badIdx, C1.TriangleCount());
        Check(wrongZ == 0 && badN == 0 && badIdx == 0, "geometria valida en el estilo nuevo", buf);

        // El tronco ya no es un alambre: radio en la base (con el ensanchamiento).
        float baseR = 0.0f;
        for (size_t i = 0; i < C1.Positions.size(); ++i)
        {
            const Vec3& p = C1.Positions[i];
            if (C1.Part[i] == static_cast<uint8_t>(TreePart::Wood) && p.Z < 0.0f)
            {
                const float r = std::sqrt(p.X * p.X + p.Y * p.Y);
                if (r > baseR) baseR = r;
            }
        }
        snprintf(buf, sizeof(buf), "radio en la base %.2f m (antes %.2f m), altura %.1f m, %d triangulos",
                 baseR, P.TrunkRadius, C1.Height, C1.TriangleCount());
        Check(baseR > 0.5f, "tronco grueso con la base ensanchada", buf);
    }

    printf("=== F9. Variacion de forma entre ejemplares ===\n");
    {
        Check(FractalTree::Build(FractalTree::Vary(P, 42u, 0.0f), 1234u).Checksum() == A.Checksum(),
              "con variacion 0, el arbol no cambia");

        const FractalTreeParams V1 = FractalTree::Vary(P, 42u, 0.6f);
        const FractalTreeParams V2 = FractalTree::Vary(P, 42u, 0.6f);
        const FractalTreeParams V3 = FractalTree::Vary(P, 43u, 0.6f);
        Check(V1.BranchAngleDeg == V2.BranchAngleDeg && V1.LengthDecay == V2.LengthDecay
              && V1.TrunkLength == V2.TrunkLength && V1.Branches == V2.Branches,
              "la variacion es determinista");
        Check(V1.BranchAngleDeg != V3.BranchAngleDeg || V1.TrunkLength != V3.TrunkLength,
              "otra semilla da otra forma");

        // Con 6 niveles no se agrega rama por nudo: protegeria el presupuesto.
        FractalTreeParams Deep = P; Deep.Depth = 6;
        bool grew = false;
        for (uint32_t s = 0; s < 200; ++s) if (FractalTree::Vary(Deep, s, 1.0f).Branches != Deep.Branches) grew = true;
        Check(!grew, "en arboles de 6 niveles la variacion no multiplica las ramas");

        int extra = 0;
        for (uint32_t s = 0; s < 200; ++s) if (FractalTree::Vary(P, s, 1.0f).Branches == P.Branches + 1) ++extra;
        snprintf(buf, sizeof(buf), "%d de 200 con 4 ramas por nudo", extra);
        Check(extra > 30 && extra < 110, "en arboles de 5 niveles, ~35% ramifica en 4", buf);
    }

    printf("=== F7. Rendimiento ===\n");
    const auto t0 = std::chrono::high_resolution_clock::now();
    uint32_t sink = 0;
    for (uint32_t s = 0; s < 24; ++s) sink ^= FractalTree::Build(P, s).Checksum();
    const auto t1 = std::chrono::high_resolution_clock::now();
    const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    printf("        24 arboles distintos en %.2f ms  (%d ramas, %d triangulos cada uno)  [%08X]\n",
           ms, A.BranchCount, A.TriangleCount(), sink);
    Check(ms < 200.0, "construir 24 arboles cuesta menos de 200 ms");

    printf("\n%s\n", failures == 0 ? "TODAS LAS PRUEBAS PASAN" : "HUBO FALLAS");
    return failures == 0 ? 0 : 1;
}
