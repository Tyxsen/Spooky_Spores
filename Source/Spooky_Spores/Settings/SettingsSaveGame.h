// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "SettingsSaveGame.generated.h"

// Save file of the player settings (slot "Settings"), independent from the game saves.
// Generic on purpose: adding a setting never touches this class.
UCLASS()
class SPOOKY_SPORES_API USettingsSaveGame : public USaveGame
{
	GENERATED_BODY()

// -----------PROPERTIES-----------
public:

	// Setting Id -> value. Read back as untrusted data (the file can be edited).
	UPROPERTY(SaveGame)
	TMap<FName, float> Values;
};
