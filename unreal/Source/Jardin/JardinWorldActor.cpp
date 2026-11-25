#include "JardinWorldActor.h"

#include "ProceduralMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "TextureResource.h"
#include "Core/JardinWater.h"
#include "Core/JardinPath.h"
#include "Core/JardinGrass.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshResources.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogJardin, Log, All);

using namespace JardinUnits;

// ---------------------------------------------------------------- ctor

AJardinWorldActor::AJardinWorldActor()
{
	// Tick solo para el pasto denso (sigue a la camara). Sin pasto denso el
	// Tick sale en la primera linea.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// La malla del terreno ya NO es un subobjeto fijo: se crea en cada
	// generacion como objeto temporal (ver BuildTerrainMesh). Asi el nivel
	// guarda solo la semilla y los parametros, nunca el resultado.

	PropLayers = FJardinPropLayer::DefaultLayers();
}

void AJardinWorldActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Al abrir el nivel no hay nada generado, porque no se guarda. Tambien se
	// llama al mover el actor: ahi ya hay mundo y no se regenera, los
	// componentes se mueven con el.
	if (!World.IsValid() || !IsValid(TerrainMesh))
	{
		Generate();
	}
}

void AJardinWorldActor::DestroyGeneratedComponents()
{
	// Se buscan TODOS los componentes de estos tipos, no solo los que este
	// actor recuerda haber creado. Versiones anteriores guardaban sus
	// componentes en el nivel; al reabrirlo, quedaban huerfanos y se dibujaban
	// encima del jardin nuevo (las esferas, conos y cubos fantasma).
	// ISM incluye HISM (subclase) y el pasto denso.
	TInlineComponentArray<UInstancedStaticMeshComponent*> Hisms(this);
	for (UInstancedStaticMeshComponent* C : Hisms)
	{
		if (IsValid(C)) C->DestroyComponent();
	}

	TInlineComponentArray<UProceduralMeshComponent*> Meshes(this);
	for (UProceduralMeshComponent* C : Meshes)
	{
		if (IsValid(C)) C->DestroyComponent();
	}

	GrassChunks.Reset();
	GrassInstanceCount = 0;
	// Las matas procedurales salen de la semilla: con otro jardin, otras matas.
	GeneratedGrassMeshes.Reset();
	PropComponents.Reset();
	FractalComponents.Reset();
	TerrainMesh = nullptr;
	WaterMesh = nullptr;
	PathMesh = nullptr;
}

void AJardinWorldActor::BeginPlay()
{
	Super::BeginPlay();

	if (bRandomSeedOnBeginPlay)
	{
		RandomizeSeedAndGenerate();
	}
	else
	{
		Generate();
	}
}

void AJardinWorldActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

#if WITH_EDITOR
	// La partida (PIE) trabaja sobre una COPIA del nivel: lo que cambia ahi se
	// tira al salir. Para que el viewport muestre el ultimo jardin jugado, se
	// busca el actor original en el mundo del editor y se le pasa la semilla.
	//
	// No se filtra por EndPlayReason: con World Partition el actor vive en una
	// celda de streaming y al cerrar la partida sale con RemovedFromWorld, no
	// con EndPlayInEditor. Lo que importa es que este actor sea la copia de PIE.
	const UWorld* MyWorld = GetWorld();
	if (!bKeepPlaySeedInEditor || !GEngine || !MyWorld || MyWorld->WorldType != EWorldType::PIE)
	{
		return;
	}

	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* EditorWorld = Context.World();
		if (Context.WorldType != EWorldType::Editor || !EditorWorld) continue;

		// Se identifica por nombre (PIE lo conserva al copiar). Si en el nivel
		// hay un solo jardin, no hace falta: es ese.
		TArray<AJardinWorldActor*> Candidates;
		for (TActorIterator<AJardinWorldActor> It(EditorWorld); It; ++It)
		{
			Candidates.Add(*It);
		}

		for (AJardinWorldActor* Original : Candidates)
		{
			if (Candidates.Num() > 1 && Original->GetFName() != GetFName()) continue;
			if (Original->Settings.Seed == Settings.Seed) continue;

			UE_LOG(LogJardin, Log, TEXT("Fin de Play: el editor adopta la semilla %d"), Settings.Seed);
			Original->Modify();
			Original->Settings.Seed = Settings.Seed;
			Original->Generate();
		}
	}
#endif
}

#if WITH_EDITOR
void AJardinWorldActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Regenerar al tocar cualquier parametro en el inspector.
	// Con ~7 ms de generacion, ajustar el ruido se vuelve interactivo.
	//
	// Solo en actores reales: el editor tambien llama a esto sobre copias
	// temporales sin mundo (al copiar o duplicar), y ahi no hay que construir nada.
	if (CanBuildInWorld())
	{
		Generate();
	}
}

void AJardinWorldActor::PostEditUndo()
{
	Super::PostEditUndo();

	if (CanBuildInWorld())
	{
		Generate();
	}
}
#endif

bool AJardinWorldActor::CanBuildInWorld() const
{
	return GetWorld() != nullptr
		&& !IsTemplate()
		&& !HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject);
}

void AJardinWorldActor::EnsureLayers()
{
	if (PropLayers.Num() == 0)
	{
		PropLayers = FJardinPropLayer::DefaultLayers();
	}
}

// ---------------------------------------------------------------- generate

void AJardinWorldActor::Generate()
{
	if (!CanBuildInWorld())
	{
		return;
	}

	EnsureLayers();

	const jardin::ZoneSet Zones = jardin::ZoneSet::Default();

	jardin::WorldData Generated = jardin::WorldGenerator::Generate(Settings.ToCore(), Zones);
	GenerationMs = static_cast<float>(jardin::WorldGenerator::GetLastGenerationMs());

	World = MakeUnique<jardin::WorldData>(MoveTemp(Generated));

	Checksum = FString::Printf(TEXT("%08X"), World->Checksum());
	VertexCount = World->Side * World->Side;

	int32 Wet = 0;
	for (float L : World->WaterLevel) if (L >= 0.0f) ++Wet;
	WaterPercent = World->HasWater() ? 100.0f * Wet / static_cast<float>(VertexCount) : 0.0f;

	ApplyPalette();

	DestroyGeneratedComponents();
	BuildTerrainMesh();
	BuildWaterMesh();
	BuildPathMesh();
	BuildProps();

	UE_LOG(LogJardin, Log,
		TEXT("Jardin generado. Semilla %d | %d vertices | %d objetos | relieve: %s | bioma: %s | agua: %s %.0f%% | %.1f ms + %.1f ms | checksum %s"),
		Settings.Seed, VertexCount, TotalProps, *GetReliefName(), *PaletteName, *GetWaterProfileName(), WaterPercent,
		GenerationMs, ScatterMs, *Checksum);
}

void AJardinWorldActor::RandomizeSeedAndGenerate()
{
	// Aqui si es legitimo usar el azar del motor: elegir la semilla es una
	// accion del usuario, no parte de la generacion. A partir de ese numero,
	// todo lo demas es determinista y reproducible.
	//
	// Modify(): desde el boton del editor, sin esto la semilla nueva no entra
	// en la transaccion; el nivel no se marca como modificado y al guardar o
	// reabrir vuelve la semilla anterior.
	Modify();
	Settings.Seed = FMath::Rand();
	Generate();
}

// ---------------------------------------------------------------- terrain

