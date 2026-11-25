// JardinGrass.cpp — Pasto denso por trozos.

#include "JardinGrass.h"

namespace jardin
{

GrassClumpMesh GrassClump::Build(const GrassClumpParams& P, uint32_t Seed)
{
    GrassClumpMesh M;
    DeterministicRandom Rng(Seed);
    const int32_t Blades = std::max(1, P.Blades);
    const int32_t Seg = std::max(1, P.Segments);
    constexpr float Deg = 0.017453293f;

    for (int32_t b = 0; b < Blades; ++b)
    {
        // Raiz: punto uniforme en el disco (raiz cuadrada del radio).
        const float Ang = Rng.NextFloat() * 6.2831853f;
        const float Rr = P.Radius * std::sqrt(Rng.NextFloat());
        const float BX = std::cos(Ang) * Rr, BY = std::sin(Ang) * Rr;

        // Se inclina hacia fuera de la mata (mas cuanto mas lejos del centro),
        // con algo de desorden en el rumbo.
        const float LeanYaw = Ang + (Rng.NextFloat() - 0.5f) * 1.2f;
        const float Lean = (0.25f + 0.75f * (Rr / std::max(P.Radius, 1e-4f))) * P.MaxLeanDeg * Deg
                         * (0.6f + 0.4f * Rng.NextFloat());
        const float H = P.Height * (1.0f + (Rng.NextFloat() * 2.0f - 1.0f) * P.HeightJitter);
        const float W = P.Width * (0.75f + 0.5f * Rng.NextFloat());
        // La cara de la brizna mira en un rumbo propio: vistas en grupo, unas
        // se ven de frente y otras de canto, como en un prado real.
        const float Face = Rng.NextFloat() * 3.1415926f;
        const float SX = std::cos(Face), SY = std::sin(Face);   // direccion "a lo ancho"
        const float Rand = Rng.NextFloat();

        const float DX = std::cos(LeanYaw), DY = std::sin(LeanYaw);
        const int32_t First = static_cast<int32_t>(M.PX.size());
        for (int32_t k = 0; k <= Seg; ++k)
        {
            const float t = static_cast<float>(k) / static_cast<float>(Seg);
            // Curva: la inclinacion crece hacia la punta (t^1.6), como una
            // brizna que se dobla por su propio peso.
            const float Out = std::sin(Lean) * H * std::pow(t, 1.6f);
            const float Up = std::cos(Lean * t) * H * t;
            const float CX = BX + DX * Out, CY = BY + DY * Out, CZ = Up;
            const float HalfW = 0.5f * W * (1.0f - t);   // en la punta, cero
            if (k < Seg)
            {
                for (int32_t s = 0; s < 2; ++s)
                {
                    const float Side = s == 0 ? -1.0f : 1.0f;
                    M.PX.push_back(CX + SX * HalfW * Side);
                    M.PY.push_back(CY + SY * HalfW * Side);
                    M.PZ.push_back(CZ);
                    M.U.push_back(static_cast<float>(s));
                    M.V.push_back(t);
                    M.BladeRand.push_back(Rand);
                }
            }
            else
            {
                M.PX.push_back(CX); M.PY.push_back(CY); M.PZ.push_back(CZ);
                M.U.push_back(0.5f); M.V.push_back(1.0f); M.BladeRand.push_back(Rand);
            }
        }

        // Caras: quads entre niveles y un triangulo en la punta; cada una por
        // los dos lados.
        auto Tri = [&M](int32_t a, int32_t b2, int32_t c)
        {
            M.Indices.insert(M.Indices.end(), { a, b2, c, a, c, b2 });
        };
        for (int32_t k = 0; k + 1 < Seg; ++k)
        {
            const int32_t L0 = First + 2 * k, R0 = L0 + 1, L1 = L0 + 2, R1 = L0 + 3;
            Tri(L0, R0, L1);
            Tri(R0, R1, L1);
        }
        const int32_t LT = First + 2 * (Seg - 1);
        Tri(LT, LT + 1, LT + 2);
    }
    M.BladeCount = Blades;
    return M;
}

void GrassField::BuildChunk(const WorldData& W, const SeedBook& Seeds, int32_t Cx, int32_t Cz,
                            float ChunkSize, const GrassParams& P, std::vector<GrassInstance>& Out)
{
    Out.clear();
    if (ChunkSize <= 0.0f || P.Density <= 0.0f) return;

    // Subsemilla del trozo: hash de (semilla del pasto, x, z). Por eso el trozo
    // no depende de nada mas que de su posicion.
    const uint32_t GrassSeed = Seeds.For(SeedBook::Grass);
    DeterministicRandom Rng(hash32::Coord(Cx, Cz, GrassSeed));
    const uint32_t PatchSeed = hash32::Combine(GrassSeed, 0x6A09E667u);

    const float Step = 1.0f / std::sqrt(P.Density);
    const int32_t N = std::max(1, static_cast<int32_t>(ChunkSize / Step + 0.5f));
    const float Cell = ChunkSize / static_cast<float>(N);
    const float X0 = static_cast<float>(Cx) * ChunkSize;
    const float Z0 = static_cast<float>(Cz) * ChunkSize;
    const float HalfPath = W.HasPath() ? W.Path.Width * 0.5f : 0.0f;
    const int32_t Variants = std::max(1, P.Variants);

    Out.reserve(static_cast<size_t>(N) * N / 2);
    for (int32_t j = 0; j < N; ++j)
    {
        for (int32_t i = 0; i < N; ++i)
        {
            // Siempre cinco numeros por candidato, se acepte o no.
            const float jx = Rng.NextFloat(), jz = Rng.NextFloat();
            const float Keep = Rng.NextFloat();
            const float Yaw = Rng.NextFloat() * 360.0f;
            const float Sc = Rng.NextFloat();

            const float X = X0 + (static_cast<float>(i) + jx) * Cell;
            const float Z = Z0 + (static_cast<float>(j) + jz) * Cell;
            if (!W.InsideBounds(X, Z, 1.0f)) continue;
            if (W.SampleSlope(X, Z) > P.MaxSlope) continue;
            if (W.SampleField(W.Height, X, Z) < 0.01f) continue;   // fondo del borde del mapa
            if (W.HasWater() && W.SampleField(W.WaterDistance, X, Z) < P.WaterClear) continue;
            if (W.HasPath() && W.SampleField(W.PathDistance, X, Z) < HalfPath + P.PathClear) continue;

            // Probabilidad de que crezca: manchas (ruido) x zona x humedad.
            // Entre PatchCut y PatchCut + 0.15 la mancha se desvanece: bordes suaves.
            const float u = (X + W.WorldSize * 0.5f) / W.WorldSize;
            const float v = (Z + W.WorldSize * 0.5f) / W.WorldSize;
            const float Patch = noise::Fbm01(u, v, PatchSeed, 3, P.PatchFrequency, 2.0f, 0.5f);
            float Prob = (Patch - P.PatchCut) / 0.15f;
            Prob = Prob < 0.0f ? 0.0f : (Prob > 1.0f ? 1.0f : Prob);
            if (W.SampleZone(X, Z) != 0) Prob *= P.ForestFactor;
            Prob *= 0.6f + 0.4f * W.SampleMoisture(X, Z);
            if (Keep >= Prob) continue;

            GrassInstance G;
            G.X = X; G.Z = Z;
            G.H = W.SampleHeightMeters(X, Z);
            G.Yaw = Yaw;
            // En el centro de las manchas el pasto es mas alto que en el borde.
            G.Scale = (P.MinScale + (P.MaxScale - P.MinScale) * Sc) * (0.75f + 0.25f * Prob);
            G.Variant = static_cast<int32_t>(hash32::Mix(static_cast<uint32_t>(Keep * 16777216.0f)) % static_cast<uint32_t>(Variants));
            Out.push_back(G);
        }
    }
}

} // namespace jardin
