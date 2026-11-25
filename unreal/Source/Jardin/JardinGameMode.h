// JardinGameMode.h — Une el personaje en primera persona y el HUD.
//
// Esta configurado como GameMode por defecto del proyecto en
// Config/DefaultEngine.ini, asi que no hay que elegirlo en World Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "JardinGameMode.generated.h"

UCLASS()
class JARDIN_API AJardinGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AJardinGameMode();
};
