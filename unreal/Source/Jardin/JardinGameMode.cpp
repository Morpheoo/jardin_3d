#include "JardinGameMode.h"
#include "JardinCharacter.h"
#include "JardinHUD.h"

AJardinGameMode::AJardinGameMode()
{
	DefaultPawnClass = AJardinCharacter::StaticClass();
	HUDClass = AJardinHUD::StaticClass();
}
