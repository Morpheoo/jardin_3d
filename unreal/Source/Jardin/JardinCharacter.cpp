#include "JardinCharacter.h"
#include "JardinWorldActor.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"

AJardinCharacter::AJardinCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// Capsula de una persona de ~1.80 m.
	GetCapsuleComponent()->InitCapsuleSize(35.0f, 90.0f);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0.0f, 0.0f, 70.0f)); // altura de los ojos
	Camera->bUsePawnControlRotation = true;
	Camera->SetFieldOfView(90.0f);

	// El cuerpo gira con el raton en horizontal; la inclinacion vertical
	// solo la aplica la camara. Inclinar el cuerpo haria que caminar hacia
	// delante apuntara al cielo.
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = WalkSpeed;
	Move->JumpZVelocity = 520.0f;
	Move->AirControl = 0.35f;
	// Colinas de un jardin, no acantilados: 50 grados es caminable.
	Move->SetWalkableFloorAngle(50.0f);
	Move->MaxStepHeight = 45.0f;
}

void AJardinCharacter::BeginPlay()
{
	Super::BeginPlay();
	FindGarden();
}

AJardinWorldActor* AJardinCharacter::FindGarden()
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

void AJardinCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bSnappedOnce)
	{
		SnapToTerrain();
		bSnappedOnce = true;
	}

	if (bAutopilot) return;

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC)
	{
		HandleLook(PC);
		HandleMove(PC);

		if (PC->WasInputKeyJustPressed(EKeys::SpaceBar))
		{
			Jump();
		}
	}

	EnforceBounds();
}

void AJardinCharacter::HandleLook(APlayerController* PC)
{
	float DX = 0.0f, DY = 0.0f;
	PC->GetInputMouseDelta(DX, DY);

	if (DX == 0.0f && DY == 0.0f) return;

	// Se modifica la rotacion de control directamente en lugar de usar
	// AddControllerPitchInput: asi el signo no depende de las escalas de
	// entrada heredadas de versiones antiguas del motor.
	FRotator R = PC->GetControlRotation();
	R.Yaw += DX * MouseSensitivity;

	const float Pitch = FRotator::NormalizeAxis(R.Pitch) + (bInvertLook ? -DY : DY) * MouseSensitivity;
	R.Pitch = FMath::Clamp(Pitch, -85.0f, 85.0f);
	R.Roll = 0.0f;

	PC->SetControlRotation(R);
}

void AJardinCharacter::HandleMove(APlayerController* PC)
{
	float Forward = 0.0f, Right = 0.0f;

	if (PC->IsInputKeyDown(EKeys::W) || PC->IsInputKeyDown(EKeys::Up))    Forward += 1.0f;
	if (PC->IsInputKeyDown(EKeys::S) || PC->IsInputKeyDown(EKeys::Down))  Forward -= 1.0f;
	if (PC->IsInputKeyDown(EKeys::D) || PC->IsInputKeyDown(EKeys::Right)) Right += 1.0f;
	if (PC->IsInputKeyDown(EKeys::A) || PC->IsInputKeyDown(EKeys::Left))  Right -= 1.0f;

	GetCharacterMovement()->MaxWalkSpeed =
		PC->IsInputKeyDown(EKeys::LeftShift) ? RunSpeed : WalkSpeed;

	if (Forward == 0.0f && Right == 0.0f) return;

	// Direcciones a partir solo del giro horizontal: mirar hacia arriba no
	// debe frenar ni desviar el paso.
	const FRotator Yaw(0.0f, PC->GetControlRotation().Yaw, 0.0f);
	const FVector Fwd = FRotationMatrix(Yaw).GetUnitAxis(EAxis::X);
	const FVector Rgt = FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y);

	// Normalizar evita que en diagonal se avance un 41% mas rapido.
	FVector Dir = Fwd * Forward + Rgt * Right;
	Dir.Normalize();

	AddMovementInput(Dir, 1.0f);
}

void AJardinCharacter::EnforceBounds()
{
	AJardinWorldActor* G = FindGarden();
	if (!G || !G->HasWorld()) return;

	const FVector Center = G->GetActorLocation();
	const float Half = G->GetWorldSizeUU() * 0.5f - BoundsMarginMeters * 100.0f;

	FVector P = GetActorLocation();
	const float CX = FMath::Clamp(P.X, Center.X - Half, Center.X + Half);
	const float CY = FMath::Clamp(P.Y, Center.Y - Half, Center.Y + Half);

	if (CX != P.X || CY != P.Y)
	{
		SetActorLocation(FVector(CX, CY, P.Z));
	}

	// Red de seguridad: si por algun motivo se atraviesa la malla, volver a
	// la superficie en lugar de caer al vacio.
	if (P.Z < G->GetTerrainHeightAt(P.X, P.Y) - 1000.0f)
	{
		SnapToTerrain();
	}
}

void AJardinCharacter::SetAutopilot(bool bOn)
{
	bAutopilot = bOn;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->StopMovementImmediately();
	Move->SetMovementMode(bOn ? MOVE_Flying : MOVE_Walking);
	SetActorEnableCollision(!bOn);
	if (!bOn) SnapToTerrain();
}

void AJardinCharacter::SnapToTerrain()
{
	AJardinWorldActor* G = FindGarden();
	if (!G || !G->HasWorld()) return;

	const FVector P = GetActorLocation();
	const float Ground = G->GetTerrainHeightAt(P.X, P.Y);
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	SetActorLocation(FVector(P.X, P.Y, Ground + HalfHeight + 50.0f),
		false, nullptr, ETeleportType::TeleportPhysics);

	GetCharacterMovement()->Velocity = FVector::ZeroVector;
}
