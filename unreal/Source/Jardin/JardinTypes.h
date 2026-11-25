// JardinTypes.h — Puente entre el nucleo C++ plano y el sistema de reflexion
// de Unreal.
//
// El nucleo (Core/) no conoce Unreal: no puede llevar USTRUCT ni UPROPERTY.
// Estos espejos existen solo para que los parametros se puedan editar en el
// inspector y en Blueprints. Cada uno sabe convertirse a su tipo del nucleo
// con ToCore().
//
// UNIDADES: el nucleo trabaja en METROS. Unreal trabaja en CENTIMETROS.
// La conversion ocurre aqui y solo aqui. Mezclar eso es la causa numero uno
// de mundos con escala absurda.

#pragma once

#include "CoreMinimal.h"
#include "Core/JardinScatter.h"
#include "Core/JardinFractalTree.h"
#include "JardinTypes.generated.h"

class UStaticMesh;
class UMaterialInterface;

namespace JardinUnits
{
	/** Unreal usa centimetros. 1 metro del nucleo = 100 unidades de Unreal. */
	constexpr float MetersToUU = 100.0f;
	constexpr float UUToMeters = 0.01f;
}

/** Vistas del modo de depuracion. */
UENUM(BlueprintType)
enum class EJardinDebugView : uint8
{
	Normal      UMETA(DisplayName = "Normal"),
	Altura      UMETA(DisplayName = "Mapa de altura"),
	Humedad     UMETA(DisplayName = "Mapa de humedad"),
	Pendiente   UMETA(DisplayName = "Mapa de pendiente"),
	Zonas       UMETA(DisplayName = "Mapa de zonas"),
	Mezcla      UMETA(DisplayName = "Fronteras de zona"),
	Agua        UMETA(DisplayName = "Distancia al agua"),
	Camino      UMETA(DisplayName = "Camino")
};

/** Regimen hidrico. Mismo orden que jardin::WaterProfile (+1). */
UENUM(BlueprintType)
enum class EJardinWaterMode : uint8
{
	Apagado       = 0 UMETA(DisplayName = "Sin agua"),
	Automatico    = 1 UMETA(DisplayName = "Automatico (lo sortea la semilla)"),
	Seco          = 2 UMETA(DisplayName = "Seco: unas lagunas"),
	Estanques     = 3 UMETA(DisplayName = "Estanques"),
	Lagos         = 4 UMETA(DisplayName = "Lagos"),
	GranLago      = 5 UMETA(DisplayName = "Gran lago (mar interior)"),
	Archipielago  = 6 UMETA(DisplayName = "Archipielago"),
	Isla          = 7 UMETA(DisplayName = "Isla")
};

/**
 * Parametros de generacion, expuestos al editor.
 * Espejo de jardin::GenerationSettings.
 */
USTRUCT(BlueprintType)
struct JARDIN_API FJardinGenerationSettings
{
	GENERATED_BODY()

