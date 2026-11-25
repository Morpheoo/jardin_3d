// JardinCharacter.h — Exploracion en primera persona.
//
// Cumple el compromiso del protocolo: recorrer el mapa con teclado y raton,
// con gravedad, colisiones contra el terreno y limites del mapa.
//
// Deliberadamente minimo. No hay inventario, combate ni interaccion: el
// personaje existe para mirar el jardin, no para jugar en el.
//
// Los controles se leen directamente del teclado y el raton (IsInputKeyDown,
// GetInputMouseDelta) en lugar de usar Enhanced Input. Asi no hace falta
// crear ni configurar ningun asset de input en el editor: todo vive en codigo.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "JardinCharacter.generated.h"

class UCameraComponent;
class AJardinWorldActor;

UCLASS()
class JARDIN_API AJardinCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AJardinCharacter();

	virtual void Tick(float DeltaSeconds) override;

	/** Coloca al personaje sobre el terreno en su posicion horizontal actual.
	 *  Se llama al empezar y cada vez que el jardin se regenera, porque el
	 *  terreno cambia de forma debajo de el. */
	UFUNCTION(BlueprintCallable, Category = "Jardin")
	void SnapToTerrain();

	/** Piloto automatico (prueba de rendimiento): sin gravedad ni controles;
	 *  quien lo activa mueve al personaje. Al apagarlo vuelve a caminar. */
	void SetAutopilot(bool bOn);
	bool IsAutopilot() const { return bAutopilot; }

	// ---------------------------------------------------------------- ajustes

	/** Velocidad al caminar, en cm/s. 600 = 6 m/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jardin|Movimiento")
	float WalkSpeed = 600.0f;

	/** Velocidad con Shift, en cm/s. Con 1 km de mapa, correr importa. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jardin|Movimiento")
	float RunSpeed = 1400.0f;

	/** Grados por unidad de movimiento del raton. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jardin|Camara")
	float MouseSensitivity = 0.6f;

	/** Activalo si al subir el raton la camara mira hacia abajo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jardin|Camara")
	bool bInvertLook = false;

	/** Distancia minima al borde del mapa, en metros. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jardin|Limites")
	float BoundsMarginMeters = 3.0f;

protected:
	virtual void BeginPlay() override;

private:
	void HandleLook(APlayerController* PC);
	void HandleMove(APlayerController* PC);
	void EnforceBounds();

	AJardinWorldActor* FindGarden();

	UPROPERTY(VisibleAnywhere, Category = "Jardin")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(Transient)
	TObjectPtr<AJardinWorldActor> Garden;

	/** El primer Tick ocurre despues de todos los BeginPlay, cuando el jardin
	 *  ya se genero. Es el momento seguro para posarse sobre el terreno. */
	bool bSnappedOnce = false;

	bool bAutopilot = false;
};
