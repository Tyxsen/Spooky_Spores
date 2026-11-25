// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PlayerUIWidget.generated.h"

class APawn;
class APlayerController;

UINTERFACE(BlueprintType)
class SPOOKY_SPORES_API UPlayerUIWidget : public UInterface
{
	GENERATED_BODY()
};

// Implemented by the widgets that need the player's pawn (stamina bar, resource counter...).
// The widget receives its dependencies, it never looks for them in the world.
class SPOOKY_SPORES_API IPlayerUIWidget
{
	GENERATED_BODY()

// -----------FUNCTIONS-----------
public:
	// Called at creation, then at every OnPossess (new pawn) and OnUnPossess (Pawn = nullptr).
	// Controller is always valid. The implementation must be idempotent (unbind from the previous pawn, then rebind).
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI")
	void InitializeForPlayer(APlayerController* Controller, APawn* Pawn);
};