	/** La misma semilla produce siempre el mismo jardin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Semilla")
	int32 Seed = 827461;

	/** Lado del mapa EN METROS. El protocolo compromete ~128. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dimensiones",
		meta = (ClampMin = "16.0", ClampMax = "1024.0"))
	float WorldSizeMeters = 128.0f;

	/**
	 * Celdas por lado. Las muestras por lado son Resolution + 1.
	 * Baja a 96 para iterar rapido; sube a 192 para las capturas.
	 * El paisaje NO cambia al cambiar la resolucion.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dimensiones",
		meta = (ClampMin = "8", ClampMax = "512"))
	int32 Resolution = 192;

	/** Altura maxima en metros. Ojo: el relieve real llega a ~67% de este valor
	 *  por efecto de HeightExponent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Altura",
		meta = (ClampMin = "0.0"))
	float MaxHeightMeters = 14.0f;

	/** Ciclos de la octava base a lo ancho del mundo. Bajo da colinas amplias. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Altura",
		meta = (ClampMin = "0.01"))
	float Frequency = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Altura",
		meta = (ClampMin = "1", ClampMax = "12"))
	int32 Octaves = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Altura",
		meta = (ClampMin = "1.01"))
	float Lacunarity = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Altura",
		meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float Gain = 0.5f;

	/** Por encima de 1 aplana los valles y aisla las cimas. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Altura",
		meta = (ClampMin = "0.1"))
	float HeightExponent = 1.35f;

	/** Domain warping: deforma las colinas para que se curven y alarguen en
	 *  vez de ser manchas redondas. 0 = apagado. 0.05 es un buen punto de partida. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Relieve",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WarpStrength = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Relieve",
		meta = (ClampMin = "0.01"))
	float WarpFrequency = 3.0f;

	/** Peso de las crestas (ruido multifractal) en las regiones montanosas.
	 *  0 = apagado, 1 = sierras completas. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Relieve",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RidgeWeight = 0.0f;

	/** Donde hay montanas. Bajo = pocas cordilleras grandes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Relieve",
		meta = (ClampMin = "0.01"))
	float MountainFrequency = 2.0f;

	/** Mesetas: plataformas elevadas con bordes abruptos, en algunas regiones.
	 *  0 = apagado. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Relieve",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TerraceWeight = 0.0f;

	/** Cuantos escalones caben en toda la altura del mapa. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Relieve",
		meta = (ClampMin = "1.0", ClampMax = "32.0"))
	float TerraceSteps = 6.0f;

	/** Donde hay mesetas. Bajo = pocas regiones grandes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Relieve",
		meta = (ClampMin = "0.01"))
	float TerraceFrequency = 2.5f;

	/** Caracter del relieve por semilla: 0 = todas las semillas usan los
	 *  parametros de arriba; 1 = cada semilla sortea Llanura, Valles, Sierras,
	 *  Mesetas, Lomas o Picos, que reescalan esos parametros. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Relieve",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ReliefVariation = 0.0f;

	/** Camino por spline (Fase 4): Dijkstra elige la ruta, Catmull-Rom la
	 *  suaviza y la calzada se aplana en el terreno. Apagado = mundo identico. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camino")
	bool bPathEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camino",
		meta = (ClampMin = "0.5", ClampMax = "20.0", EditCondition = "bPathEnabled"))
	float PathWidthMeters = 3.0f;

	/** Metros a cada lado donde el aplanado se funde con el terreno. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camino",
		meta = (ClampMin = "0.0", EditCondition = "bPathEnabled"))
	float PathShoulderMeters = 2.5f;

	/** Separacion inicial entre puntos de control de la curva. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camino",
		meta = (ClampMin = "5.0", EditCondition = "bPathEnabled"))
	float PathControlSpacingMeters = 40.0f;

	/** Agua por Priority-Flood: los lagos aparecen donde el relieve los
	 *  permite. En automatico la semilla sortea el regimen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Agua")
	EJardinWaterMode WaterMode = EJardinWaterMode::Apagado;

	/** Lagos mas pequenos que esto se descartan, en m2. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Agua",
		meta = (ClampMin = "0.0"))
	float WaterMinAreaM2 = 400.0f;

	/** Humedad extra en la orilla: las especies humedas crecen junto al agua. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Agua",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WaterMoistureBoost = 0.25f;

	/** A que distancia de la orilla se desvanece esa humedad, en metros. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Agua",
		meta = (ClampMin = "1.0"))
	float WaterMoistureRadiusMeters = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Humedad",
		meta = (ClampMin = "0.01"))
	float MoistureFrequency = 1.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Humedad",
		meta = (ClampMin = "1", ClampMax = "8"))
	int32 MoistureOctaves = 3;

	/** Convierte al tipo del nucleo. */
	jardin::GenerationSettings ToCore() const;
};

/**
 * Bioma de color: el matiz de la vegetacion y del suelo de un jardin.
 *
 * La semilla sortea una paleta (con su peso) y el actor la escribe en una
 * Material Parameter Collection. Los materiales conservan el sombreado de sus
 * texturas y solo cambian el matiz. El agua no se toca.
 */
