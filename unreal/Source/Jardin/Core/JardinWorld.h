// JardinWorld.h — Parametros, campos del mundo y clasificacion de zonas.
//
// C++ plano. Sin tipos de Unreal. Unidades en METROS.

#pragma once

#include "JardinCore.h"
#include <vector>

namespace jardin
{

// ---------------------------------------------------------------- settings

// Todos los parametros de generacion en un solo objeto.
//
// Regla: si un valor influye en el mundo, vive aqui. Nada de constantes
// escondidas dentro de los generadores, porque entonces el conjunto
// "semilla + settings" dejaria de describir el mundo por completo.
struct GenerationSettings
{
    int32_t Seed = 827461;

    // Lado del mapa EN METROS. La capa de Unreal multiplica por 100.
    float WorldSize = 128.0f;

    // Celdas por lado. Las muestras por lado son Resolution + 1.
    int32_t Resolution = 192;

    // --- campo de altura ---
    float MaxHeight = 14.0f;      // metros
    float Frequency = 1.6f;       // ciclos de la octava base a lo ancho del mundo
    int32_t Octaves = 5;
    float Lacunarity = 2.0f;
    float Gain = 0.5f;

    // Exponente sobre la altura normalizada. Por encima de 1 aplana los valles
    // y aisla las cimas, que es lo que da aspecto de valle o parque.
    float HeightExponent = 1.35f;

    // --- relieve avanzado (0 = desactivado; con todo en 0 el terreno es
    //     identico, bit a bit, al de la Fase 0) ---

    // Domain warping: desplaza las coordenadas con otro ruido antes de
    // muestrear la altura. Las colinas dejan de ser manchas redondas y se
    // curvan y alargan como formas organicas. En fraccion del mapa.
    float WarpStrength = 0.0f;
    float WarpFrequency = 3.0f;

    // Cuanto pesan las crestas en las regiones montanosas, en [0,1].
    float RidgeWeight = 0.0f;

    // Frecuencia del mapa que decide donde hay montanas. Baja = pocas
    // cordilleras grandes; el resto del mapa sigue siendo llanura o colina.
    float MountainFrequency = 2.0f;

    // Mesetas: escalones de terreno con plataformas planas y taludes
    // abruptos, solo en algunas regiones. 0 = desactivado.
    float TerraceWeight = 0.0f;
    float TerraceSteps = 6.0f;
    float TerraceFrequency = 2.5f;

    // Caracter del relieve por semilla. 0 = apagado: todas las semillas usan
    // exactamente los parametros de arriba. 1 = cada semilla sortea un caracter
    // (Llanura, Valles, Sierras, Mesetas, Lomas, Picos) que reescala esos
    // parametros. Valores intermedios lo acercan mas o menos a la base.
    float ReliefVariation = 0.0f;

    // --- agua (0 = sin agua: el mundo es identico al de antes) ---

    // 0 = apagado; 1 = automatico (la semilla sortea el regimen hidrico);
    // 2..7 = regimen fijo, en el orden de WaterProfile (Seco ... Isla).
    int32_t WaterMode = 0;

    // Lagos con menos superficie que esta se descartan, en m2.
    float WaterMinAreaM2 = 400.0f;

    // Humedad extra junto al agua, y a que distancia se desvanece (metros).
    // Es la cascada ecologica: cerca del agua crecen las especies humedas.
    float WaterMoistureBoost = 0.25f;
    float WaterMoistureRadius = 25.0f;

    // --- camino (0 = sin camino: el mundo es identico al de antes) ---

    // 0 = apagado; 1 = un camino que cruza el mapa (Dijkstra + Catmull-Rom).
    int32_t PathMode = 0;
    float PathWidth = 3.0f;            // metros, ancho de la cinta
    float PathShoulder = 2.5f;         // metros a cada lado donde el aplanado se desvanece
    float PathControlSpacing = 40.0f;  // metros entre puntos de control de la curva

    // --- campo de humedad ---
    float MoistureFrequency = 1.1f;
    int32_t MoistureOctaves = 3;
    float MoistureLacunarity = 2.0f;
    float MoistureGain = 0.5f;

