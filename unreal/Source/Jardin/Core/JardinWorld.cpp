// JardinWorld.cpp — Campos del mundo, clasificacion de zonas y orquestador.

#include "JardinWorld.h"
#include "JardinWater.h"
#include "JardinPath.h"
#include <chrono>

namespace jardin
{

// ---------------------------------------------------------------- settings

void GenerationSettings::Validate()
{
    if (WorldSize < 16.0f)   WorldSize = 16.0f;
    if (WorldSize > 1024.0f) WorldSize = 1024.0f;

    if (Resolution < 8)   Resolution = 8;
    if (Resolution > 512) Resolution = 512;

    if (MaxHeight < 0.0f) MaxHeight = 0.0f;

    if (Frequency < 0.01f)   Frequency = 0.01f;
    if (Octaves < 1)         Octaves = 1;
    if (Octaves > 12)        Octaves = 12;
    if (Lacunarity < 1.01f)  Lacunarity = 1.01f;
    if (Gain <= 0.0f)        Gain = 0.01f;
    if (Gain > 1.0f)         Gain = 1.0f;
    if (HeightExponent < 0.1f) HeightExponent = 0.1f;

    if (WarpStrength < 0.0f) WarpStrength = 0.0f;
    if (WarpStrength > 1.0f) WarpStrength = 1.0f;
    if (WarpFrequency < 0.01f) WarpFrequency = 0.01f;
    if (RidgeWeight < 0.0f) RidgeWeight = 0.0f;
    if (RidgeWeight > 1.0f) RidgeWeight = 1.0f;
    if (MountainFrequency < 0.01f) MountainFrequency = 0.01f;
    if (TerraceWeight < 0.0f) TerraceWeight = 0.0f;
    if (TerraceWeight > 1.0f) TerraceWeight = 1.0f;
    if (TerraceSteps < 1.0f) TerraceSteps = 1.0f;
    if (TerraceSteps > 32.0f) TerraceSteps = 32.0f;
    if (TerraceFrequency < 0.01f) TerraceFrequency = 0.01f;

    if (PathMode < 0) PathMode = 0;
    if (PathMode > 1) PathMode = 1;
    if (PathWidth < 0.5f) PathWidth = 0.5f;
    if (PathWidth > 20.0f) PathWidth = 20.0f;
    if (PathShoulder < 0.0f) PathShoulder = 0.0f;
    if (PathControlSpacing < 5.0f) PathControlSpacing = 5.0f;

    if (WaterMode < 0) WaterMode = 0;
    if (WaterMode > 7) WaterMode = 7;
    if (WaterMinAreaM2 < 0.0f) WaterMinAreaM2 = 0.0f;
    if (WaterMoistureBoost < 0.0f) WaterMoistureBoost = 0.0f;
    if (WaterMoistureBoost > 1.0f) WaterMoistureBoost = 1.0f;
    if (WaterMoistureRadius < 0.5f) WaterMoistureRadius = 0.5f;

    if (MoistureFrequency < 0.01f)  MoistureFrequency = 0.01f;
    if (MoistureOctaves < 1)        MoistureOctaves = 1;
    if (MoistureOctaves > 8)        MoistureOctaves = 8;
    if (MoistureLacunarity < 1.01f) MoistureLacunarity = 1.01f;
    if (MoistureGain <= 0.0f)       MoistureGain = 0.01f;
    if (MoistureGain > 1.0f)        MoistureGain = 1.0f;
}

// ---------------------------------------------------------------- world data

WorldData::WorldData(int32_t InResolution, float InWorldSize, float InMaxHeight, uint32_t InMasterSeed)
    : Resolution(InResolution)
    , Side(InResolution + 1)
    , WorldSize(InWorldSize)
    , MaxHeight(InMaxHeight)
    , MasterSeed(InMasterSeed)
{
    const size_t Count = static_cast<size_t>(Side) * static_cast<size_t>(Side);
    Height.assign(Count, 0.0f);
    Moisture.assign(Count, 0.0f);
    Slope.assign(Count, 0.0f);
    Zone.assign(Count, 0);
    ZoneBlend.assign(Count, 0.0f);
}

namespace
{
    inline float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
    inline float LerpF(float a, float b, float t) { return a + (b - a) * t; }
}

float WorldData::SampleField(const std::vector<float>& Field, float WorldX, float WorldZ) const
{
    const float u = Clamp01((WorldX + WorldSize * 0.5f) / WorldSize) * static_cast<float>(Resolution);
    const float v = Clamp01((WorldZ + WorldSize * 0.5f) / WorldSize) * static_cast<float>(Resolution);

    int32_t x0 = static_cast<int32_t>(u);
    int32_t y0 = static_cast<int32_t>(v);
    if (x0 >= Resolution) x0 = Resolution - 1;
    if (y0 >= Resolution) y0 = Resolution - 1;

    const float tx = u - static_cast<float>(x0);
    const float ty = v - static_cast<float>(y0);

    const float a = Field[Index(x0,     y0)];
    const float b = Field[Index(x0 + 1, y0)];
    const float c = Field[Index(x0,     y0 + 1)];
    const float d = Field[Index(x0 + 1, y0 + 1)];

    return LerpF(LerpF(a, b, tx), LerpF(c, d, tx), ty);
}

uint8_t WorldData::SampleZone(float WorldX, float WorldZ) const
{
    const float u = Clamp01((WorldX + WorldSize * 0.5f) / WorldSize) * static_cast<float>(Resolution);
    const float v = Clamp01((WorldZ + WorldSize * 0.5f) / WorldSize) * static_cast<float>(Resolution);

    int32_t x = static_cast<int32_t>(u + 0.5f);
    int32_t y = static_cast<int32_t>(v + 0.5f);
    if (x > Resolution) x = Resolution;
    if (y > Resolution) y = Resolution;

    return Zone[Index(x, y)];
}

uint32_t WorldData::Checksum() const
{
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < Height.size(); ++i)
    {
        // Cuantizar evita que una diferencia irrelevante en el ultimo bit
        // dispare una falsa alarma.
        const uint32_t q = static_cast<uint32_t>(Height[i] * 65535.0f);
        h = hash32::Combine(h, q);
        h = hash32::Combine(h, static_cast<uint32_t>(Zone[i]));
    }
    // El agua entra en la huella solo si existe: sin agua, el checksum es el
    // de siempre (64C68E3D sigue valiendo).
    for (size_t i = 0; i < WaterLevel.size(); ++i)
    {
        const uint32_t q = WaterLevel[i] < 0.0f ? 0xFFFFFFFFu : static_cast<uint32_t>(WaterLevel[i] * 65535.0f);
        h = hash32::Combine(h, q);
    }
    return h;
}