void AJardinWorldActor::BuildTerrainMesh()
{
	if (!World.IsValid()) return;

	const int32 Side = World->Side;
	const int32 Res = World->Resolution;
	const int32 NumVerts = Side * Side;
	const int32 NumQuads = Res * Res;

	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> UV0, UV1, UV2, UV3;
	TArray<FLinearColor> Colors;
	TArray<FProcMeshTangent> Tangents;
	TArray<int32> Triangles;

	Vertices.SetNumUninitialized(NumVerts);
	Normals.SetNumUninitialized(NumVerts);
	UV0.SetNumUninitialized(NumVerts);
	UV1.SetNumUninitialized(NumVerts);
	Colors.SetNumUninitialized(NumVerts);
	Tangents.SetNumUninitialized(NumVerts);

	// UV2.X = mascara del camino: 1 en la calzada, 0 al final del arcen. El
	// material pinta ahi tierra pisada. Sin camino, UV2 queda vacio.
	const bool bPath = World->HasPath();
	if (bPath) UV2.SetNumUninitialized(NumVerts);
	const float PathHalf = bPath ? World->Path.Width * 0.5f : 0.0f;
	const float PathEdge = PathHalf + FMath::Max(0.5f, Settings.PathShoulderMeters);
	Triangles.SetNumUninitialized(NumQuads * 6);

	const float Cell = World->CellSize();

	for (int32 y = 0; y < Side; ++y)
	{
		for (int32 x = 0; x < Side; ++x)
		{
			const int32 i = World->Index(x, y);

			// Mapeo de ejes: el nucleo usa (X, Z) horizontales y la altura
			// aparte. Unreal es Z-up, asi que la altura va a Z y la segunda
			// horizontal del nucleo va a Y.
			Vertices[i] = FVector(
				World->GridToWorldX(x) * MetersToUU,
				World->GridToWorldZ(y) * MetersToUU,
				World->HeightMeters(x, y) * MetersToUU);

			// Normales a partir del gradiente del campo de altura.
			//
			// Se prefiere esto a calcularlas promediando caras: es mas rapido
			// y da normales continuas exactas, sin el facetado visible que
			// aparece en pendientes suaves.
			//
			// Para una superficie z = f(x, y), la normal es (-df/dx, -df/dy, 1)
			// normalizada. Las derivadas se aproximan por diferencias centrales.
			// Estan en metros por metro, asi que las unidades se cancelan y no
			// hay que convertir nada aqui.
			const int32 xm = x > 0 ? x - 1 : x;
			const int32 xp = x < Side - 1 ? x + 1 : x;
			const int32 ym = y > 0 ? y - 1 : y;
			const int32 yp = y < Side - 1 ? y + 1 : y;

			const float dx = (World->HeightMeters(xp, y) - World->HeightMeters(xm, y))
			               / (static_cast<float>(xp - xm) * Cell);
			const float dy = (World->HeightMeters(x, yp) - World->HeightMeters(x, ym))
			               / (static_cast<float>(yp - ym) * Cell);

			Normals[i] = FVector(-dx, -dy, 1.0f).GetSafeNormal();
			Tangents[i] = FProcMeshTangent(1.0f, 0.0f, 0.0f);

			const float u = static_cast<float>(x) / static_cast<float>(Res);
			const float v = static_cast<float>(y) / static_cast<float>(Res);
			UV0[i] = FVector2D(u, v);

			// Datos que la Fase 1 necesita para colorear el terreno por altura
			// y pendiente sin pintar ninguna textura. Calcularlos ahora no
			// cuesta nada; anadirlos despues obligaria a reconstruir la malla.
			UV1[i] = FVector2D(World->Zone[i], World->ZoneBlend[i]);
			// Alfa = cercania al agua: 0 en el agua y la orilla, 1 a mas de
			// 6 m. El material pinta arena humeda donde baja.
			const float Shore = World->HasWater()
				? FMath::Clamp(World->WaterDistance[i] / 6.0f, 0.0f, 1.0f) : 1.0f;
			Colors[i] = FLinearColor(World->Height[i], World->Slope[i], World->Moisture[i], Shore);
			if (bPath)
			{
				const float Mask = 1.0f - FMath::SmoothStep(PathHalf, PathEdge, World->PathDistance[i]);
				UV2[i] = FVector2D(Mask, 0.0f);
			}
		}
	}

	// Cada celda de la rejilla son dos triangulos.
	//
	// El sentido de giro decide hacia donde mira la cara. Con este mapeo de
	// ejes en Unreal, el producto vectorial de las tangentes apunta hacia
	// arriba, asi que el orden natural es (i0, i1, i2). Si el terreno se ve
	// desde abajo pero desaparece visto desde arriba, activa
	// bInvertTriangleWinding en el inspector.
	int32 t = 0;
	for (int32 y = 0; y < Res; ++y)
	{
		for (int32 x = 0; x < Res; ++x)
		{
			const int32 i0 = World->Index(x,     y);
			const int32 i1 = World->Index(x + 1, y);
			const int32 i2 = World->Index(x,     y + 1);
			const int32 i3 = World->Index(x + 1, y + 1);

			if (!bInvertTriangleWinding)
			{
				Triangles[t++] = i0; Triangles[t++] = i1; Triangles[t++] = i2;
				Triangles[t++] = i1; Triangles[t++] = i3; Triangles[t++] = i2;
			}
			else
			{
				Triangles[t++] = i0; Triangles[t++] = i2; Triangles[t++] = i1;
				Triangles[t++] = i1; Triangles[t++] = i2; Triangles[t++] = i3;
			}
		}
	}

	// RF_Transient: este componente nunca se escribe en el nivel.
	TerrainMesh = NewObject<UProceduralMeshComponent>(this, NAME_None, RF_Transient);
	TerrainMesh->SetupAttachment(RootComponent);

	// El terreno es una superficie: no tiene sentido una colision simplificada
	// de cajas. Se usa la malla tal cual.
	TerrainMesh->bUseComplexAsSimpleCollision = true;
	TerrainMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	TerrainMesh->SetCollisionObjectType(ECC_WorldStatic);
	TerrainMesh->RegisterComponent();

	TerrainMesh->CreateMeshSection_LinearColor(
		0, Vertices, Triangles, Normals, UV0, UV1, UV2, UV3, Colors, Tangents,
		/*bCreateCollision=*/ true);

	if (TerrainMaterial)
	{
		TerrainMesh->SetMaterial(0, TerrainMaterial);
	}
}

// ---------------------------------------------------------------- water

bool AJardinWorldActor::GetPathPose(float SMeters, FVector& OutPosition, FVector& OutDirection) const
{
	if (!World.IsValid() || !World->HasPath()) return false;
	const std::vector<jardin::PathSample>& S = World->Path.Samples;

	// Las muestras estan cada ~1 m: el indice sale casi directo de S.
	const float Sc = FMath::Clamp(SMeters, 0.0f, World->Path.Length);
	int32 k = FMath::Clamp(static_cast<int32>(Sc), 0, static_cast<int32>(S.size()) - 2);
	while (k > 0 && S[k].S > Sc) --k;
	while (k + 2 < static_cast<int32>(S.size()) && S[k + 1].S < Sc) ++k;
	const jardin::PathSample& A = S[k];
	const jardin::PathSample& B = S[k + 1];
	const float t = FMath::Clamp((Sc - A.S) / FMath::Max(0.01f, B.S - A.S), 0.0f, 1.0f);

	const FVector Local(FMath::Lerp(A.X, B.X, t) * MetersToUU, FMath::Lerp(A.Z, B.Z, t) * MetersToUU,
	                    FMath::Lerp(A.H, B.H, t) * MetersToUU);
	OutPosition = GetActorTransform().TransformPosition(Local);
	OutDirection = GetActorTransform().TransformVectorNoScale(
		FVector(FMath::Lerp(A.TX, B.TX, t), FMath::Lerp(A.TZ, B.TZ, t), 0.0f).GetSafeNormal());
	return true;
}