    // Acota valores a rangos donde el generador se comporta. Se llama siempre
    // antes de generar: evita que un valor tecleado por error produzca una
    // malla de millones de vertices o una division entre cero.
    void Validate();
};

// ---------------------------------------------------------------- path data

// Una muestra de la curva del camino, cada ~1 m de longitud de arco.
struct PathSample
{
    float X = 0.0f, Z = 0.0f;     // metros, coordenadas de mundo
    float H = 0.0f;               // altura de la calzada, metros
    float TX = 1.0f, TZ = 0.0f;   // tangente unitaria
    float S = 0.0f;               // longitud de arco desde el inicio, metros
};

struct PathData
{
    std::vector<float> ControlX, ControlZ;   // puntos de control de Catmull-Rom
    std::vector<PathSample> Samples;
    float Width = 0.0f;
    float Length = 0.0f;
    bool Empty() const { return Samples.size() < 2; }
};

// ---------------------------------------------------------------- world data

// COSTURA 2 — Los campos son datos, no calculos incrustados.
//
// La generacion LLENA estos arreglos. El renderizado, la colocacion y el modo
// de depuracion solo los LEEN. Nadie recalcula ruido por su cuenta.
//
// Agregar un campo nuevo en una fase posterior (DistanceToWater en la Fase 3,
// PathMask en la Fase 4) es agregar un vector aqui y llenarlo. No se toca
// nada de lo existente.
//
// Sistema de coordenadas: mundo centrado en el origen, X y Z de -WorldSize/2
// a +WorldSize/2, altura en metros.
//
// OJO: aqui "Z" es la segunda coordenada horizontal, como en el codigo original.
// Unreal es Z-up, asi que la capa del motor mapea (X, Z, Altura) -> (X, Y, Z).
class WorldData
{
public:
    WorldData(int32_t InResolution, float InWorldSize, float InMaxHeight, uint32_t InMasterSeed);

    int32_t Resolution = 0;   // celdas por lado
    int32_t Side = 0;         // muestras por lado = Resolution + 1
    float WorldSize = 0.0f;   // metros
    float MaxHeight = 0.0f;   // metros
    uint32_t MasterSeed = 0;

    std::vector<float> Height;      // [0,1] normalizado
    std::vector<float> Moisture;    // [0,1]
    std::vector<float> Slope;       // [0,1]; 0 plano, 1 vertical
    std::vector<uint8_t> Zone;

    // Margen entre el primer y el segundo puntaje de zona, en [0,1].
    // En la Fase 0 no se usa. Existe desde ahora porque es exactamente lo que
    // necesita la Fase 6 para fronteras difusas en vez de cortes duros.
    std::vector<float> ZoneBlend;

    // --- agua (vacios si el agua esta apagada) ---

    /** Altura normalizada de la superficie del agua en cada celda; -1 = seco. */
    std::vector<float> WaterLevel;

    /** Distancia en metros al agua mas cercana (0 dentro del agua). */
    std::vector<float> WaterDistance;

    /** Regimen hidrico que salio (valor de WaterProfile). */
    uint8_t WaterProfileId = 0;

    /** Caracter del relieve que salio (valor de ReliefStyle); -1 = sin variacion. */
    int32_t ReliefStyleId = -1;

    // --- camino (vacios si el camino esta apagado) ---

    /** La curva del camino (puntos de control y muestras). */
    PathData Path;

    /** Distancia en metros al eje del camino. */
    std::vector<float> PathDistance;

    bool HasWater() const { return !WaterLevel.empty(); }
    bool HasPath() const { return !Path.Empty() && !PathDistance.empty(); }

    float CellSize() const { return WorldSize / static_cast<float>(Resolution); }

    int32_t Index(int32_t x, int32_t y) const { return y * Side + x; }

    float HeightMeters(int32_t x, int32_t y) const { return Height[Index(x, y)] * MaxHeight; }

    float GridToWorldX(int32_t x) const { return x * CellSize() - WorldSize * 0.5f; }
    float GridToWorldZ(int32_t y) const { return y * CellSize() - WorldSize * 0.5f; }

    // Interpolacion bilineal de cualquier campo en coordenadas de mundo.
    // Necesaria para consultar altura o pendiente en posiciones que no caen
    // sobre un vertice: es lo que usan la colocacion de objetos y el jugador.
    float SampleField(const std::vector<float>& Field, float WorldX, float WorldZ) const;

    float SampleHeightMeters(float WorldX, float WorldZ) const
    {
        return SampleField(Height, WorldX, WorldZ) * MaxHeight;
    }
    float SampleSlope(float WorldX, float WorldZ) const    { return SampleField(Slope, WorldX, WorldZ); }
    float SampleMoisture(float WorldX, float WorldZ) const { return SampleField(Moisture, WorldX, WorldZ); }

    // Zona en una posicion de mundo. Vecino mas cercano, no interpolada.
    uint8_t SampleZone(float WorldX, float WorldZ) const;

    bool InsideBounds(float WorldX, float WorldZ, float Margin = 0.0f) const
    {
        const float Half = WorldSize * 0.5f - Margin;
        return WorldX >= -Half && WorldX <= Half && WorldZ >= -Half && WorldZ <= Half;
    }