// ---------------------------------------------------------------- zones

ZoneSet ZoneSet::Default()
{
    ZoneSet Set;
    Set.BlendScale = 0.25f;

    ZoneRule Pradera;
    Pradera.Name = "Pradera";  Pradera.Id = ZoneSet::Pradera;  Pradera.Bias = 0.05f;
    Pradera.HeightCenter   = 0.35f; Pradera.HeightWidth   = 0.65f; Pradera.HeightWeight   = 0.8f;
    Pradera.MoistureCenter = 0.30f; Pradera.MoistureWidth = 0.55f; Pradera.MoistureWeight = 1.2f;
    Pradera.SlopeCenter    = 0.00f; Pradera.SlopeWidth    = 0.50f; Pradera.SlopeWeight    = 0.6f;

    ZoneRule Bosque;
    Bosque.Name = "Bosque";    Bosque.Id = ZoneSet::Bosque;    Bosque.Bias = 0.0f;
    Bosque.HeightCenter   = 0.55f; Bosque.HeightWidth   = 0.65f; Bosque.HeightWeight   = 0.8f;
    Bosque.MoistureCenter = 0.75f; Bosque.MoistureWidth = 0.55f; Bosque.MoistureWeight = 1.2f;
    Bosque.SlopeCenter    = 0.15f; Bosque.SlopeWidth    = 0.60f; Bosque.SlopeWeight    = 0.6f;

    Set.Rules.push_back(Pradera);
    Set.Rules.push_back(Bosque);
    return Set;
}

