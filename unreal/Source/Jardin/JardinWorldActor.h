// JardinWorldActor.h — El actor que genera el jardin y lo lleva a la escena.
//
// Division de responsabilidades, deliberada:
//   Core/   decide COMO es el mundo. C++ plano, sin Unreal, probado aparte.
//   Aqui    traduce esa decision a geometria, instancias y texturas.
//
// Si algun dia cambia la forma de dibujar, se reescribe este archivo y el
// algoritmo no se toca. Y al reves: el nucleo se puede mejorar sin abrir el motor.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JardinTypes.h"
#include "JardinWorldActor.generated.h"

class UProceduralMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UTexture2D;
class UMaterialParameterCollection;

UCLASS()
class JARDIN_API AJardinWorldActor : public AActor
{
	GENERATED_BODY()

public:
	AJardinWorldActor();

	// ---------------------------------------------------------------- config

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jardin")
	FJardinGenerationSettings Settings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jardin")
	TArray<FJardinPropLayer> PropLayers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jardin")
	TObjectPtr<UMaterialInterface> TerrainMaterial;

	/** Material de la calzada del camino. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jardin")
	TObjectPtr<UMaterialInterface> PathMaterial;

	/** Material de la superficie del agua (Single Layer Water). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jardin")
	TObjectPtr<UMaterialInterface> WaterMaterial;

	/** Biomas de color. La semilla sortea uno segun su peso. Vacio = colores
	 *  originales. Los llena construir_jardin.py (tabla PALETAS). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jardin|Bioma")
	TArray<FJardinPalette> Palettes;

	/** -1 = la semilla elige; 0..N-1 = paleta fija (para comparar o capturar). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jardin|Bioma", meta = (ClampMin = "-1"))
	int32 ForcedPalette = -1;

	/** Coleccion de parametros (MPC_Bioma) que leen los materiales de la
	 *  vegetacion y del terreno: Follaje, Pasto, Pradera, Bosque, Recolor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jardin|Bioma")
	TObjectPtr<UMaterialParameterCollection> BiomeCollection;

	/** Si esta activo, el jardin se regenera con semilla nueva al empezar la
	 *  partida. Es lo que hace que cada vez que entres haya un jardin distinto. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jardin")
	bool bRandomSeedOnBeginPlay = true;

	// ---------------------------------------------------------------- pasto denso
	// Tapiz de pasto solo alrededor de la camara, por trozos (ver Core/JardinGrass.h).
	// Solo en la partida: en el viewport del editor no hay camara de jugador.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jardin|Pasto denso")
	bool bDenseGrass = false;

	/** Matas generadas por codigo (Core/JardinGrass: briznas finas con
	 *  normales hacia arriba). Si es falso se usan GrassMeshes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jardin|Pasto denso")
	bool bProceduralGrass = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jardin|Pasto denso", meta = (ClampMin = "1", ClampMax = "64"))
	int32 GrassBlades = 32;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jardin|Pasto denso", meta = (ClampMin = "0.02"))
	float GrassBladeHeightMeters = 0.20f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jardin|Pasto denso", meta = (ClampMin = "0.01"))
	float GrassClumpRadiusMeters = 0.22f;

	/** Ancho de cada brizna en la base. Mas ancho = el pasto tapa mas suelo,
	 *  sin costar un triangulo mas. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jardin|Pasto denso", meta = (ClampMin = "0.002"))
	float GrassBladeWidthMeters = 0.028f;

	/** Matas distintas que se generan (cada trozo las mezcla). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jardin|Pasto denso", meta = (ClampMin = "1", ClampMax = "8"))
	int32 GrassClumpVariants = 4;

	/** Matas de pasto (una variante por malla). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jardin|Pasto denso")
	TArray<TObjectPtr<UStaticMesh>> GrassMeshes;

	/** Material para todas las matas (con el desvanecido a lo lejos). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jardin|Pasto denso")
	TObjectPtr<UMaterialInterface> GrassMaterial;

	/** Radio alrededor de la camara, en metros. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jardin|Pasto denso", meta = (ClampMin = "8.0"))
	float GrassRadiusMeters = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jardin|Pasto denso", meta = (ClampMin = "4.0"))
	float GrassChunkMeters = 16.0f;

	/** Candidatas por m2 antes de manchas, zona y humedad. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jardin|Pasto denso", meta = (ClampMin = "0.1", ClampMax = "25.0"))
	float GrassDensity = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jardin|Pasto denso", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GrassForestFactor = 0.3f;

	/** Valor de la mancha por debajo del cual el suelo queda desnudo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jardin|Pasto denso", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GrassPatchCut = 0.30f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jardin|Pasto denso", meta = (ClampMin = "0.01"))
	float GrassMinScale = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jardin|Pasto denso", meta = (ClampMin = "0.01"))
	float GrassMaxScale = 1.3f;

	/** Trozos nuevos como maximo por frame: reparte el trabajo y evita tirones. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jardin|Pasto denso", meta = (ClampMin = "1"))
	int32 GrassChunksPerFrame = 4;

	/** Trozos de pasto vivos ahora mismo (para el HUD). */
	int32 GetGrassChunkCount() const { return GrassChunks.Num(); }
	int32 GetGrassInstanceCount() const { return GrassInstanceCount; }

