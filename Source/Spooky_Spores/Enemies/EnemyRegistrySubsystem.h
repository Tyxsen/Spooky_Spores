// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "EnemyRegistrySubsystem.generated.h"

class AEnemySoldier;

/**
 * Registry of the enemy soldiers of a world, so that other systems (servant combat) can find living enemies
 * without iterating over every actor. Soldiers register on BeginPlay and unregister on EndPlay.
 * Weak pointers: the registry never keeps a soldier alive and never dangles. C++ only.
 */
UCLASS()
class SPOOKY_SPORES_API UEnemyRegistrySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

// -----------FUNCTIONS-----------
public:
	// Called by AEnemySoldier::BeginPlay / EndPlay only. Duplicates and unknown soldiers are ignored.
	void RegisterEnemy(AEnemySoldier* Enemy);
	void UnregisterEnemy(AEnemySoldier* Enemy);

	// Registered, valid and not dead soldiers (a dead soldier may still be registered until its EndPlay).
	TArray<AEnemySoldier*> GetLivingEnemies() const;

// -----------PROPERTIES-----------
private:
	// Not UPROPERTY on purpose: weak pointers only.
	TArray<TWeakObjectPtr<AEnemySoldier>> Enemies;
};