const char* ZoneSet::NameOf(uint8_t Id) const
{
    for (const ZoneRule& R : Rules)
        if (R.Id == Id) return R.Name;
    return "?";
}

void ZoneClassifier::Classify(WorldData& World, const ZoneSet& Zones)
{
    if (Zones.Rules.empty()) return;

    const float BlendScale = Zones.BlendScale > 0.0001f ? Zones.BlendScale : 0.25f;
    const size_t Count = World.Height.size();

    for (size_t i = 0; i < Count; ++i)
    {
        const float h = World.Height[i];
        const float m = World.Moisture[i];
        const float s = World.Slope[i];

        float BestScore = -1e30f;
        float SecondScore = -1e30f;
        uint8_t BestId = Zones.Rules[0].Id;

        for (const ZoneRule& R : Zones.Rules)
        {
            const float Score = R.Score(h, m, s);
            if (Score > BestScore)
            {
                SecondScore = BestScore;
                BestScore = Score;
                BestId = R.Id;
            }
            else if (Score > SecondScore)
            {
                SecondScore = Score;
            }
        }

        World.Zone[i] = BestId;

        // Con una sola regla no hay segundo lugar: la zona es total.
        const float Margin = (SecondScore <= -1e29f)
                           ? 1.0f
                           : (BestScore - SecondScore) / BlendScale;

        World.ZoneBlend[i] = Margin < 0.0f ? 0.0f : (Margin > 1.0f ? 1.0f : Margin);
    }
}

// ---------------------------------------------------------------- fields

namespace
{
    // Multiplicador en [0,1] que baja el terreno cerca del borde del mapa.
    // Usa distancia de Chebyshev al centro, de modo que la caida sigue la
    // forma cuadrada del mapa y no un circulo.
    float EdgeFalloff(float u, float v)
    {
        constexpr float Start = 0.82f;

        const float dx = std::fabs(u - 0.5f) * 2.0f;
        const float dy = std::fabs(v - 0.5f) * 2.0f;
        const float d = dx > dy ? dx : dy;

        if (d <= Start) return 1.0f;

        float t = (d - Start) / (1.0f - Start);
        if (t > 1.0f) t = 1.0f;

        // Smoothstep invertido: la transicion no tiene esquina visible.
        return 1.0f - (t * t * (3.0f - 2.0f * t));
    }

    inline float SmoothStep(float Edge0, float Edge1, float X)
    {
        float t = (X - Edge0) / (Edge1 - Edge0);
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        return t * t * (3.0f - 2.0f * t);
    }

    // Exponente de altura. Por encima de 1 aplana los valles y aisla las cimas.
    inline float ApplyExponent(float h, const GenerationSettings& S)
    {
        // Se calcula en double y se estrecha a float, igual que Math.Pow
        // en la version C#. Con dos floats, std::pow resuelve a la
        // sobrecarga float y redondea distinto: la diferencia es de 1 ULP,
        // invisible en el terreno, pero suficiente para que una muestra
        // justo en un borde de cuantizacion cambie el checksum.
        if (std::fabs(S.HeightExponent - 1.0f) > 0.001f)
            h = static_cast<float>(std::pow(static_cast<double>(h),
                                            static_cast<double>(S.HeightExponent)));
        return h;
    }

    // Exponente y caida al borde: el ultimo paso del camino de la Fase 0.
    inline float FinishHeight(float h, float u, float v, const GenerationSettings& S)
    {
        return ApplyExponent(h, S) * EdgeFalloff(u, v);
    }

    /**
     * Mesetas. Divide la altura en Steps escalones; dentro de cada uno, una
     * sigmoide deja una plataforma casi plana y un talud corto y abrupto.
     * Con Sharpness alto los taludes son paredes; con 1 no hay escalones.
     */
    inline float Terrace(float h, float Steps, float Sharpness)
    {
        const float t = h * Steps;
        const float i = std::floor(t);
        const float f = t - i;
        const float a = std::pow(f, Sharpness);
        const float b = std::pow(1.0f - f, Sharpness);
        return (i + a / (a + b)) / Steps;
    }