	virtual void Tick(float DeltaSeconds) override;

	/** Es de noche: se dibujan las capas nocturnas (luciernagas). Lo pone
	 *  atmosfera.py segun el AMBIENTE elegido. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jardin")
	bool bIsNight = false;

	/** Al salir de Play, el jardin del editor adopta la ultima semilla que
	 *  se vio en el juego. Sin esto, el viewport vuelve siempre a la semilla
	 *  guardada en el nivel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jardin")
	bool bKeepPlaySeedInEditor = true;

	/**
	 * Invierte el orden de los triangulos.
	 *
	 * El nucleo viene de una version donde la altura era el eje Y; en Unreal
	 * la altura es Z, asi que la mano del sistema de coordenadas se invierte y
	 * con ella el sentido de giro de las caras.
	 *
	 * Sintoma de que hay que cambiar esta casilla: el terreno se ve desde abajo
	 * pero desaparece visto desde arriba.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jardin|Avanzado")
	bool bInvertTriangleWinding = true;

	// ---------------------------------------------------------------- acciones

	/** Genera el jardin completo: campos, zonas, malla e instancias. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Jardin")
	void Generate();

	/** Semilla nueva al azar y regenerar. Un jardin distinto cada vez. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Jardin")
	void RandomizeSeedAndGenerate();

	/**
	 * REGLA DE CONTROL: genera el mismo mundo varias veces y compara su huella.
	 * Si dos ejecuciones con la misma semilla difieren, hay aleatoriedad no
	 * controlada y la fase no esta terminada.
	 *
	 * Causas habituales: FMath::Rand dentro de la generacion, iterar un TMap
	 * asumiendo orden, o depender del tiempo o del orden de tick.
	 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Jardin|Verificacion")
	void CheckDeterminism();

	// ---------------------------------------------------------------- consultas

	/**
	 * Construye la textura de un mapa de depuracion.
	 * Devuelve nullptr para EJardinDebugView::Normal o si aun no hay mundo.
	 *
	 * Esto es lo que demuestra que el jardin no es decoracion: se ve el dato
	 * numerico que produjo cada decision visual.
	 */
	UFUNCTION(BlueprintCallable, Category = "Jardin|Debug")
	UTexture2D* CreateDebugTexture(EJardinDebugView View) const;

	/** Altura del terreno, en unidades de Unreal, en una posicion de mundo. */
	UFUNCTION(BlueprintPure, Category = "Jardin")
	float GetTerrainHeightAt(float WorldX, float WorldY) const;

	UFUNCTION(BlueprintPure, Category = "Jardin")
	bool HasWorld() const { return World.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "Jardin")
	int32 GetSeed() const { return Settings.Seed; }

	UFUNCTION(BlueprintPure, Category = "Jardin")
	int32 GetTotalProps() const { return TotalProps; }

	UFUNCTION(BlueprintPure, Category = "Jardin")
	TArray<int32> GetPropCounts() const { return PropCounts; }

	/** Regimen hidrico que salio con esta semilla ("Lagos", "Isla"...). */
	UFUNCTION(BlueprintPure, Category = "Jardin")
	FString GetWaterProfileName() const;

	/** Caracter del relieve que salio ("Sierras", "Llanura"...). */
	UFUNCTION(BlueprintPure, Category = "Jardin")
	FString GetReliefName() const;

	/** Bioma de color que salio con esta semilla. */
	UFUNCTION(BlueprintPure, Category = "Jardin")
	FString GetPaletteName() const { return PaletteName; }

	/** Posicion (cm, mundo) y direccion del camino a S metros del inicio.
	 *  Falso si no hay camino. Lo usa la prueba de rendimiento. */
	bool GetPathPose(float SMeters, FVector& OutPosition, FVector& OutDirection) const;

	/** Longitud del camino en metros (0 = sin camino). */
	UFUNCTION(BlueprintPure, Category = "Jardin")
	float GetPathLength() const;

	/** Porcentaje del mapa cubierto de agua. */
	UFUNCTION(BlueprintPure, Category = "Jardin")
	float GetWaterPercent() const { return WaterPercent; }