float AJardinWorldActor::GetPathLength() const
{
	return World.IsValid() && World->HasPath() ? World->Path.Length : 0.0f;
}

void AJardinWorldActor::BuildPathMesh()
{
	if (!World.IsValid() || !World->HasPath()) return;

	// La cinta la construye el nucleo (ancho constante, perpendicular a la
	// tangente de la curva). Aqui solo se pasa a centimetros y a Z-up.
	const jardin::RibbonMesh R = jardin::PathRibbon::Build(*World);
	if (R.Rows < 2) return;

	const int32 NumVerts = R.Rows * 2;
	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> UV0;
	TArray<FLinearColor> Colors;
	TArray<FProcMeshTangent> Tangents;
	TArray<int32> Triangles;
	Vertices.SetNumUninitialized(NumVerts);
	Normals.SetNumUninitialized(NumVerts);
	UV0.SetNumUninitialized(NumVerts);
	Colors.SetNumUninitialized(NumVerts);
	Tangents.SetNumUninitialized(NumVerts);

	const std::vector<jardin::PathSample>& S = World->Path.Samples;
	for (int32 r = 0; r < R.Rows; ++r)
	{
		// Normal: la calzada es plana de lado a lado y sube o baja a lo largo.
		const int32 rm = r > 0 ? r - 1 : r, rp = r < R.Rows - 1 ? r + 1 : r;
		const float ds = FMath::Max(0.01f, S[rp].S - S[rm].S);
		const float Grade = (S[rp].H - S[rm].H) / ds;
		const FVector N = FVector(-Grade * S[r].TX, -Grade * S[r].TZ, 1.0f).GetSafeNormal();

		for (int32 c = 0; c < 2; ++c)
		{
			const int32 i = R.Index(c, r);
			Vertices[i] = FVector(R.PX[i] * MetersToUU, R.PZ[i] * MetersToUU, R.PY[i] * MetersToUU);
			Normals[i] = N;
			Tangents[i] = FProcMeshTangent(FVector(S[r].TX, S[r].TZ, 0.0f), false);
			UV0[i] = FVector2D(R.U[i], R.V[i]);
			Colors[i] = FLinearColor::White;
		}
	}

	// Mismo patron y mismo sentido de giro que la rejilla del terreno:
	// columna 0/1 = izquierda/derecha, fila = muestra.
	Triangles.Reserve((R.Rows - 1) * 6);
	for (int32 r = 0; r < R.Rows - 1; ++r)
	{
		const int32 i0 = R.Index(0, r), i1 = R.Index(1, r);
		const int32 i2 = R.Index(0, r + 1), i3 = R.Index(1, r + 1);
		if (!bInvertTriangleWinding)
		{
			Triangles.Append({ i0, i1, i2, i1, i3, i2 });
		}
		else
		{
			Triangles.Append({ i0, i2, i1, i1, i2, i3 });
		}
	}

	PathMesh = NewObject<UProceduralMeshComponent>(this, NAME_None, RF_Transient);
	PathMesh->SetupAttachment(RootComponent);
	// Sin colision: se camina sobre el terreno aplanado, 4 cm por debajo.
	PathMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PathMesh->SetCastShadow(false);
	PathMesh->RegisterComponent();
	PathMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UV0, Colors, Tangents, false);
	if (PathMaterial) PathMesh->SetMaterial(0, PathMaterial);
}

// ---------------------------------------------------------------- pasto denso

void AJardinWorldActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bDenseGrass && World.IsValid() && GetWorld() && GetWorld()->IsGameWorld())
	{
		UpdateGrass();
	}
	else if (GrassChunks.Num() > 0)
	{
		ClearGrass();
	}
}

void AJardinWorldActor::ClearGrass()
{
	for (auto& Pair : GrassChunks)
		for (UInstancedStaticMeshComponent* C : Pair.Value)
			if (IsValid(C)) C->DestroyComponent();
	GrassChunks.Reset();
	GrassInstanceCount = 0;
}

void AJardinWorldActor::UpdateGrass()
{
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !PC->PlayerCameraManager) return;
	if (bProceduralGrass) EnsureProceduralGrassMeshes();
	if (ActiveGrassMeshes().Num() == 0) return;

	// Camara en coordenadas del nucleo (metros, relativas al actor).
	const FVector Local = GetActorTransform().InverseTransformPosition(PC->PlayerCameraManager->GetCameraLocation());
	const float Cx = Local.X * UUToMeters, Cz = Local.Y * UUToMeters;
	const float Chunk = GrassChunkMeters;
	const float Keep = GrassRadiusMeters + Chunk * 0.75f;     // se crea hasta aqui...
	const float Drop = GrassRadiusMeters + Chunk * 1.5f;      // ...y se quita mas alla: histeresis, sin parpadeo en el borde

	auto CenterDist = [&](const FIntPoint& P)
	{
		return FVector2f((P.X + 0.5f) * Chunk - Cx, (P.Y + 0.5f) * Chunk - Cz).Size();
	};

	// Quitar los lejanos.
	for (auto It = GrassChunks.CreateIterator(); It; ++It)
	{
		if (CenterDist(It.Key()) > Drop)
		{
			for (UInstancedStaticMeshComponent* C : It.Value())
			{
				if (IsValid(C)) { GrassInstanceCount -= C->GetInstanceCount(); C->DestroyComponent(); }
			}
			It.RemoveCurrent();
		}
	}

	// Los que faltan, del mas cercano al mas lejano, unos pocos por frame.
	const int32 R = FMath::CeilToInt(Keep / Chunk);
	const int32 X0 = jardin::GrassField::ChunkCoord(Cx, Chunk), Z0 = jardin::GrassField::ChunkCoord(Cz, Chunk);
	TArray<TPair<float, FIntPoint>> Missing;
	for (int32 z = Z0 - R; z <= Z0 + R; ++z)
		for (int32 x = X0 - R; x <= X0 + R; ++x)
		{
			const FIntPoint P(x, z);
			const float D = CenterDist(P);
			if (D <= Keep && !GrassChunks.Contains(P)) Missing.Emplace(D, P);
		}
	Missing.Sort([](const TPair<float, FIntPoint>& A, const TPair<float, FIntPoint>& B) { return A.Key < B.Key; });
	for (int32 i = 0; i < Missing.Num() && i < GrassChunksPerFrame; ++i)
	{
		BuildGrassChunk(Missing[i].Value);
	}
}