USTRUCT(BlueprintType)
struct JARDIN_API FJardinPalette
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bioma")
	FString Name = TEXT("Verde");

	/** Probabilidad relativa de que la semilla elija esta paleta. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bioma", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;

	/** Cuanto se recolorea la vegetacion: 0 = colores originales del kit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bioma", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Strength = 1.0f;

	/** Copas de los arboles (pinos y comunes). Color lineal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bioma")
	FLinearColor Foliage = FLinearColor(0.30f, 0.48f, 0.14f);

	/** Pasto, arbustos y helechos. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bioma")
	FLinearColor Grass = FLinearColor(0.30f, 0.48f, 0.14f);

	/** Suelo de la pradera y del bosque, en el material del terreno. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bioma")
	FLinearColor Meadow = FLinearColor(0.30f, 0.48f, 0.14f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bioma")
	FLinearColor Forest = FLinearColor(0.12f, 0.24f, 0.08f);

	/**
	 * Coeficientes de dispersion de Rayleigh del cielo (SkyAtmosphere). Es lo
	 * que hace azul el cielo de la Tierra: el aire dispersa ~6 veces mas el azul
	 * que el rojo. Cambiar la proporcion cambia el color del cielo, del
	 * horizonte y de la luz ambiental de forma fisicamente coherente.
	 * Por defecto, los de la Tierra.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bioma|Cielo")
	FLinearColor SkyScattering = FLinearColor(0.175287f, 0.409607f, 1.0f);

	/** Matiz de la niebla, para que el horizonte se funda con el cielo. Solo
	 *  cuenta el matiz: el brillo lo conserva la niebla del ambiente. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bioma|Cielo")
	FLinearColor FogTint = FLinearColor(0.45f, 0.62f, 0.95f);

	/**
	 * Tonalidades: una parte de los arboles y del pasto toma el "color primo",
	 * el matiz vecino (en la Tierra, el verde amarillento). Grados de giro del
	 * matiz en el circulo HSV: negativo hacia el amarillo/rojo, positivo hacia
	 * el azul/magenta.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bioma|Tonalidades",
		meta = (ClampMin = "-180.0", ClampMax = "180.0"))
	float CousinHueDegrees = -25.0f;

	/** El bioma tiene agua propia. Si es falso, el agua es la de MI_Agua
	 *  (el turquesa de siempre). Los coeficientes los calcula el script a partir
	 *  de un color (columna "agua" de PALETAS), en 1/cm, como los usa el motor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bioma|Agua")
	bool bWaterOverride = false;

	/** Absorcion por color (1/cm): el agua se "come" estos canales con la profundidad. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bioma|Agua")
	FLinearColor WaterAbsorption = FLinearColor(0.0022f, 0.00028f, 0.00018f);

	/** Dispersion por color (1/cm): el velo de color que se ve en lo hondo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bioma|Agua")
	FLinearColor WaterScattering = FLinearColor(0.00002f, 0.00012f, 0.00014f);

	/** Tinte del fondo visto a traves del agua (1 = ninguno). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bioma|Agua")
	FLinearColor WaterTint = FLinearColor(0.70f, 1.0f, 1.05f);

	/** Cuanto varian los tonos entre ejemplares: 0 = todos iguales. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bioma|Tonalidades",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Variety = 1.0f;
};

/**
 * Parametros del arbol fractal. Espejo de jardin::FractalTreeParams.
 *
 * La regla: cada rama produce Branches ramas hijas, cada una LengthDecay veces
 * mas corta y RadiusDecay veces mas delgada, abiertas BranchAngle grados, hasta
 * Depth niveles. En las puntas, racimos de hojas.
 */
USTRUCT(BlueprintType)
struct JARDIN_API FJardinFractalSettings
{
	GENERATED_BODY()