	UFUNCTION(BlueprintPure, Category = "Jardin")
	float GetWorldSizeUU() const { return Settings.WorldSizeMeters * JardinUnits::MetersToUU; }

	/** Huella del mundo actual, en hexadecimal. Para la prueba de reproducibilidad. */
	UFUNCTION(BlueprintPure, Category = "Jardin|Debug")
	FString GetChecksum() const { return Checksum; }

	/** Tiempo de la ultima generacion completa (campos + colocacion), en ms. */
	UFUNCTION(BlueprintPure, Category = "Jardin|Debug")
	float GetLastGenerationMs() const { return GenerationMs + ScatterMs; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Al abrir el nivel en el editor no hay mundo generado, porque nunca se
	 *  guarda: se reconstruye aqui a partir de la semilla y los parametros. */
	virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	/** Ctrl+Z restaura los parametros; el jardin tiene que seguirlos. */
	virtual void PostEditUndo() override;
#endif

private:
	void BuildTerrainMesh();
	void BuildProps();
	void BuildWaterMesh();
	void BuildPathMesh();

	/** Crea los trozos de pasto que faltan alrededor de la camara y quita los lejanos. */
	void UpdateGrass();
	void BuildGrassChunk(const FIntPoint& Chunk);
	void ClearGrass();

	/** Construye las matas procedurales como mallas estaticas (una vez). */
	void EnsureProceduralGrassMeshes();
	const TArray<TObjectPtr<UStaticMesh>>& ActiveGrassMeshes() const
	{
		return bProceduralGrass ? GeneratedGrassMeshes : GrassMeshes;
	}

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMesh>> GeneratedGrassMeshes;

	/** Trozo -> sus componentes (uno por variante). Los componentes cuelgan del
	 *  actor (OwnedComponents), asi que el recolector de basura no los toca. */
	TMap<FIntPoint, TArray<UInstancedStaticMeshComponent*>> GrassChunks;
	int32 GrassInstanceCount = 0;
	void EnsureLayers();

	/** Sortea el bioma de color y lo escribe en BiomeCollection. */
	void ApplyPalette();

	/** Destruye TODO lo generado que cuelgue del actor, incluidos restos de
	 *  versiones anteriores que se guardaron en el nivel por error. */
	void DestroyGeneratedComponents();

	/** Construye un arbol fractal unico en la posicion de un objeto colocado. */
	void SpawnFractalTree(const jardin::Placement& P, const FJardinPropLayer& Layer, uint32 TreeSeed);

	/**
	 * True si este actor vive en un mundo real y puede crear componentes.
	 *
	 * Falso para la plantilla de clase y para las copias temporales que el
	 * editor fabrica al copiar, pegar o duplicar un actor: esas copias no
	 * tienen mundo, y registrar un componente en ellas dispara
	 * ensure(MyOwnerWorld) en ActorComponent.cpp.
	 */
	bool CanBuildInWorld() const;

	/**
	 * La malla del terreno. Transient a proposito: el nivel guarda la RECETA del
	 * jardin (semilla y parametros), nunca el resultado. Guardar el resultado
	 * llego a ocupar 177 MB por actor y dejaba restos de jardines viejos.
	 */
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Jardin|Componentes")
	TObjectPtr<UProceduralMeshComponent> TerrainMesh;

	/** Cinta del camino. Transient, como el terreno. */
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Jardin|Componentes")
	TObjectPtr<UProceduralMeshComponent> PathMesh;

	/** Superficie de los lagos y el mar. Transient, como el terreno. */
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Jardin|Componentes")
	TObjectPtr<UProceduralMeshComponent> WaterMesh;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> PropComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UProceduralMeshComponent>> FractalComponents;

	// ---- estado, solo lectura desde el inspector ----

	UPROPERTY(VisibleAnywhere, Category = "Jardin|Estado")
	int32 VertexCount = 0;

	UPROPERTY(VisibleAnywhere, Category = "Jardin|Estado")
	int32 TotalProps = 0;

	UPROPERTY(VisibleAnywhere, Category = "Jardin|Estado")
	TArray<int32> PropCounts;

	UPROPERTY(VisibleAnywhere, Category = "Jardin|Estado")
	float GenerationMs = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Jardin|Estado")
	float ScatterMs = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Jardin|Estado")
	FString Checksum;

	UPROPERTY(VisibleAnywhere, Category = "Jardin|Estado")
	float WaterPercent = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Jardin|Estado")
	FString PaletteName;

	/**
	 * El mundo generado. No es un UObject y no puede ser UPROPERTY: es un tipo
	 * del nucleo, que a proposito no sabe nada de Unreal.
	 */
	TUniquePtr<jardin::WorldData> World;
};