    void GenerateHeight(WorldData& World, const GenerationSettings& S, const SeedBook& Seeds)
    {
        const int32_t Side = World.Side;
        const uint32_t Seed = Seeds.For(SeedBook::Terrain);

        const bool bWarp = S.WarpStrength > 0.0001f;
        const bool bRidge = S.RidgeWeight > 0.0001f;
        const bool bTerrace = S.TerraceWeight > 0.0001f;

        // ------------------------------------------------ camino de la Fase 0
        // Sin relieve avanzado, exactamente las mismas operaciones que entonces:
        // el checksum 64C68E3D no cambia.
        if (!bWarp && !bRidge && !bTerrace)
        {
            for (int32_t y = 0; y < Side; ++y)
            {
                for (int32_t x = 0; x < Side; ++x)
                {
                    // Se muestrea en coordenadas normalizadas [0,1] y no en metros.
                    // Asi, cambiar WorldSize reescala el mundo en vez de mostrar otra
                    // region del ruido: la misma semilla da el mismo paisaje a
                    // cualquier tamano o resolucion.
                    const float u = static_cast<float>(x) / static_cast<float>(World.Resolution);
                    const float v = static_cast<float>(y) / static_cast<float>(World.Resolution);

                    const float h = noise::Fbm01(u, v, Seed, S.Octaves, S.Frequency, S.Lacunarity, S.Gain);
                    World.Height[World.Index(x, y)] = FinishHeight(h, u, v, S);
                }
            }
            return;
        }

        // ------------------------------------------------ relieve avanzado
        //
        // Dos escalas de relieve que se SUMAN:
        //   colinas    fBm normalizado, ocupa la parte baja del rango de altura
        //   cordilleras ruido de crestas, solo donde la mascara de montana lo
        //              permite, y se eleva hasta la altura maxima
        //
        // Ambos campos se normalizan a [0,1] con su minimo y maximo reales. El
        // fBm casi nunca llega a sus extremos teoricos, y sin normalizar el
        // terreno se quedaba en un 60-80% de MaxHeight. Asi MaxHeight es la
        // altura real de las cimas.
        const uint32_t SeedWarpX = Seeds.For(SeedBook::WarpX);
        const uint32_t SeedWarpY = Seeds.For(SeedBook::WarpY);
        const uint32_t SeedRidge = Seeds.For(SeedBook::Ridge);
        const uint32_t SeedMountain = Seeds.For(SeedBook::Mountain);
        const uint32_t SeedTerrace = Seeds.For(SeedBook::Terrace);

        const size_t N = World.Height.size();
        std::vector<float> Hills(N), Ridges(bRidge ? N : 0), Mountain(bRidge ? N : 0);
        float HMin = 1e30f, HMax = -1e30f, RMin = 1e30f, RMax = -1e30f;

        for (int32_t y = 0; y < Side; ++y)
        {
            for (int32_t x = 0; x < Side; ++x)
            {
                const int32_t i = World.Index(x, y);
                const float u = static_cast<float>(x) / static_cast<float>(World.Resolution);
                const float v = static_cast<float>(y) / static_cast<float>(World.Resolution);

                // Domain warping: el punto se desplaza segun otros dos ruidos
                // antes de muestrear. Donde el desplazamiento cambia rapido, las
                // formas se estiran y se curvan.
                float su = u, sv = v;
                if (bWarp)
                {
                    su += S.WarpStrength * noise::Fbm(u, v, SeedWarpX, 3, S.WarpFrequency, 2.0f, 0.5f);
                    sv += S.WarpStrength * noise::Fbm(u, v, SeedWarpY, 3, S.WarpFrequency, 2.0f, 0.5f);
                }

                const float h = noise::Fbm01(su, sv, Seed, S.Octaves, S.Frequency, S.Lacunarity, S.Gain);
                Hills[i] = h;
                if (h < HMin) HMin = h;
                if (h > HMax) HMax = h;

                if (bRidge)
                {
                    const float r = noise::RidgedFbm01(su, sv, SeedRidge, S.Octaves,
                                                       S.Frequency * 0.6f, S.Lacunarity, 0.5f);
                    Ridges[i] = r;
                    if (r < RMin) RMin = r;
                    if (r > RMax) RMax = r;

                    Mountain[i] = SmoothStep(0.40f, 0.70f,
                        noise::Fbm01(u, v, SeedMountain, 2, S.MountainFrequency, 2.0f, 0.5f));
                }
            }
        }

        const float HScale = HMax - HMin > 1e-6f ? 1.0f / (HMax - HMin) : 1.0f;
        const float RScale = RMax - RMin > 1e-6f ? 1.0f / (RMax - RMin) : 1.0f;

        // Fraccion de la altura que ocupan las colinas cuando hay cordilleras.
        // El resto del rango queda para las montanas: sobresalen claramente.
        const float HillsShare = bRidge ? 0.40f : 1.0f;

        for (int32_t y = 0; y < Side; ++y)
        {
            for (int32_t x = 0; x < Side; ++x)
            {
                const int32_t i = World.Index(x, y);
                const float u = static_cast<float>(x) / static_cast<float>(World.Resolution);
                const float v = static_cast<float>(y) / static_cast<float>(World.Resolution);

                // El exponente se aplica SOLO a las colinas, antes de sumar las
                // cordilleras: aplana el fondo de los valles sin aplastar las
                // montanas. Aplicado al final, aplastaba todo el terreno.
                const float hn = (Hills[i] - HMin) * HScale;
                float h = ApplyExponent(hn, S) * HillsShare;

                if (bRidge)
                {
                    // La cordillera se SUMA sobre las colinas. Un poco de colina
                    // dentro de la montana evita que las faldas sean lisas.
                    const float rn = (Ridges[i] - RMin) * RScale;
                    const float Peak = 0.75f * rn + 0.25f * hn;
                    h += Mountain[i] * S.RidgeWeight * Peak * (1.0f - HillsShare);
                }

                // Mesetas solo en algunas regiones, con su propia mascara.
                if (bTerrace)
                {
                    const float TMask = SmoothStep(0.45f, 0.65f,
                        noise::Fbm01(u, v, SeedTerrace, 2, S.TerraceFrequency, 2.0f, 0.5f));
                    const float Stepped = Terrace(h, S.TerraceSteps, 4.0f);
                    h = h + (Stepped - h) * (S.TerraceWeight * TMask);
                }

                h = h < 0.0f ? 0.0f : (h > 1.0f ? 1.0f : h);
                World.Height[i] = h * EdgeFalloff(u, v);
            }
        }
    }