    // Huella digital del mundo generado.
    //
    // REGLA DE CONTROL: despues de cada fase, generar la misma semilla dos veces
    // y comparar. Si cambio, algo introdujo aleatoriedad no determinista y la
    // fase no esta terminada.
    uint32_t Checksum() const;
};

// ---------------------------------------------------------------- zones

// COSTURA 3 — Clasificacion por puntaje, no por if.
//
// La tentacion con solo dos zonas es escribir:
//     if (altura > 0.5f) zona = Bosque; else zona = Pradera;
// Eso funciona hoy y es una trampa: agregar una tercera zona con fronteras
// suaves obligaria a reescribir el clasificador y toda la colocacion.
//
// Aqui cada zona calcula un puntaje y gana el mas alto. Agregar la zona
// rocosa-humeda de la Fase 6 es agregar una entrada al vector.
struct ZoneRule
{
    const char* Name = "Zona";
    uint8_t Id = 0;

    float Bias = 0.0f;

    // Cada campo aporta mediante una "banda de preferencia": la zona puntua
    // alto cerca de su centro y cae linealmente hasta cero a una distancia
    // igual a su ancho.
    //
    // Se usa esto y no umbrales porque una banda expresa "a esta zona le gusta
    // la humedad media-alta" en lugar de "humedad mayor que 0.6", que es lo
    // que produce fronteras rectas y artificiales.
    float HeightCenter = 0.5f,   HeightWidth = 1.0f,   HeightWeight = 1.0f;
    float MoistureCenter = 0.5f, MoistureWidth = 1.0f, MoistureWeight = 1.0f;
    float SlopeCenter = 0.0f,    SlopeWidth = 1.0f,    SlopeWeight = 1.0f;

    // Funcion tienda: 1 en el centro, 0 a una distancia igual al ancho.
    static float Band(float Value, float Center, float Width)
    {
        if (Width <= 0.0001f) return 0.0f;
        const float d = std::fabs(Value - Center) / Width;
        const float r = 1.0f - d;
        return r < 0.0f ? 0.0f : (r > 1.0f ? 1.0f : r);
    }

    float Score(float H, float M, float S) const
    {
        return Bias
             + HeightWeight   * Band(H, HeightCenter,   HeightWidth)
             + MoistureWeight * Band(M, MoistureCenter, MoistureWidth)
             + SlopeWeight    * Band(S, SlopeCenter,    SlopeWidth);
    }
};

struct ZoneSet
{
    enum : uint8_t { Pradera = 0, Bosque = 1 /*, Rocosa = 2  // Fase 6 */ };

    std::vector<ZoneRule> Rules;

    // Margen de puntaje a partir del cual una zona se considera "decidida".
    // Margenes menores se guardan como valores intermedios en ZoneBlend.
    float BlendScale = 0.25f;

    static ZoneSet Default();

    const char* NameOf(uint8_t Id) const;
};

// ---------------------------------------------------------------- pipeline

namespace FieldGenerator
{
    // Orden obligatorio: la pendiente se deriva de la altura, asi que la altura
    // tiene que estar completa antes de calcularla.
    void Generate(WorldData& World, const GenerationSettings& S, const SeedBook& Seeds);
}

namespace ZoneClassifier
{
    // Llena Zone y ZoneBlend. Requiere Height, Moisture y Slope ya generados.
    void Classify(WorldData& World, const ZoneSet& Zones);
}

/** Caracter del relieve. La semilla lo sortea si ReliefVariation > 0. */
enum class ReliefStyle : int32_t
{
    Llanura = 0,   // lomas bajas y anchas
    Valles,        // la configuracion base, sin reescalar
    Sierras,       // cordilleras altas y frecuentes
    Mesetas,       // plataformas escalonadas
    Lomas,         // colinas retorcidas y estrechas
    Picos,         // montanas altas y afiladas
    Count
};

const char* ReliefStyleName(int32_t StyleId);

namespace ReliefStyles
{
    /**
     * Sortea el caracter del relieve con la subsemilla "relief" y reescala los
     * parametros de relieve de S. Devuelve el caracter, o -1 si
     * S.ReliefVariation es 0 (en ese caso S no cambia: el mundo es identico).
     *
     * Ademas del caracter, cada parametro recibe un +-12% propio: dos semillas
     * con el mismo caracter tampoco tienen el mismo relieve.
     */
    int32_t Apply(GenerationSettings& S, const SeedBook& Seeds);
}

namespace WorldGenerator
{
    // Unico punto de entrada. Orden fijo, cada paso depende del anterior:
    //   semillas -> altura -> humedad -> pendiente -> zonas
    //
    // Fases posteriores insertan pasos sin tocar los existentes:
    //   Fase 3: excavar cuenca y DistanceToWater, antes de las zonas
    //   Fase 4: trazar la spline y PathMask, despues de las zonas
    WorldData Generate(GenerationSettings Settings, const ZoneSet& Zones);

    // Milisegundos de la ultima generacion. Solo informativo.
    double GetLastGenerationMs();
}

} // namespace jardin
