#include "JardinHUD.h"
#include "JardinWorldActor.h"
#include "JardinCharacter.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "Misc/App.h"
#include "RenderTimer.h"
#include "DynamicRHI.h"

namespace
{
	const TCHAR* ViewName(EJardinDebugView V)
	{
		switch (V)
		{
		case EJardinDebugView::Normal:    return TEXT("Normal");
		case EJardinDebugView::Altura:    return TEXT("Mapa de altura");
		case EJardinDebugView::Humedad:   return TEXT("Mapa de humedad");
		case EJardinDebugView::Pendiente: return TEXT("Mapa de pendiente");
		case EJardinDebugView::Zonas:     return TEXT("Mapa de zonas");
		case EJardinDebugView::Mezcla:    return TEXT("Fronteras de zona");
		case EJardinDebugView::Agua:      return TEXT("Distancia al agua");
		case EJardinDebugView::Camino:    return TEXT("Camino");
		default:                          return TEXT("?");
		}
	}

	constexpr int32 ViewCount = 8;
}

AJardinWorldActor* AJardinHUD::FindGarden()
{
	if (!IsValid(Garden))
	{
		for (TActorIterator<AJardinWorldActor> It(GetWorld()); It; ++It)
		{
			Garden = *It;
			break;
		}
	}
	return Garden;
}

void AJardinHUD::DrawHUD()
{
	Super::DrawHUD();

	HandleKeys();
	TickBenchmark();

	if (!Canvas) return;
	DrawBenchmark();
	if (!bVisible) return;

	DrawPanel();
}

// ---------------------------------------------------------------- prueba de rendimiento
//
// Mismo recorrido siempre (el camino de la semilla, a 12 m/s), asi las cifras
// de antes y despues de un cambio se pueden comparar. Se mide:
//   frame   tiempo total entre frames (lo que da los fps)
//   juego   hilo de juego (logica, HUD, colocar la camara)
//   render  hilo de render (preparar lo que se dibuja: aqui pesan miles de instancias)
//   GPU     la tarjeta grafica (triangulos, sombras, Lumen, niebla)
// El mayor de juego/render/GPU es el cuello de botella. Ojo: con VSync o un
// tope de fps el frame no baja de 16.7 ms; por eso cuentan sobre todo los tres.

void AJardinHUD::StartBenchmark()
{
	AJardinWorldActor* G = FindGarden();
	AJardinCharacter* C = PlayerOwner ? Cast<AJardinCharacter>(PlayerOwner->GetPawn()) : nullptr;
	if (!G || !C || G->GetPathLength() < 50.0f)
	{
		BenchReport = TEXT("Prueba de rendimiento: hace falta un jardin con camino.");
		BenchReportUntil = FPlatformTime::Seconds() + 6.0;
		return;
	}
	FrameMs.Reset(); GameMs.Reset(); RenderMs.Reset(); GpuMs.Reset();
	BenchS = 0.0f;
	BenchTime = 0.0f;
	BenchEnd = FMath::Min(G->GetPathLength(), BenchSpeed * BenchMaxSeconds);
	bBenchmark = true;
	C->SetAutopilot(true);
}

void AJardinHUD::StopBenchmark(bool bReport)
{
	bBenchmark = false;
	if (AJardinCharacter* C = PlayerOwner ? Cast<AJardinCharacter>(PlayerOwner->GetPawn()) : nullptr)
	{
		C->SetAutopilot(false);
	}
	if (!bReport || FrameMs.Num() < 10) return;

	auto Avg = [](const TArray<float>& V) { double S = 0; for (float x : V) S += x; return V.Num() ? S / V.Num() : 0.0; };
	TArray<float> Sorted = FrameMs;
	Sorted.Sort();
	const float P99 = Sorted[FMath::Min(Sorted.Num() - 1, FMath::FloorToInt(Sorted.Num() * 0.99f))];
	const double FrameAvg = Avg(FrameMs);

	AJardinWorldActor* G = FindGarden();
	BenchReport = FString::Printf(
		TEXT("RENDIMIENTO  semilla %d | %d frames | %.0f fps (1%% bajo %.0f) | frame %.1f ms | juego %.1f | render %.1f | GPU %.1f ms | %d objetos"),
		G ? G->GetSeed() : 0, FrameMs.Num(), FrameAvg > 0 ? 1000.0 / FrameAvg : 0.0, P99 > 0 ? 1000.0f / P99 : 0.0f,
		FrameAvg, Avg(GameMs), Avg(RenderMs), Avg(GpuMs), G ? G->GetTotalProps() : 0);
	BenchReportUntil = FPlatformTime::Seconds() + 20.0;
	UE_LOG(LogTemp, Display, TEXT("%s"), *BenchReport);
}