    void GenerateMoisture(WorldData& World, const GenerationSettings& S, uint32_t Seed)
    {
        const int32_t Side = World.Side;

        for (int32_t y = 0; y < Side; ++y)
        {
            for (int32_t x = 0; x < Side; ++x)
            {
                const float u = static_cast<float>(x) / static_cast<float>(World.Resolution);
                const float v = static_cast<float>(y) / static_cast<float>(World.Resolution);

                float m = noise::Fbm01(u, v, Seed, S.MoistureOctaves, S.MoistureFrequency,
                                       S.MoistureLacunarity, S.MoistureGain);

                // Sesgo suave: lo bajo tiende a ser mas humedo que lo alto.
                // No es simulacion de nada, solo una correlacion que hace que
                // las zonas resultantes se lean como coherentes.
                const float h = World.Height[World.Index(x, y)];
                m = m * 0.75f + (1.0f - h) * 0.25f;

                // Junto al agua el suelo es mas humedo. Decae exponencialmente
                // con la distancia; asi la humedad de un lago cambia las zonas
                // y la vegetacion de su orilla.
                if (World.HasWater())
                {
                    const float d = World.WaterDistance[World.Index(x, y)];
                    m += S.WaterMoistureBoost * std::exp(-d / S.WaterMoistureRadius);
                }

                World.Moisture[World.Index(x, y)] = Clamp01(m);
            }
        }
    }

