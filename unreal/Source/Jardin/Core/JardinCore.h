// JardinCore.h — Hash, generador determinista, derivacion de semillas y ruido.
//
// C++ PLANO. Cero tipos de Unreal, cero includes del motor.
//
// Es deliberado: este archivo y los de generacion son el aporte academico del
// proyecto. Al no depender del motor se pueden compilar y probar con cl.exe
// fuera de Unreal, que es como se verifican antes de integrarlos.
//
// Unidades: TODO el nucleo trabaja en METROS. La conversion a centimetros
// (unidades de Unreal) ocurre solo en la capa del motor. Mezclar eso es la
// causa numero uno de mundos con escala absurda.

#pragma once

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <string>

namespace jardin
{

// ---------------------------------------------------------------- hash

// Funciones hash deterministas de 32 bits.
//
// No se usa std::hash<std::string>: la norma no garantiza que sea estable
// entre ejecuciones ni entre implementaciones. Se implementa FNV-1a a mano.
namespace hash32
{
    // Mezcla de avalancha: cambiar un bit de la entrada cambia ~la mitad de
    // los bits de la salida. El desbordamiento de unsigned esta definido
    // por la norma de C++, asi que esto es portable.
    inline uint32_t Mix(uint32_t x)
    {
        x ^= 2747636419u;
        x *= 2654435769u;
        x ^= x >> 16;
        x *= 2654435769u;
        x ^= x >> 16;
        x *= 2654435769u;
        return x;
    }

    inline uint32_t Combine(uint32_t a, uint32_t b)
    {
        return Mix(a ^ (b + 0x9E3779B9u + (a << 6) + (a >> 2)));
    }

    // Hash de coordenada entera 2D con semilla. Base del ruido.
    inline uint32_t Coord(int32_t x, int32_t y, uint32_t seed)
    {
        uint32_t h = static_cast<uint32_t>(x) * 0x27D4EB2Du;
        h ^= static_cast<uint32_t>(y) * 0x165667B1u;
        h ^= seed;
        return Mix(h);
    }

    // FNV-1a de 32 bits. Estable entre ejecuciones y plataformas.
    inline uint32_t Text(const char* s)
    {
        uint32_t h = 2166136261u;
        while (*s)
        {
            h ^= static_cast<uint32_t>(static_cast<unsigned char>(*s++));
            h *= 16777619u;
        }
        return h;
    }

    inline float ToUnit(uint32_t h)   { return (h >> 8) * (1.0f / 16777216.0f); } // [0,1)
    inline float ToSigned(uint32_t h) { return ToUnit(h) * 2.0f - 1.0f; }         // [-1,1)
}

// ---------------------------------------------------------------- rng

// Generador pseudoaleatorio determinista (xorshift32).
//
// Sustituye por completo a FMath::Rand y FRandomStream dentro de la generacion.
// FMath::Rand comparte estado global: cualquier otro sistema del motor que lo
// use desplazaria la secuencia y la misma semilla dejaria de dar el mismo mundo.
class DeterministicRandom
{
public:
    explicit DeterministicRandom(uint32_t seed)
        // El estado 0 es punto fijo de xorshift: se quedaria atascado en 0.
        : State(seed == 0u ? 0x9E3779B9u : seed) {}

    uint32_t NextUInt()
    {
        uint32_t x = State;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        State = x;
        return x;
    }

    float NextFloat() { return (NextUInt() >> 8) * (1.0f / 16777216.0f); }

    float Range(float Min, float Max) { return Min + (Max - Min) * NextFloat(); }

    int32_t Range(int32_t MinInclusive, int32_t MaxExclusive)
    {
        const int32_t Span = MaxExclusive - MinInclusive;
        if (Span <= 0) return MinInclusive;
        return MinInclusive + static_cast<int32_t>(NextUInt() % static_cast<uint32_t>(Span));
    }

    bool Chance(float P) { return NextFloat() < P; }

    uint32_t GetState() const { return State; }

private:
    uint32_t State;
};

// Sorteo con pesos: devuelve un indice en [0, Count) con probabilidad
// proporcional a su peso. Los pesos negativos cuentan como 0; si todos son 0,
// el sorteo es uniforme. Consume un solo numero del generador.
inline int32_t PickWeighted(DeterministicRandom& Rng, const float* Weights, int32_t Count)
{
    if (Count <= 0) return -1;
    float Total = 0.0f;
    for (int32_t i = 0; i < Count; ++i) Total += Weights[i] > 0.0f ? Weights[i] : 0.0f;
    const float R = Rng.NextFloat();
    if (Total <= 0.0f) return std::min(static_cast<int32_t>(R * Count), Count - 1);

    float Acc = 0.0f;
    for (int32_t i = 0; i < Count; ++i)
    {
        Acc += (Weights[i] > 0.0f ? Weights[i] : 0.0f) / Total;
        if (R < Acc) return i;
    }
    // Redondeo: R muy cerca de 1. El ultimo con peso.
    for (int32_t i = Count - 1; i >= 0; --i) if (Weights[i] > 0.0f) return i;
    return Count - 1;
}

// ---------------------------------------------------------------- seeds

// COSTURA 1 — Semillas derivadas.
//
// De una semilla maestra se derivan subsemillas independientes, una por
// subsistema, mediante el hash de su nombre.
//
// Consecuencia practica: agregar el agua (Fase 3) o los caminos (Fase 4) no
// altera el terreno ni los arboles de los mundos ya generados. Con un solo
// generador compartido, cualquier sistema nuevo desplazaria a los demas y
// la semilla 827461 dejaria de significar lo mismo.
class SeedBook
{
public:
    explicit SeedBook(uint32_t InMaster) : Master(InMaster) {}
    explicit SeedBook(int32_t InMaster) : Master(static_cast<uint32_t>(InMaster)) {}