namespace
{
	// Geometria de una mata (nucleo, metros) -> FMeshDescription (cm, Z arriba).
	void ClumpToMeshDescription(const jardin::GrassClumpMesh& C, FMeshDescription& MD)
	{
		FStaticMeshAttributes Attr(MD);
		Attr.Register();
		TVertexAttributesRef<FVector3f> Pos = Attr.GetVertexPositions();
		TVertexInstanceAttributesRef<FVector3f> Nrm = Attr.GetVertexInstanceNormals();
		TVertexInstanceAttributesRef<FVector3f> Tan = Attr.GetVertexInstanceTangents();
		TVertexInstanceAttributesRef<float> Sgn = Attr.GetVertexInstanceBinormalSigns();
		TVertexInstanceAttributesRef<FVector2f> UVs = Attr.GetVertexInstanceUVs();
		TVertexInstanceAttributesRef<FVector4f> Col = Attr.GetVertexInstanceColors();
		UVs.SetNumChannels(1);

		const int32 NV = static_cast<int32>(C.PX.size());
		MD.ReserveNewVertices(NV);
		MD.ReserveNewVertexInstances(NV);
		MD.ReserveNewTriangles(static_cast<int32>(C.Indices.size() / 3));
		TArray<FVertexInstanceID> VI;
		VI.Reserve(NV);
		for (int32 i = 0; i < NV; ++i)
		{
			const FVertexID Vx = MD.CreateVertex();
			Pos[Vx] = FVector3f(C.PX[i] * JardinUnits::MetersToUU, C.PY[i] * JardinUnits::MetersToUU,
			                    C.PZ[i] * JardinUnits::MetersToUU);
			const FVertexInstanceID In = MD.CreateVertexInstance(Vx);
			// Normal hacia arriba, como el suelo (ver GrassClump::Build).
			Nrm[In] = FVector3f(0.0f, 0.0f, 1.0f);
			Tan[In] = FVector3f(1.0f, 0.0f, 0.0f);
			Sgn[In] = 1.0f;
			UVs.Set(In, 0, FVector2f(C.U[i], C.V[i]));
			// R = altura en la brizna (degradado y peso del viento); G = tono de la brizna.
			Col[In] = FVector4f(C.V[i], C.BladeRand[i], 0.0f, 1.0f);
			VI.Add(In);
		}
		const FPolygonGroupID Group = MD.CreatePolygonGroup();
		Attr.GetPolygonGroupMaterialSlotNames()[Group] = TEXT("Pasto");
		for (size_t t = 0; t + 2 < C.Indices.size(); t += 3)
		{
			MD.CreateTriangle(Group, { VI[C.Indices[t]], VI[C.Indices[t + 1]], VI[C.Indices[t + 2]] });
		}
	}
}

void AJardinWorldActor::EnsureProceduralGrassMeshes()
{
	if (GeneratedGrassMeshes.Num() == GrassClumpVariants) return;
	GeneratedGrassMeshes.Reset();

	jardin::GrassClumpParams P;
	P.Blades = GrassBlades;
	P.Height = GrassBladeHeightMeters;
	P.Radius = GrassClumpRadiusMeters;
	P.Width = GrassBladeWidthMeters;
	const uint32 Base = jardin::SeedBook(World.IsValid() ? World->MasterSeed : 1u).For(jardin::SeedBook::Grass);

	// LODs de la mata: con la MISMA semilla, las primeras N briznas son las
	// mismas que en el LOD0 (el generador las crea en orden). De lejos quedan
	// menos briznas pero mas anchas, asi la mata cubre lo mismo del suelo y
	// el cambio no se nota.
	//                      briznas  tramos  ancho   se usa a partir de (tamano en pantalla)
	struct FLodRow { float Blades; int32 Segments; float Width; float ScreenSize; };
	const FLodRow Lods[] = { { 1.00f, 3, 1.0f, 1.0f },     // de cerca
	                         { 0.40f, 2, 2.0f, 0.030f },   // ~8 m
	                         { 0.15f, 1, 3.0f, 0.010f } }; // ~25 m

	for (int32 v = 0; v < GrassClumpVariants; ++v)
	{
		const uint32 Seed = jardin::hash32::Combine(Base, 1000u + v);
		TArray<FMeshDescription> MDs;
		MDs.SetNum(UE_ARRAY_COUNT(Lods));
		TArray<const FMeshDescription*> Ptrs;
		for (int32 l = 0; l < UE_ARRAY_COUNT(Lods); ++l)
		{
			jardin::GrassClumpParams LP = P;
			LP.Blades = FMath::Max(2, FMath::RoundToInt(P.Blades * Lods[l].Blades));
			LP.Segments = Lods[l].Segments;
			LP.Width = P.Width * Lods[l].Width;
			ClumpToMeshDescription(jardin::GrassClump::Build(LP, Seed), MDs[l]);
			Ptrs.Add(&MDs[l]);
		}

		UStaticMesh* SM = NewObject<UStaticMesh>(this, NAME_None, RF_Transient);
		SM->GetStaticMaterials().Add(FStaticMaterial(GrassMaterial, TEXT("Pasto")));
		UStaticMesh::FBuildMeshDescriptionsParams BP;
		BP.bFastBuild = true;
		BP.bBuildSimpleCollision = false;
		BP.bMarkPackageDirty = false;
		BP.bCommitMeshDescription = false;
		BP.bAllowCpuAccess = false;
		SM->BuildFromMeshDescriptions(Ptrs, BP);
		// La construccion rapida pone LOD1 a 0.75 de pantalla (casi siempre):
		// se fijan los umbrales propios.
		if (FStaticMeshRenderData* RD = SM->GetRenderData())
		{
			for (int32 l = 0; l < UE_ARRAY_COUNT(Lods) && l < RD->LODResources.Num(); ++l)
			{
				RD->ScreenSize[l].Default = Lods[l].ScreenSize;
			}
		}
		GeneratedGrassMeshes.Add(SM);
	}
	UE_LOG(LogJardin, Log, TEXT("Pasto: %d matas procedurales de %d briznas, 3 LODs"), GeneratedGrassMeshes.Num(), GrassBlades);
}

void AJardinWorldActor::BuildGrassChunk(const FIntPoint& Chunk)
{
	jardin::GrassParams P;
	P.Density = GrassDensity;
	P.ForestFactor = GrassForestFactor;
	P.PatchCut = GrassPatchCut;
	P.MinScale = GrassMinScale;
	P.MaxScale = GrassMaxScale;
	const TArray<TObjectPtr<UStaticMesh>>& Meshes = ActiveGrassMeshes();
	P.Variants = Meshes.Num();

	std::vector<jardin::GrassInstance> Out;
	jardin::GrassField::BuildChunk(*World, jardin::SeedBook(World->MasterSeed), Chunk.X, Chunk.Y,
	                               GrassChunkMeters, P, Out);

	TArray<TArray<FTransform>> PerVariant;
	PerVariant.SetNum(Meshes.Num());
	for (const jardin::GrassInstance& G : Out)
	{
		// 3 cm hundida: el terreno es una malla de triangulos y la altura del
		// nucleo es bilineal; asi la base nunca queda flotando.
		PerVariant[G.Variant].Emplace(FRotator(0.0f, G.Yaw, 0.0f),
			FVector(G.X * MetersToUU, G.Z * MetersToUU, (G.H - 0.03f) * MetersToUU), FVector(G.Scale));
	}

	TArray<UInstancedStaticMeshComponent*>& Comps = GrassChunks.Add(Chunk);   // vacio = trozo sin pasto, no se reintenta
	for (int32 v = 0; v < Meshes.Num(); ++v)
	{
		if (PerVariant[v].Num() == 0 || !Meshes[v]) continue;
		UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
		ISM->SetupAttachment(RootComponent);
		ISM->SetStaticMesh(Meshes[v]);
		ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		// Miles de briznas proyectando sombra costarian mas que el resto del
		// jardin; la oclusion ambiental ya las asienta en el suelo.
		ISM->SetCastShadow(false);
		ISM->SetMobility(EComponentMobility::Movable);
		const int32 EndUU = FMath::RoundToInt((GrassRadiusMeters + GrassChunkMeters) * MetersToUU);
		ISM->SetCullDistances(FMath::RoundToInt(GrassRadiusMeters * MetersToUU * 0.9f), EndUU);
		if (GrassMaterial)
		{
			for (int32 m = 0; m < Meshes[v]->GetStaticMaterials().Num(); ++m) ISM->SetMaterial(m, GrassMaterial);
		}
		ISM->RegisterComponent();
		ISM->AddInstances(PerVariant[v], false, false);
		GrassInstanceCount += PerVariant[v].Num();
		Comps.Add(ISM);
	}
}