    // Pendiente por diferencias centrales sobre el campo de altura.
    //
    // dh/dx ~ (h[x+1] - h[x-1]) / (2 * CellSize), en metros por metro.
    // La magnitud del gradiente es la tangente del angulo de inclinacion,
    // asi que atan de esa magnitud da el angulo, y dividir entre PI/2 lo
    // normaliza a [0,1]: 0 plano, 1 vertical.
    void GenerateSlope(WorldData& World)
    {
        const int32_t Side = World.Side;
        const float Cell = World.CellSize();
        constexpr float HalfPi = 1.57079633f;

        for (int32_t y = 0; y < Side; ++y)
        {
            for (int32_t x = 0; x < Side; ++x)
            {
                const int32_t xm = x > 0 ? x - 1 : x;
                const int32_t xp = x < Side - 1 ? x + 1 : x;
                const int32_t ym = y > 0 ? y - 1 : y;
                const int32_t yp = y < Side - 1 ? y + 1 : y;

                const float dx = (World.HeightMeters(xp, y) - World.HeightMeters(xm, y))
                               / (static_cast<float>(xp - xm) * Cell);
                const float dy = (World.HeightMeters(x, yp) - World.HeightMeters(x, ym))
                               / (static_cast<float>(yp - ym) * Cell);

                // Tambien en double, por la misma razon que el pow de la altura.
                const double Magnitude = std::sqrt(static_cast<double>(dx) * dx
                                                 + static_cast<double>(dy) * dy);
                const float Slope = static_cast<float>(std::atan(Magnitude)) / HalfPi;

                World.Slope[World.Index(x, y)] = Slope > 1.0f ? 1.0f : Slope;
            }
        }
    }
}

void FieldGenerator::Generate(WorldData& World, const GenerationSettings& S, const SeedBook& Seeds)
{
    GenerateHeight(World, S, Seeds);
    // El agua va entre la altura y la humedad: puede excavar el terreno (Gran
    // lago) y cambia la humedad de su alrededor.
    WaterGenerator::Generate(World, S, Seeds);
    // El camino necesita saber donde esta el agua para esquivarla, y aplana el
    // terreno: la humedad y la pendiente deben ver ese terreno ya aplanado.
    PathGenerator::Generate(World, S, Seeds);
    GenerateMoisture(World, S, Seeds.For(SeedBook::Moisture));
    GenerateSlope(World);
}

// ---------------------------------------------------------------- generator

namespace
{
    double GLastGenerationMs = 0.0;
}

double WorldGenerator::GetLastGenerationMs() { return GLastGenerationMs; }

// ---------------------------------------------------------------- relief styles

const char* ReliefStyleName(int32_t StyleId)
{
    switch (static_cast<ReliefStyle>(StyleId))
    {
    case ReliefStyle::Llanura: return "Llanura";
    case ReliefStyle::Valles:  return "Valles";
    case ReliefStyle::Sierras: return "Sierras";
    case ReliefStyle::Mesetas: return "Mesetas";
    case ReliefStyle::Lomas:   return "Lomas";
    case ReliefStyle::Picos:   return "Picos";
    default:                   return "Base";
    }
}

namespace
{
    // Cada caracter es una fila de multiplicadores sobre la configuracion base.
    // Igual que las zonas (costura 3): agregar un caracter es agregar una fila.
    // Las sumas (Ridge/Terrace) van aparte porque con base 0 un multiplicador
    // no haria nada.
    struct StyleRow
    {
        float Weight;       // probabilidad relativa
        float Height;       // x MaxHeight
        float Frequency;    // x Frequency (mas = colinas mas estrechas)
        float Warp;         // x WarpStrength
        float Ridge;        // x RidgeWeight
        float Mountain;     // x MountainFrequency (mas = mas cordilleras)
        float Terrace;      // x TerraceWeight
        float TerraceAdd;   // + TerraceWeight
        float Exponent;     // x HeightExponent (mas = valles mas planos)
    };