void AJardinHUD::TickBenchmark()
{
	if (!bBenchmark) return;
	AJardinWorldActor* G = FindGarden();
	AJardinCharacter* C = PlayerOwner ? Cast<AJardinCharacter>(PlayerOwner->GetPawn()) : nullptr;
	if (!G || !C) { StopBenchmark(false); return; }

	const float Dt = static_cast<float>(FApp::GetDeltaTime());
	BenchTime += Dt;
	BenchS += BenchSpeed * Dt;
	if (BenchS >= BenchEnd) { StopBenchmark(true); return; }

	if (BenchTime > BenchWarmup)
	{
		FrameMs.Add(Dt * 1000.0f);
		GameMs.Add(FPlatformTime::ToMilliseconds(GGameThreadTime));
		RenderMs.Add(FPlatformTime::ToMilliseconds(GRenderThreadTime));
		GpuMs.Add(FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()));
	}

	// Ojos a 1.7 m sobre la calzada, mirando por donde va el camino un poco
	// hacia abajo: lo que veria alguien paseando.
	FVector Pos, Dir;
	if (!G->GetPathPose(BenchS, Pos, Dir)) { StopBenchmark(false); return; }
	// Como al posarse en el terreno: el centro de la capsula a media altura,
	// asi la camara queda a la altura de siempre.
	const float HalfHeight = C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	C->SetActorLocation(Pos + FVector(0, 0, HalfHeight + 20.0f), false, nullptr, ETeleportType::TeleportPhysics);
	FRotator R = Dir.Rotation();
	R.Pitch = -6.0f;
	PlayerOwner->SetControlRotation(R);
}

void AJardinHUD::DrawBenchmark()
{
	const float X = 24.0f;
	const float Y = Canvas->ClipY - 60.0f;
	if (bBenchmark)
	{
		const FString Msg = FString::Printf(TEXT("PRUEBA DE RENDIMIENTO  %.0f / %.0f m   (B para cancelar)"), BenchS, BenchEnd);
		DrawRect(FLinearColor(0, 0, 0, 0.6f), X - 10.0f, Y - 6.0f, 520.0f, 30.0f);
		DrawText(Msg, FLinearColor(1.0f, 0.85f, 0.3f), X, Y, GEngine->GetMediumFont());
	}
	else if (!BenchReport.IsEmpty() && FPlatformTime::Seconds() < BenchReportUntil)
	{
		DrawRect(FLinearColor(0, 0, 0, 0.6f), X - 10.0f, Y - 6.0f, Canvas->ClipX - 2.0f * X + 20.0f, 30.0f);
		DrawText(BenchReport, FLinearColor(0.6f, 1.0f, 0.6f), X, Y, GEngine->GetMediumFont());
	}
}

void AJardinHUD::HandleKeys()
{
	if (!PlayerOwner) return;

	if (PlayerOwner->WasInputKeyJustPressed(EKeys::H))
	{
		bVisible = !bVisible;
	}

	if (PlayerOwner->WasInputKeyJustPressed(EKeys::Tab))
	{
		View = static_cast<EJardinDebugView>((static_cast<int32>(View) + 1) % ViewCount);
	}

	AJardinWorldActor* G = FindGarden();
	if (!G) return;

	if (PlayerOwner->WasInputKeyJustPressed(EKeys::B))
	{
		if (bBenchmark) StopBenchmark(false);
		else StartBenchmark();
	}
	if (bBenchmark) return;   // durante la prueba no se regenera nada

	const bool bNewSeed = PlayerOwner->WasInputKeyJustPressed(EKeys::N);
	const bool bRegen = PlayerOwner->WasInputKeyJustPressed(EKeys::R);

	if (bNewSeed || bRegen)
	{
		if (bNewSeed) G->RandomizeSeedAndGenerate();
		else          G->Generate();

		// El terreno cambio de forma bajo el jugador: volver a posarlo.
		if (AJardinCharacter* C = Cast<AJardinCharacter>(PlayerOwner->GetPawn()))
		{
			C->SnapToTerrain();
		}
	}
}

void AJardinHUD::RefreshMapTexture()
{
	AJardinWorldActor* G = FindGarden();
	if (!G || !G->HasWorld() || View == EJardinDebugView::Normal)
	{
		MapTexture = nullptr;
		return;
	}

	const FString Current = G->GetChecksum();
	if (MapTexture && TextureView == View && TextureChecksum == Current)
	{
		return; // nada cambio
	}

	MapTexture = G->CreateDebugTexture(View);
	TextureView = View;
	TextureChecksum = Current;
}