FString AJardinWorldActor::GetReliefName() const
{
	if (!World.IsValid()) return TEXT("-");
	return UTF8_TO_TCHAR(jardin::ReliefStyleName(World->ReliefStyleId));
}

void AJardinWorldActor::ApplyPalette()
{
	PaletteName = TEXT("Original");
	if (Palettes.Num() == 0) return;

	// El sorteo es del nucleo y con su propia subsemilla: anadir paletas o
	// cambiar sus colores no altera el terreno, el agua ni la colocacion.
	int32 Index = ForcedPalette;
	if (!Palettes.IsValidIndex(Index))
	{
		TArray<float> Weights;
		for (const FJardinPalette& P : Palettes) Weights.Add(P.Weight);
		jardin::DeterministicRandom Rng = jardin::SeedBook(Settings.Seed).Rng(jardin::SeedBook::Palette);
		Index = jardin::PickWeighted(Rng, Weights.GetData(), Weights.Num());
	}
	const FJardinPalette& P = Palettes[Index];
	PaletteName = P.Name;

	// La coleccion tiene una instancia por mundo (editor y partida aparte):
	// cada jardin pinta el suyo.
	UWorld* W = GetWorld();
	if (!W) return;

	if (UMaterialParameterCollectionInstance* Inst = BiomeCollection
		? W->GetParameterCollectionInstance(BiomeCollection) : nullptr)
	{
		Inst->SetScalarParameterValue(TEXT("Recolor"), P.Strength);
		Inst->SetVectorParameterValue(TEXT("Follaje"), P.Foliage);
		Inst->SetVectorParameterValue(TEXT("Pasto"), P.Grass);
		Inst->SetVectorParameterValue(TEXT("Pradera"), P.Meadow);
		Inst->SetVectorParameterValue(TEXT("Bosque"), P.Forest);

		// Color primo: el mismo color con el matiz girado en HSV. El material
		// lo recibe como COCIENTE (primo / base, canal a canal): multiplicado
		// por cualquier color de la familia -tambien las hojas texturizadas del
		// bioma verde original- lo lleva hacia el matiz vecino.
		auto CousinRatio = [&P](const FLinearColor& Base)
		{
			FLinearColor HSV = Base.LinearRGBToHSV();          // R = matiz en grados, G = saturacion, B = valor
			HSV.R = FMath::Fmod(HSV.R + P.CousinHueDegrees + 360.0f, 360.0f);
			const FLinearColor Cousin = HSV.HSVToLinearRGB();
			auto Div = [](float A, float B) { return FMath::Clamp(A / FMath::Max(B, 0.02f), 0.0f, 4.0f); };
			return FLinearColor(Div(Cousin.R, Base.R), Div(Cousin.G, Base.G), Div(Cousin.B, Base.B), 1.0f);
		};
		Inst->SetVectorParameterValue(TEXT("PrimoFollaje"), CousinRatio(P.Foliage));
		Inst->SetVectorParameterValue(TEXT("PrimoPasto"), CousinRatio(P.Grass));
		Inst->SetScalarParameterValue(TEXT("Variedad"), P.Variety);

		// Agua del bioma: M_Agua mezcla sus parametros con estos segun AguaBioma.
		Inst->SetScalarParameterValue(TEXT("AguaBioma"), P.bWaterOverride ? 1.0f : 0.0f);
		Inst->SetVectorParameterValue(TEXT("AguaAbsorcion"), P.WaterAbsorption);
		Inst->SetVectorParameterValue(TEXT("AguaDispersion"), P.WaterScattering);
		Inst->SetVectorParameterValue(TEXT("AguaTinte"), P.WaterTint);
	}

	// Cielo: coeficientes de Rayleigh. El SkyLight con captura en tiempo real
	// recoge el nuevo cielo solo, y con el la luz ambiental de la escena.
	for (TActorIterator<ASkyAtmosphere> It(W); It; ++It)
	{
		if (USkyAtmosphereComponent* Sky = It->GetComponent())
		{
			Sky->SetRayleighScattering(P.SkyScattering);
		}
	}

	// Niebla: se cambia el MATIZ y se conserva su luminancia. Asi sirve para
	// cualquier ambiente de atmosfera.py (la de noche es oscura, la de dia
	// clara) y aplicar paletas una tras otra no la va aclarando ni oscureciendo.
	auto Luma = [](const FLinearColor& C) { return 0.2126f * C.R + 0.7152f * C.G + 0.0722f * C.B; };
	const float TintLuma = Luma(P.FogTint);
	for (TActorIterator<AExponentialHeightFog> It(W); It; ++It)
	{
		UExponentialHeightFogComponent* Fog = It->GetComponent();
		if (!Fog || TintLuma <= KINDA_SMALL_NUMBER) continue;
		const float Current = Luma(Fog->FogInscatteringLuminance);
		FLinearColor C = P.FogTint * (Current / TintLuma);
		C.A = 1.0f;
		Fog->SetFogInscatteringColor(C);
	}
}

FString AJardinWorldActor::GetWaterProfileName() const
{
	if (!World.IsValid() || !World->HasWater()) return TEXT("Sin agua");
	return UTF8_TO_TCHAR(jardin::WaterProfileName(static_cast<jardin::WaterProfile>(World->WaterProfileId)));
}

