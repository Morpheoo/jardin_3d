// JardinHUD.h — Interfaz y modo de depuracion.
//
// Cumple el compromiso del protocolo: mostrar la semilla, los conteos de
// objetos y alternar entre la vista normal y los mapas del generador.
//
// Es lo que demuestra que el jardin no es decoracion: en pantalla se ve el
// dato numerico que produjo cada decision visual, y donde esta el jugador
// dentro de ese dato.
//
// Controles:
//   Tab  siguiente mapa (Normal, Altura, Humedad, Pendiente, Zonas, Fronteras)
//   N    semilla nueva: un jardin distinto al instante
//   R    regenerar con la misma semilla (debe salir identico)
//   H    mostrar u ocultar el panel
//   B    prueba de rendimiento: la camara recorre el camino a velocidad fija
//        y mide fps y tiempos de juego, render y GPU (otra B la detiene)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "JardinTypes.h"
#include "JardinHUD.generated.h"

class AJardinWorldActor;
class UTexture2D;

UCLASS()
class JARDIN_API AJardinHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	/** Lado del mini-mapa en pixeles. */
	UPROPERTY(EditAnywhere, Category = "Jardin")
	float MapSizePixels = 320.0f;

private:
	void HandleKeys();
	void RefreshMapTexture();
	void DrawPanel();
	void DrawMap(float X, float Y);

	AJardinWorldActor* FindGarden();

	// ---- prueba de rendimiento ----
	void StartBenchmark();
	void StopBenchmark(bool bReport);
	void TickBenchmark();
	void DrawBenchmark();

	bool bBenchmark = false;
	float BenchS = 0.0f;         // metros recorridos
	float BenchTime = 0.0f;      // segundos desde el inicio
	float BenchEnd = 0.0f;       // metros a recorrer
	TArray<float> FrameMs, GameMs, RenderMs, GpuMs;
	FString BenchReport;         // ultimo resultado, para mostrarlo
	double BenchReportUntil = 0.0;

	/** Velocidad de la camara en la prueba (m/s) y duracion maxima (s). */
	static constexpr float BenchSpeed = 12.0f;
	static constexpr float BenchMaxSeconds = 60.0f;
	/** Los primeros segundos no cuentan: carga de shaders y streaming. */
	static constexpr float BenchWarmup = 2.0f;

	UPROPERTY(Transient)
	TObjectPtr<AJardinWorldActor> Garden;

	/** UPROPERTY para que el recolector de basura no la destruya. */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> MapTexture;

	EJardinDebugView View = EJardinDebugView::Normal;
	bool bVisible = true;

	/** La textura solo se reconstruye si cambia la vista o el jardin. */
	EJardinDebugView TextureView = EJardinDebugView::Normal;
	FString TextureChecksum;
};