	/** Niveles de ramificacion, contando el tronco. Las ramas crecen como Branches^Depth. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fractal", meta = (ClampMin = "1", ClampMax = "7"))
	int32 Depth = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fractal", meta = (ClampMin = "1", ClampMax = "5"))
	int32 Branches = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fractal", meta = (ClampMin = "0.1"))
	float TrunkLengthMeters = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fractal", meta = (ClampMin = "0.01"))
	float TrunkRadiusMeters = 0.22f;

	/** Razon de autosimilitud: cuanto mide cada rama respecto a su madre. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fractal", meta = (ClampMin = "0.1", ClampMax = "0.95"))
	float LengthDecay = 0.72f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fractal", meta = (ClampMin = "0.1", ClampMax = "0.95"))
	float RadiusDecay = 0.62f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fractal", meta = (ClampMin = "0.0", ClampMax = "89.0"))
	float BranchAngleDegrees = 34.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fractal", meta = (ClampMin = "0.0", ClampMax = "45.0"))
	float AngleJitterDegrees = 10.0f;

	/** Tendencia de las ramas a crecer hacia arriba. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fractal", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float UpBias = 0.30f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fractal", meta = (ClampMin = "0.0"))
	float LeafSizeMeters = 0.85f;

	/** Tramos por rama. Con mas de 1, la rama se curva entre tramos. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fractal|Forma", meta = (ClampMin = "1", ClampMax = "6"))
	int32 SegmentsPerBranch = 3;

	/** Desviacion maxima entre tramos, en grados: ramas algo nudosas. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fractal|Forma", meta = (ClampMin = "0.0", ClampMax = "45.0"))
	float BendDegrees = 12.0f;

	/** Donde nacen las ramas laterales a lo largo de la madre. 1 = todas en la
	 *  punta (forma de paraguas); 0.55 = desde la mitad hacia arriba. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fractal|Forma", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BranchStart = 0.55f;

	/** Ensanchamiento de la base del tronco. 0.5 = 50% mas ancha. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fractal|Forma", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float TrunkFlare = 0.5f;

	/** Racimos redondeados (icosaedro deformado) en vez de octaedros. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fractal|Hojas")
	bool bRoundLeaves = true;

	/** Cuantos de los ultimos niveles llevan hojas. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fractal|Hojas", meta = (ClampMin = "1", ClampMax = "7"))
	int32 LeafLevels = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fractal|Hojas", meta = (ClampMin = "1", ClampMax = "6"))
	int32 LeavesPerTip = 1;

	/** Deformacion de los racimos redondeados. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fractal|Hojas", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float LeafJitter = 0.30f;

	/** Cuanto cambia la forma entre ejemplares de la especie (apertura, altura,
	 *  copa, ramas por nudo). 0 = misma silueta para todos. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fractal|Variedad", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Variation = 0.5f;

	jardin::FractalTreeParams ToCore() const;
};

/**
 * Configuracion de una familia de objetos: arboles, rocas, arbustos.
 * Espejo de jardin::PropLayer, mas la malla que se instancia.
 */
USTRUCT(BlueprintType)
struct JARDIN_API FJardinPropLayer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capa")
	FString Name = TEXT("ArbolA");

	/** Malla que se instancia. Si esta vacia, la capa se calcula pero no se dibuja. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capa")
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	/**
	 * Variantes de malla (por ejemplo, los 5 pinos del kit). Si tiene elementos,
	 * cada objeto elige una de forma determinista y Mesh se ignora.
	 * Si esta vacia, toda la capa usa Mesh.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capa")
	TArray<TObjectPtr<UStaticMesh>> Variants;

	/** Las mallas que realmente usa la capa: Variants si hay, si no Mesh. */
	TArray<UStaticMesh*> EffectiveMeshes() const;