void AJardinWorldActor::BuildWaterMesh()
{
	if (!World.IsValid() || !World->HasWater()) return;

	const int32 Side = World->Side;
	const int32 Res = World->Resolution;
	const float Cell = World->CellSize();

	// Nivel en cada vertice de la rejilla. Los vertices secos que tocan agua
	// toman el nivel de su vecina mojada: asi la superficie llega hasta mas
	// alla de la orilla y se mete bajo el terreno, sin dejar huecos.
	TArray<float> Level;
	Level.Init(-1.0f, Side * Side);
	for (int32 y = 0; y < Side; ++y)
	{
		for (int32 x = 0; x < Side; ++x)
		{
			const int32 i = World->Index(x, y);
			float L = World->WaterLevel[i];
			if (L < 0.0f)
			{
				for (int32 dy = -1; dy <= 1; ++dy)
					for (int32 dx = -1; dx <= 1; ++dx)
					{
						const int32 nx = x + dx, ny = y + dy;
						if (nx < 0 || ny < 0 || nx >= Side || ny >= Side) continue;
						L = FMath::Max(L, World->WaterLevel[World->Index(nx, ny)]);
					}
			}
			Level[i] = L;
		}
	}

	// Solo los vertices que usa alguna celda mojada entran en la malla.
	TArray<int32> Remap;
	Remap.Init(-1, Side * Side);
	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> UV0;
	TArray<FLinearColor> Colors;
	TArray<FProcMeshTangent> Tangents;
	TArray<int32> Triangles;

	auto Vertex = [&](int32 x, int32 y) -> int32
	{
		const int32 i = World->Index(x, y);
		if (Remap[i] >= 0) return Remap[i];
		const float LevelM = Level[i] * World->MaxHeight;
		Remap[i] = Vertices.Num();
		Vertices.Add(FVector(World->GridToWorldX(x) * MetersToUU,
		                     World->GridToWorldZ(y) * MetersToUU,
		                     LevelM * MetersToUU));
		Normals.Add(FVector::UpVector);
		Tangents.Add(FProcMeshTangent(1.0f, 0.0f, 0.0f));
		UV0.Add(FVector2D(World->GridToWorldX(x), World->GridToWorldZ(y)));   // metros
		// R = profundidad (0..1 en 10 m): orillas claras, centro profundo.
		const float Depth = FMath::Max(0.0f, LevelM - World->HeightMeters(x, y));
		Colors.Add(FLinearColor(FMath::Clamp(Depth / 10.0f, 0.0f, 1.0f), 0.0f, 0.0f, 1.0f));
		return Remap[i];
	};

	for (int32 y = 0; y < Res; ++y)
	{
		for (int32 x = 0; x < Res; ++x)
		{
			const int32 c[4] = { World->Index(x, y), World->Index(x + 1, y),
			                     World->Index(x, y + 1), World->Index(x + 1, y + 1) };
			bool bWet = false;
			for (int32 k = 0; k < 4; ++k) bWet |= World->WaterLevel[c[k]] >= 0.0f;
			if (!bWet) continue;

			const int32 i0 = Vertex(x, y),     i1 = Vertex(x + 1, y);
			const int32 i2 = Vertex(x, y + 1), i3 = Vertex(x + 1, y + 1);

			// Mismo orden que el terreno: las dos mallas miran hacia el mismo lado.
			if (!bInvertTriangleWinding)
			{
				Triangles.Append({ i0, i1, i2, i1, i3, i2 });
			}
			else
			{
				Triangles.Append({ i0, i2, i1, i1, i2, i3 });
			}
		}
	}

	if (Triangles.Num() == 0) return;

	WaterMesh = NewObject<UProceduralMeshComponent>(this, NAME_None, RF_Transient);
	WaterMesh->SetupAttachment(RootComponent);
	// Sin colision: se puede caminar por el fondo. No proyecta sombra: el agua
	// clara deja ver el fondo iluminado.
	WaterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WaterMesh->SetCastShadow(false);
	WaterMesh->RegisterComponent();

	const TArray<FVector2D> Empty;
	WaterMesh->CreateMeshSection_LinearColor(
		0, Vertices, Triangles, Normals, UV0, Empty, Empty, Empty, Colors, Tangents,
		/*bCreateCollision=*/ false);

	if (WaterMaterial)
	{
		WaterMesh->SetMaterial(0, WaterMaterial);
	}
}

// ---------------------------------------------------------------- props

void AJardinWorldActor::BuildProps()
{
	if (!World.IsValid()) return;

	// Las instancias anteriores ya las destruyo DestroyGeneratedComponents.

	// Traducir las capas del inspector a las del nucleo y repartir.
	std::vector<jardin::PropLayer> CoreLayers;
	CoreLayers.reserve(PropLayers.Num());
	for (const FJardinPropLayer& L : PropLayers)
	{
		CoreLayers.push_back(L.ToCore());
	}

	const jardin::SeedBook Seeds(World->MasterSeed);
	const jardin::ScatterResult Result = jardin::Scatterer::Scatter(*World, CoreLayers, Seeds);

	ScatterMs = static_cast<float>(Result.ElapsedMs);
	TotalProps = Result.Total();

	PropCounts.Reset();
	for (int32 Count : Result.CountPerLayer)
	{
		PropCounts.Add(Count);
	}

	// Un componente instanciado por cada (capa, variante).
	//
	// HISM da instancing, LOD automatico y culling por distancia sin trabajo
	// extra. Es la razon principal por la que miles de arboles no hunden el
	// rendimiento: se dibujan en una sola llamada por malla, no una por objeto.
	TArray<TArray<UStaticMesh*>> MeshesPerLayer;
	TArray<TArray<TArray<FTransform>>> Transforms;   // [capa][variante]
	MeshesPerLayer.SetNum(PropLayers.Num());
	Transforms.SetNum(PropLayers.Num());

	for (int32 i = 0; i < PropLayers.Num(); ++i)
	{
		MeshesPerLayer[i] = PropLayers[i].EffectiveMeshes();
		Transforms[i].SetNum(FMath::Max(1, MeshesPerLayer[i].Num()));
	}

	// Arboles fractales: cada objeto de esas capas es un arbol construido por
	// codigo, con su propia subsemilla derivada de la del jardin. Todos son
	// distintos y la misma semilla del jardin reproduce exactamente los mismos.
	TArray<int32> FractalIndex;
	FractalIndex.Init(0, PropLayers.Num());
	const uint32 FractalSeed = Seeds.For(jardin::SeedBook::Fractal);

	for (const jardin::Placement& P : Result.Placements)
	{
		if (!Transforms.IsValidIndex(P.LayerIndex)) continue;

		const FJardinPropLayer& Layer = PropLayers[P.LayerIndex];
		if (Layer.bFractalTree)
		{
			const uint32 LayerSeed = jardin::hash32::Combine(FractalSeed, jardin::hash32::Text(TCHAR_TO_UTF8(*Layer.Name)));
			const uint32 TreeSeed = jardin::hash32::Combine(LayerSeed, static_cast<uint32>(FractalIndex[P.LayerIndex]++));
			SpawnFractalTree(P, Layer, TreeSeed);
			continue;
		}

		TArray<TArray<FTransform>>& PerVariant = Transforms[P.LayerIndex];
		const int32 V = PerVariant.IsValidIndex(P.Variant) ? P.Variant : 0;

		PerVariant[V].Emplace(
			FRotator(0.0f, P.RotationDegrees, 0.0f),
			FVector(P.X * MetersToUU, P.Z * MetersToUU, P.HeightMeters * MetersToUU),
			FVector(P.Scale));
	}

	int32 SkippedNoMesh = 0;

	for (int32 i = 0; i < PropLayers.Num(); ++i)
	for (int32 v = 0; v < Transforms[i].Num(); ++v)
	{
		if (PropLayers[i].bFractalTree) break;   // ya se construyeron arriba
		if (PropLayers[i].bNightOnly && !bIsNight) break;   // de dia no se dibujan
		if (!PropLayers[i].bVisible) break;

		UStaticMesh* Mesh = MeshesPerLayer[i].IsValidIndex(v) ? MeshesPerLayer[i][v] : nullptr;
		if (Mesh == nullptr)
		{
			SkippedNoMesh += Transforms[i][v].Num();
			continue;
		}
		if (Transforms[i][v].Num() == 0)
		{
			continue;
		}

		// RF_Transient y sin AddInstanceComponent: nunca se escribe en el nivel.
		UHierarchicalInstancedStaticMeshComponent* HISM =
			NewObject<UHierarchicalInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);

		HISM->SetupAttachment(RootComponent);

		// Configurar ANTES de registrar: cada cambio sobre un componente ya
		// registrado reconstruye su estado de render, y aqui serian cuatro.
		HISM->SetStaticMesh(Mesh);

		// Los props no necesitan colision: el jugador camina sobre el terreno.
		// Cientos de colliders no aportan nada y cuestan rendimiento.
		HISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		// Las capas nocturnas son luz (material emisivo): no tapan al sol.
		HISM->SetCastShadow(!PropLayers[i].bNightOnly);
		if (PropLayers[i].bNightOnly)
		{
			// Vuelan hasta ~3 m de su punto de anclaje (World Position Offset)
			// y miden ~4 cm: se amplian sus limites a ~2.5 m para que el
			// culling no las borre cuando el anclaje sale de camara.
			HISM->SetBoundsScale(60.0f);

		}

		// Distancia de corte: la de la capa, o 80 m para las nocturnas (miden
		// ~4 cm). Empieza a desvanecerse al 85% si el material lo soporta.
		const float CullM = PropLayers[i].CullDistanceMeters > 0.0f ? PropLayers[i].CullDistanceMeters
			: (PropLayers[i].bNightOnly ? 80.0f : 0.0f);
		if (CullM > 0.0f)
		{
			const int32 EndUU = FMath::RoundToInt(CullM * MetersToUU);
			HISM->SetCullDistances(FMath::RoundToInt(EndUU * 0.85f), EndUU);
		}

		// En un mundo generado en tiempo de ejecucion los componentes tienen
		// que ser moviles; el instancing sigue funcionando igual.
		HISM->SetMobility(EComponentMobility::Movable);

		HISM->RegisterComponent();

		// Una sola llamada para toda la variante, en espacio local del actor.
		HISM->AddInstances(Transforms[i][v], /*bShouldReturnIndices=*/ false,
			/*bWorldSpace=*/ false);

		PropComponents.Add(HISM);
	}

	if (SkippedNoMesh > 0)
	{
		UE_LOG(LogJardin, Warning,
			TEXT("%d objetos calculados pero no dibujados: asigna una Static Mesh a cada capa."),
			SkippedNoMesh);
	}
}

