// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BaseZoneSubsystem.generated.h"

class ABaseZone;

/**
 * One-entry registry so code that spawns at runtime (servants) can find the base zone
 * without a level reference. The first registered zone wins; later ones are ignored with an error.
 */
UCLASS()
class SPOOKY_SPORES_API UBaseZoneSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

// -----------FUNCTIONS-----------
public:
	// Returns the registered zone, or nullptr when none is registered (yet). Do not cache a nullptr result.
	UFUNCTION(BlueprintPure, Category = "BaseZone")
	ABaseZone* GetBaseZone() const;

	// Called by ABaseZone::BeginPlay / EndPlay only.
	void RegisterZone(ABaseZone* Zone);
	void UnregisterZone(ABaseZone* Zone);

// -----------PROPERTIES-----------
private:
	TWeakObjectPtr<ABaseZone> RegisteredZone;
};