    //                              peso  alto  frec  warp  cresta mont  meseta +mes  expo
    const StyleRow GStyles[] = {
        /* Llanura */             { 0.18f, 0.55f, 0.70f, 1.2f, 0.35f, 0.8f, 0.3f, 0.0f, 1.25f },
        /* Valles  */             { 0.22f, 1.00f, 1.00f, 1.0f, 1.00f, 1.0f, 1.0f, 0.0f, 1.00f },
        /* Sierras */             { 0.18f, 1.25f, 0.90f, 1.0f, 1.15f, 1.6f, 0.3f, 0.0f, 1.10f },
        /* Mesetas */             { 0.14f, 1.05f, 0.85f, 0.8f, 0.50f, 0.9f, 1.0f, 0.3f, 0.90f },
        /* Lomas   */             { 0.16f, 0.80f, 1.45f, 2.8f, 0.60f, 1.2f, 0.4f, 0.0f, 0.90f },
        /* Picos   */             { 0.12f, 1.40f, 1.20f, 1.0f, 1.15f, 2.4f, 0.2f, 0.0f, 1.20f },
    };
    static_assert(sizeof(GStyles) / sizeof(GStyles[0]) == static_cast<size_t>(ReliefStyle::Count),
                  "una fila por caracter");
}

int32_t ReliefStyles::Apply(GenerationSettings& S, const SeedBook& Seeds)
{
    const float V = S.ReliefVariation < 0.0f ? 0.0f : (S.ReliefVariation > 1.0f ? 1.0f : S.ReliefVariation);
    if (V <= 0.0f) return -1;

    constexpr int32_t N = static_cast<int32_t>(ReliefStyle::Count);
    float Weights[N];
    for (int32_t i = 0; i < N; ++i) Weights[i] = GStyles[i].Weight;

    // Orden de consumo del generador fijo: primero el caracter, luego un
    // numero por parametro. Cambiarlo cambia todos los relieves.
    DeterministicRandom Rng(Seeds.For(SeedBook::Relief));
    const int32_t Id = PickWeighted(Rng, Weights, N);
    const StyleRow& R = GStyles[Id];

    // Con V < 1 el multiplicador se acerca a 1 (la base).
    auto Mix = [V](float M) { return 1.0f + (M - 1.0f) * V; };
    auto Jitter = [&Rng, V]() { return 1.0f + Rng.Range(-0.12f, 0.12f) * V; };

    S.MaxHeight         *= Mix(R.Height)    * Jitter();
    S.Frequency         *= Mix(R.Frequency) * Jitter();
    S.WarpStrength      *= Mix(R.Warp)      * Jitter();
    S.RidgeWeight       *= Mix(R.Ridge)     * Jitter();
    S.MountainFrequency *= Mix(R.Mountain)  * Jitter();
    S.TerraceWeight      = S.TerraceWeight * Mix(R.Terrace) + R.TerraceAdd * V;
    S.HeightExponent    *= Mix(R.Exponent)  * Jitter();
    return Id;
}

WorldData WorldGenerator::Generate(GenerationSettings Settings, const ZoneSet& Zones)
{
    Settings.Validate();

    const auto Start = std::chrono::high_resolution_clock::now();

    const SeedBook Seeds(Settings.Seed);

    // Antes de crear el mundo: el caracter puede cambiar MaxHeight.
    const int32_t Style = ReliefStyles::Apply(Settings, Seeds);
    Settings.Validate();

    WorldData World(Settings.Resolution, Settings.WorldSize, Settings.MaxHeight, Seeds.GetMaster());
    World.ReliefStyleId = Style;

    FieldGenerator::Generate(World, Settings, Seeds);
    ZoneClassifier::Classify(World, Zones);

    const auto End = std::chrono::high_resolution_clock::now();
    GLastGenerationMs = std::chrono::duration<double, std::milli>(End - Start).count();

    return World;
}

} // namespace jardin