// ---------------------------------------------------------------- fractal trees

void AJardinWorldActor::SpawnFractalTree(const jardin::Placement& P, const FJardinPropLayer& Layer, uint32 TreeSeed)
{
	// Cada ejemplar varia su forma (apertura, altura, copa...) segun su semilla.
	const jardin::FractalTreeParams Params =
		jardin::FractalTree::Vary(Layer.Fractal.ToCore(), TreeSeed, Layer.Fractal.Variation);
	const jardin::TreeMesh Tree = jardin::FractalTree::Build(Params, TreeSeed);
	if (Tree.Positions.empty()) return;

	// Tono propio del ejemplar, derivado de su semilla. El material lo lee del
	// canal G del color de vertice: asi cada arbol rosa tiene su matiz.
	const float Tint = jardin::hash32::ToUnit(jardin::hash32::Mix(TreeSeed ^ 0x9E3779B9u));

	// Dos secciones: 0 = madera, 1 = hojas. Cada una lleva su material.
	TArray<FVector> Verts[2], Norms[2];
	TArray<int32> Tris[2];
	TArray<FVector2D> UVs[2];
	TArray<FLinearColor> Cols[2];
	TArray<FProcMeshTangent> Tans[2];

	TArray<int32> Remap;
	Remap.Init(INDEX_NONE, static_cast<int32>(Tree.Positions.size()));

	auto AddVertex = [&](int32 Src) -> int32
	{
		const int32 S = Tree.Part[Src] == static_cast<uint8>(jardin::TreePart::Leaf) ? 1 : 0;
		if (Remap[Src] == INDEX_NONE)
		{
			const jardin::Vec3& Pos = Tree.Positions[Src];
			const jardin::Vec3& N = Tree.Normals[Src];
			// El arbol del nucleo ya es Z-up, en metros: solo se escala a cm.
			Remap[Src] = Verts[S].Add(FVector(Pos.X, Pos.Y, Pos.Z) * MetersToUU);
			Norms[S].Add(FVector(N.X, N.Y, N.Z));
			UVs[S].Add(FVector2D(0.0f, Tree.Flex[Src]));
			// R = flexibilidad (peso del viento de la Fase 2), G = tono del ejemplar.
			Cols[S].Add(FLinearColor(Tree.Flex[Src], Tint, 0.0f, 1.0f));
			Tans[S].Add(FProcMeshTangent(1.0f, 0.0f, 0.0f));
		}
		return Remap[Src];
	};

	for (size_t t = 0; t + 2 < Tree.Triangles.size(); t += 3)
	{
		const int32 A = Tree.Triangles[t], B = Tree.Triangles[t + 1], C = Tree.Triangles[t + 2];
		const int32 S = Tree.Part[A] == static_cast<uint8>(jardin::TreePart::Leaf) ? 1 : 0;

		// El nucleo emite caras antihorarias (mano derecha); Unreal espera el
		// orden contrario, igual que con el terreno. Se intercambian B y C.
		Tris[S].Add(AddVertex(A));
		Tris[S].Add(AddVertex(C));
		Tris[S].Add(AddVertex(B));
	}

	UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this, NAME_None, RF_Transient);
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetRelativeTransform(FTransform(
		FRotator(0.0f, P.RotationDegrees, 0.0f),
		FVector(P.X * MetersToUU, P.Z * MetersToUU, P.HeightMeters * MetersToUU),
		FVector(P.Scale)));

	// Colision del tronco y las ramas, si la capa la pide.
	Mesh->bUseComplexAsSimpleCollision = true;
	Mesh->SetCollisionEnabled(Layer.bFractalCollision ? ECollisionEnabled::QueryAndPhysics
	                                                  : ECollisionEnabled::NoCollision);
	Mesh->SetCollisionObjectType(ECC_WorldStatic);
	Mesh->SetCastShadow(true);
	Mesh->RegisterComponent();

	Mesh->CreateMeshSection_LinearColor(0, Verts[0], Tris[0], Norms[0], UVs[0], Cols[0], Tans[0],
		/*bCreateCollision=*/ Layer.bFractalCollision);
	Mesh->CreateMeshSection_LinearColor(1, Verts[1], Tris[1], Norms[1], UVs[1], Cols[1], Tans[1], /*bCreateCollision=*/ false);

	if (Layer.FractalBarkMaterial) Mesh->SetMaterial(0, Layer.FractalBarkMaterial);
	if (Layer.FractalLeafMaterial) Mesh->SetMaterial(1, Layer.FractalLeafMaterial);

	FractalComponents.Add(Mesh);
}

// ---------------------------------------------------------------- queries

float AJardinWorldActor::GetTerrainHeightAt(float WorldX, float WorldY) const
{
	if (!World.IsValid()) return 0.0f;

	// De unidades de Unreal a metros del nucleo, y de vuelta.
	const float MetersX = (WorldX - GetActorLocation().X) * UUToMeters;
	const float MetersZ = (WorldY - GetActorLocation().Y) * UUToMeters;

	return World->SampleHeightMeters(MetersX, MetersZ) * MetersToUU + GetActorLocation().Z;
}

// ---------------------------------------------------------------- determinism