void AJardinHUD::DrawPanel()
{
	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	AJardinWorldActor* G = FindGarden();

	const float X = 24.0f;
	float Y = 24.0f;
	const float Line = 22.0f;

	const FLinearColor Title(1.0f, 0.95f, 0.75f);
	const FLinearColor Text(0.92f, 0.92f, 0.92f);
	const FLinearColor Dim(0.65f, 0.65f, 0.65f);

	RefreshMapTexture();

	const float PanelW = MapTexture ? FMath::Max(360.0f, MapSizePixels + 40.0f) : 360.0f;
	const int32 LayerCount = G ? G->PropLayers.Num() : 0;
	const float PanelH = 12 * Line + LayerCount * Line + (MapTexture ? MapSizePixels + 16.0f : 0.0f) + 28.0f;

	// Fondo semitransparente para que el texto se lea sobre el cielo.
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f), X - 12.0f, Y - 10.0f, PanelW, PanelH);

	DrawText(TEXT("WORLD DEBUG"), Title, X, Y, Font, 1.1f);
	Y += Line * 1.3f;

	if (!G || !G->HasWorld())
	{
		DrawText(TEXT("No hay jardin generado en el nivel."), Text, X, Y, Font);
		return;
	}

	DrawText(FString::Printf(TEXT("Semilla: %d"), G->GetSeed()), Text, X, Y, Font);            Y += Line;
	DrawText(FString::Printf(TEXT("Mapa: %.0f x %.0f m"),
		G->GetWorldSizeUU() / 100.0f, G->GetWorldSizeUU() / 100.0f), Text, X, Y, Font);        Y += Line;
	DrawText(FString::Printf(TEXT("Relieve: %s"), *G->GetReliefName()), Text, X, Y, Font);      Y += Line;
	DrawText(FString::Printf(TEXT("Bioma: %s"), *G->GetPaletteName()), Text, X, Y, Font);      Y += Line;
	DrawText(G->GetPathLength() > 0.0f
		? FString::Printf(TEXT("Camino: %.0f m"), G->GetPathLength())
		: FString(TEXT("Camino: no")), Text, X, Y, Font);                                       Y += Line;
	if (G->bDenseGrass)
	{
		DrawText(FString::Printf(TEXT("Pasto denso: %d matas en %d trozos"), G->GetGrassInstanceCount(), G->GetGrassChunkCount()),
			Text, X, Y, Font);                                                                      Y += Line;
	}
	DrawText(FString::Printf(TEXT("Agua: %s (%.0f%%)"),
		*G->GetWaterProfileName(), G->GetWaterPercent()), Text, X, Y, Font);                    Y += Line;
	DrawText(FString::Printf(TEXT("Generado en %.1f ms"), G->GetLastGenerationMs()), Text, X, Y, Font); Y += Line;
	DrawText(FString::Printf(TEXT("Checksum: %s"), *G->GetChecksum()), Dim, X, Y, Font);       Y += Line * 1.3f;

	const TArray<int32> Counts = G->GetPropCounts();
	for (int32 i = 0; i < LayerCount && i < Counts.Num(); ++i)
	{
		DrawText(FString::Printf(TEXT("%s: %d"), *G->PropLayers[i].Name, Counts[i]), Text, X, Y, Font);
		Y += Line;
	}
	DrawText(FString::Printf(TEXT("Total: %d"), G->GetTotalProps()), Text, X, Y, Font);        Y += Line * 1.3f;

	DrawText(FString::Printf(TEXT("Vista: %s"), ViewName(View)), Title, X, Y, Font);           Y += Line;

	if (MapTexture)
	{
		Y += 6.0f;
		DrawMap(X, Y);
		Y += MapSizePixels + 10.0f;
	}

	DrawText(TEXT("Tab mapa · N semilla nueva · R regenerar · H ocultar · B rendimiento"), Dim, X, Y, Font, 0.85f);
}

void AJardinHUD::DrawMap(float X, float Y)
{
	const float Scale = MapSizePixels / static_cast<float>(MapTexture->GetSizeX());
	DrawTextureSimple(MapTexture, X, Y, Scale);

	// Posicion del jugador sobre el mapa.
	//
	// La fila y de la textura corresponde a la coordenada Y del mundo, asi que
	// basta normalizar la posicion respecto al jardin: no hace falta girar nada.
	AJardinWorldActor* G = FindGarden();
	APawn* Pawn = PlayerOwner ? PlayerOwner->GetPawn() : nullptr;
	if (!G || !Pawn) return;

	const float Size = G->GetWorldSizeUU();
	const FVector Rel = Pawn->GetActorLocation() - G->GetActorLocation();
	const float U = FMath::Clamp(Rel.X / Size + 0.5f, 0.0f, 1.0f);
	const float V = FMath::Clamp(Rel.Y / Size + 0.5f, 0.0f, 1.0f);

	const float PX = X + U * MapSizePixels;
	const float PY = Y + V * MapSizePixels;

	DrawRect(FLinearColor::Black, PX - 5.0f, PY - 5.0f, 10.0f, 10.0f);
	DrawRect(FLinearColor(1.0f, 0.2f, 0.2f), PX - 3.0f, PY - 3.0f, 6.0f, 6.0f);
}