    uint32_t GetMaster() const { return Master; }

    uint32_t For(const char* Subsystem) const
    {
        return hash32::Combine(Master, hash32::Text(Subsystem));
    }

    DeterministicRandom Rng(const char* Subsystem) const
    {
        return DeterministicRandom(For(Subsystem));
    }

    // Nombres en un solo sitio: una errata en la cadena no da error de
    // compilacion, da otro mundo.
    static constexpr const char* Terrain   = "terrain";
    static constexpr const char* Moisture  = "moisture";
    static constexpr const char* Scatter   = "scatter";
    static constexpr const char* Variation = "variation";
    static constexpr const char* Water     = "water";  // reservado, Fase 3
    static constexpr const char* Path      = "path";   // reservado, Fase 4

    // Relieve (agregados despues de la Fase 0). Al ser subsistemas nuevos con
    // nombre propio, no desplazan a ninguno de los anteriores.
    static constexpr const char* WarpX     = "warp_x";
    static constexpr const char* WarpY     = "warp_y";
    static constexpr const char* Ridge     = "ridge";
    static constexpr const char* Mountain  = "mountain";
    static constexpr const char* Terrace   = "terrace";
    static constexpr const char* Fractal   = "fractal";   // un arbol fractal distinto por objeto
    static constexpr const char* Relief    = "relief";    // caracter del relieve (llanura, sierras...)
    static constexpr const char* Palette   = "palette";   // bioma de color del jardin
    static constexpr const char* Grass     = "grass";     // pasto denso por trozos

private:
    uint32_t Master;
};

// ---------------------------------------------------------------- noise

// Ruido procedural propio.
//
// Se ofrecen dos ruidos base:
//   Value  - el mas simple de explicar. Interpola valores aleatorios de rejilla.
//   Perlin - ruido de gradiente. Visualmente mucho mejor para terreno porque
//            no deja el artefacto de bloques del value noise.
// Para el terreno se usa Perlin.
namespace noise
{
    // Quintic fade de Perlin: 6t^5 - 15t^4 + 10t^3.
    // Primera y segunda derivada nulas en 0 y 1, lo que elimina las
    // discontinuidades visibles en las fronteras de la rejilla.
    inline float Fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }

    inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }

    inline int32_t FloorToInt(float v)
    {
        const int32_t i = static_cast<int32_t>(v);
        return v < static_cast<float>(i) ? i - 1 : i;
    }

    float Value(float x, float y, uint32_t seed);
    float Perlin(float x, float y, uint32_t seed);

    // Movimiento browniano fraccional: suma de octavas, cada una con el doble
    // de frecuencia (lacunarity) y la mitad de amplitud (gain) que la anterior.
    //
    // Es lo que convierte un ruido suave y aburrido en relieve con detalle a
    // varias escalas: colinas grandes, ondulaciones medias y rugosidad fina.
    //
    // Cada octava usa su propia subsemilla para que no se correlacionen.
    // El resultado se divide entre la suma de amplitudes para conservar [-1,1]
    // sin importar cuantas octavas se usen.
    float Fbm(float x, float y, uint32_t seed, int32_t octaves,
              float frequency, float lacunarity, float gain, bool usePerlin = true);

    // fBm remapeado a [0,1]. Es la forma que usan los campos del mundo.
    float Fbm01(float x, float y, uint32_t seed, int32_t octaves,
                float frequency, float lacunarity, float gain, bool usePerlin = true);

    // Multifractal con crestas (Musgrave), en [0,1].
    //
    // Cada octava usa 1 - |ruido|: donde el ruido cruza cero aparece un pico
    // agudo, y la union de esos picos forma lineas de cresta, como sierras.
    // Elevarlo al cuadrado afila las crestas y ensancha los valles.
    //
    // Ademas, cada octava se pondera por la anterior: el detalle fino solo
    // aparece sobre las crestas, no en los valles. Es lo que hace que las
    // montanas se vean erosionadas y no simplemente ruidosas.
    float RidgedFbm01(float x, float y, uint32_t seed, int32_t octaves,
                      float frequency, float lacunarity, float gain);
}

} // namespace jardin