void AJardinWorldActor::CheckDeterminism()
{
	const jardin::ZoneSet Zones = jardin::ZoneSet::Default();
	const jardin::GenerationSettings Core = Settings.ToCore();

	const uint32 Reference = jardin::WorldGenerator::Generate(Core, Zones).Checksum();
	bool bOk = true;

	for (int32 i = 1; i < 3; ++i)
	{
		const uint32 Current = jardin::WorldGenerator::Generate(Core, Zones).Checksum();
		if (Current != Reference)
		{
			bOk = false;
			UE_LOG(LogJardin, Error,
				TEXT("[Determinismo] Ejecucion %d dio %08X, se esperaba %08X"),
				i + 1, Current, Reference);
		}
	}

	// La prueba complementaria: un generador que siempre devuelva lo mismo
	// tambien pasaria la anterior.
	jardin::GenerationSettings Other = Core;
	Other.Seed = Core.Seed + 1;
	const uint32 OtherChecksum = jardin::WorldGenerator::Generate(Other, Zones).Checksum();

	if (OtherChecksum == Reference)
	{
		bOk = false;
		UE_LOG(LogJardin, Error,
			TEXT("[Variedad] Dos semillas distintas dieron %08X. La semilla no llega al generador."),
			Reference);
	}

	if (bOk)
	{
		UE_LOG(LogJardin, Log,
			TEXT("[Determinismo] CORRECTO. Semilla %d -> %08X, estable en 3 ejecuciones. Semilla %d -> %08X."),
			Core.Seed, Reference, Other.Seed, OtherChecksum);
	}
	else
	{
		UE_LOG(LogJardin, Error, TEXT("[Determinismo] FALLO. Revisa las causas en el encabezado de este metodo."));
	}
}

// ---------------------------------------------------------------- debug maps

namespace
{
	/** Gradiente de terreno: agua, pasto, roca, nieve. */
	FColor HeightGradient(float T)
	{
		struct FStop { float Pos; uint8 R, G, B; };
		static const FStop Stops[] = {
			{ 0.00f,  31,  51,  89 },
			{ 0.25f,  77, 115,  64 },
			{ 0.55f, 115, 140,  77 },
			{ 0.80f, 128, 115, 102 },
			{ 1.00f, 235, 235, 242 },
		};
		constexpr int32 Count = sizeof(Stops) / sizeof(Stops[0]);

		T = FMath::Clamp(T, 0.0f, 1.0f);

		for (int32 i = 1; i < Count; ++i)
		{
			if (T <= Stops[i].Pos)
			{
				const FStop& A = Stops[i - 1];
				const FStop& B = Stops[i];
				const float Span = B.Pos - A.Pos;
				const float K = Span > 0.0f ? (T - A.Pos) / Span : 0.0f;
				return FColor(
					static_cast<uint8>(FMath::Lerp<float>(A.R, B.R, K)),
					static_cast<uint8>(FMath::Lerp<float>(A.G, B.G, K)),
					static_cast<uint8>(FMath::Lerp<float>(A.B, B.B, K)),
					255);
			}
		}
		return FColor(Stops[Count - 1].R, Stops[Count - 1].G, Stops[Count - 1].B, 255);
	}

	/** Color plano por zona. Magenta = id sin color asignado. */
	FColor ZoneColor(uint8 Zone)
	{
		switch (Zone)
		{
		case 0:  return FColor(140, 184,  89, 255);   // Pradera
		case 1:  return FColor( 41,  97,  56, 255);   // Bosque
		// Fase 6: case 2: return FColor(115, 107, 97, 255);  // Rocosa-humeda
		default: return FColor(255,   0, 255, 255);
		}
	}
}

UTexture2D* AJardinWorldActor::CreateDebugTexture(EJardinDebugView View) const
{
	if (!World.IsValid() || View == EJardinDebugView::Normal)
	{
		return nullptr;
	}

	const int32 Side = World->Side;
	const int32 Count = Side * Side;

	TArray<FColor> Pixels;
	Pixels.SetNumUninitialized(Count);

	// UN solo constructor de texturas para TODOS los mapas.
	//
	// Agregar la vista de densidad, o la mascara del camino de la Fase 4, es
	// anadir un case aqui. La alternativa -un constructor especifico por mapa-
	// cuesta media tarde por cada mapa nuevo.
	for (int32 i = 0; i < Count; ++i)
	{
		switch (View)
		{
		case EJardinDebugView::Altura:
			Pixels[i] = HeightGradient(World->Height[i]);
			break;

		case EJardinDebugView::Humedad:
		{
			const uint8 V = static_cast<uint8>(FMath::Clamp(World->Moisture[i], 0.0f, 1.0f) * 255.0f);
			Pixels[i] = FColor(V, V, V, 255);
			break;
		}

		case EJardinDebugView::Pendiente:
		{
			const uint8 V = static_cast<uint8>(FMath::Clamp(World->Slope[i], 0.0f, 1.0f) * 255.0f);
			Pixels[i] = FColor(V, V, V, 255);
			break;
		}

		case EJardinDebugView::Zonas:
			Pixels[i] = ZoneColor(World->Zone[i]);
			break;

		case EJardinDebugView::Mezcla:
		{
			// Las fronteras difusas salen oscuras. En la Fase 0 es solo
			// diagnostico; en la Fase 6 es el dato que interpolara los bordes.
			const FColor Base = ZoneColor(World->Zone[i]);
			const float K = FMath::Lerp(0.25f, 1.0f, World->ZoneBlend[i]);
			Pixels[i] = FColor(
				static_cast<uint8>(Base.R * K),
				static_cast<uint8>(Base.G * K),
				static_cast<uint8>(Base.B * K),
				255);
			break;
		}

		case EJardinDebugView::Agua:
		{
			// Azul = agua; el gris se aclara al alejarse de la orilla (50 m).
			if (World->HasWater() && World->WaterLevel[i] >= 0.0f)
			{
				Pixels[i] = FColor(30, 110, 220, 255);
			}
			else
			{
				const float D = World->HasWater() ? World->WaterDistance[i] / 50.0f : 1.0f;
				const uint8 V = static_cast<uint8>(FMath::Clamp(D, 0.0f, 1.0f) * 200.0f + 40.0f);
				Pixels[i] = FColor(V, V, V, 255);
			}
			break;
		}

		case EJardinDebugView::Camino:
		{
			// Naranja = calzada; el arcen se desvanece hacia el gris del relieve.
			const FColor Base = HeightGradient(World->Height[i]);
			const uint8 G = static_cast<uint8>((Base.R + Base.G + Base.B) / 3);
			if (World->HasPath())
			{
				const float Half = World->Path.Width * 0.5f;
				const float T = 1.0f - FMath::SmoothStep(Half, Half + 6.0f, World->PathDistance[i]);
				Pixels[i] = FColor(
					static_cast<uint8>(FMath::Lerp(static_cast<float>(G), 255.0f, T)),
					static_cast<uint8>(FMath::Lerp(static_cast<float>(G), 140.0f, T)),
					static_cast<uint8>(FMath::Lerp(static_cast<float>(G), 20.0f, T)), 255);
			}
			else
			{
				Pixels[i] = FColor(G, G, G, 255);
			}
			break;
		}

		default:
			Pixels[i] = FColor::Black;
			break;
		}
	}

	UTexture2D* Texture = UTexture2D::CreateTransient(Side, Side, PF_B8G8R8A8);
	if (!Texture) return nullptr;

	// Sin suavizado: queremos ver el dato, no una fotografia de el.
	Texture->Filter = TextureFilter::TF_Nearest;
	Texture->AddressX = TA_Clamp;
	Texture->AddressY = TA_Clamp;
	Texture->SRGB = true;

	FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(Data, Pixels.GetData(), Count * sizeof(FColor));
	Mip.BulkData.Unlock();

	Texture->UpdateResource();

	return Texture;
}
