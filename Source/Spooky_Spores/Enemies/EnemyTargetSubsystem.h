// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "EnemyTargetSubsystem.generated.h"

class AController;

/**
 * Soft reservation of enemy targets: one claim per soldier controller, so soldiers spread over the destructibles
 * instead of piling up on the nearest one. Claims are weak, so a destroyed soldier or target never leaves a stale entry
 * (they are also released explicitly by AEnemyAIController). C++ only.
 */
UCLASS()
class SPOOKY_SPORES_API UEnemyTargetSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

// -----------FUNCTIONS-----------
public:
	// Replaces the previous claim of this claimant.
	void SetClaim(const AController* Claimant, AActor* Target);
	void ReleaseClaim(const AController* Claimant);

	// Number of live claimants whose claim is on this live target.
	int32 GetClaimCount(const AActor* Target) const;

	// Fewest claims wins (so a free candidate always beats an occupied one), ties go to the nearest in 2D.
	// The claimant's own claim is not counted. Returns nullptr when Candidates is empty.
	AActor* PickTarget(const AController* Claimant, const FVector& Origin, const TArray<AActor*>& Candidates) const;

// -----------PROPERTIES-----------
private:
	// One claim per controller. Not UPROPERTY on purpose: weak pointers only.
	TMap<TWeakObjectPtr<const AController>, TWeakObjectPtr<AActor>> Claims;
};
