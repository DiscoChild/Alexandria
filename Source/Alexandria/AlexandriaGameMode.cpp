// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Alexandria.h"
#include "AlexandriaGameMode.h"
#include "AlexandriaCharacter.h"

DEFINE_LOG_CATEGORY( AlexandriaLog );

AAlexandriaGameMode::AAlexandriaGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPersonCPP/Blueprints/ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