	/**
	 * En vez de instanciar una malla, cada objeto de la capa es un arbol
	 * fractal construido por codigo, con su propia subsemilla: todos distintos.
	 * Pensado para pocos ejemplares (decenas): cada uno es una malla propia.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arbol fractal")
	bool bFractalTree = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arbol fractal", meta = (EditCondition = "bFractalTree"))
	FJardinFractalSettings Fractal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arbol fractal", meta = (EditCondition = "bFractalTree"))
	TObjectPtr<UMaterialInterface> FractalBarkMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arbol fractal", meta = (EditCondition = "bFractalTree"))
	TObjectPtr<UMaterialInterface> FractalLeafMaterial = nullptr;

	/** El tronco y las ramas bloquean al jugador. Cocinar la colision cuesta:
	 *  con cientos de arboles pequenos conviene apagarla y dejarla en los grandes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arbol fractal", meta = (EditCondition = "bFractalTree"))
	bool bFractalCollision = true;

	/** Si es falso, la capa se reparte (no cambia nada del resto del jardin)
	 *  pero no se dibuja. Se usa para el pasto del kit cuando esta el pasto denso. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capa")
	bool bVisible = true;

	/** Solo se dibuja de noche (luciernagas). La capa se reparte siempre, para
	 *  que el conteo y el resto del jardin no dependan de la hora; de dia
	 *  simplemente no se crean sus instancias. Tampoco proyecta sombra. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capa")
	bool bNightOnly = false;

	/** Separacion nominal de la rejilla de candidatos, en metros.
	 *  Es el techo de densidad: bajarlo es la forma de tener mas arboles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Distribucion",
		meta = (ClampMin = "0.25"))
	float SpacingMeters = 4.5f;

	/** Espacio que reserva el objeto, en metros. Dos objetos chocan si su
	 *  distancia es menor que la suma de sus radios. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Distribucion",
		meta = (ClampMin = "0.05"))
	float ExclusionRadiusMeters = 2.4f;

	/** Frecuencia de la mascara de densidad. Baja agrupa en manchas grandes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Distribucion")
	float DensityFrequency = 2.2f;

	/** 0 acepta casi todo, 1 casi nada. Subirlo es la forma de tener menos. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Distribucion",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DensityThreshold = 0.20f;

	/** Pendiente maxima admitida, en [0,1]. 0 plano, 1 vertical. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reglas",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxSlope = 0.32f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reglas",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinHeight01 = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reglas",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxHeight01 = 0.85f;

	/** Zonas donde puede aparecer. Vacio significa todas. 0 = Pradera, 1 = Bosque. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reglas")
	TArray<uint8> AllowedZones;

	/** Margen al borde del mapa, en metros. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reglas",
		meta = (ClampMin = "0.0"))
	float BoundsMarginMeters = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variacion",
		meta = (ClampMin = "0.001"))
	float MinScale = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variacion",
		meta = (ClampMin = "0.001"))
	float MaxScale = 1.30f;

	/** Tope duro. Red de seguridad contra una configuracion accidental. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variacion",
		meta = (ClampMin = "1"))
	int32 MaxCount = 20000;

	// ------------------------------------------------------------ ecologia
	// Valores por defecto = comportamiento de la Fase 0.

	/** Humedad minima que tolera la especie. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ecologia",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinMoisture = 0.0f;

	/** Humedad maxima que tolera la especie. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ecologia",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxMoisture = 1.0f;

	/** 0 = objetos salpicados por todo el mapa. 1 = manchas con borde
	 *  definido: arboledas, praderas de flores. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ecologia",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Clustering = 0.0f;

	/** Es un arbol que da sombra. Las capas de sotobosque y de claro se fijan
	 *  en estos objetos. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ecologia")
	bool bIsCanopy = false;

	/** Solo crece a la sombra de un arbol (helechos, hongos). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ecologia")
	bool bRequireCanopy = false;

	/** Distancia maxima al arbol mas cercano, en metros. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ecologia",
		meta = (ClampMin = "0.1", EditCondition = "bRequireCanopy"))
	float CanopyDistanceMeters = 5.0f;

	/** Solo crece al sol, lejos de los arboles (flores, pasto). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ecologia")
	bool bAvoidCanopy = false;

	/** Distancia minima al arbol mas cercano, en metros. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ecologia",
		meta = (ClampMin = "0.1", EditCondition = "bAvoidCanopy"))
	float ClearingDistanceMeters = 4.0f;

	/** Distancia minima a la orilla, en metros. Solo cuenta si hay agua. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ecologia",
		meta = (ClampMin = "0.0"))
	float MinWaterDistanceMeters = 1.0f;

	/** A partir de esta distancia a la camara la capa no se dibuja (metros).
	 *  0 = siempre. Pasto, flores y piedras a 100 m ya no ocupan un pixel,
	 *  pero cuestan lo mismo que de cerca. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendimiento",
		meta = (ClampMin = "0.0"))
	float CullDistanceMeters = 0.0f;

	/** Distancia minima al BORDE del camino, en metros. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reglas",
		meta = (ClampMin = "0.0"))
	float MinPathDistanceMeters = 0.5f;

	/** Convierte al tipo del nucleo. */
	jardin::PropLayer ToCore() const;

	/** Las tres capas del protocolo: dos arboles y una roca. */
	static TArray<FJardinPropLayer> DefaultLayers();
};
