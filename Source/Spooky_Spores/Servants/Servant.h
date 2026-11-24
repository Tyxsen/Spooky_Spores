// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Enemies/Destructible.h"
#include "Servant.generated.h"

class UHealthComponent;

// Resurrected servant spawned by the player from a Necrospore.
// Possessed by an AServantAIController as soon as it is spawned or placed: it wanders in the base zone.
// Destructible: enemies may target it. It dies (destroyed, no drop) when its health reaches 0.
UCLASS()
class SPOOKY_SPORES_API AServant : public ACharacter, public IDestructible
{
	GENERATED_BODY()

// -----------FUNCTIONS-----------
public:
	AServant();

	virtual UHealthComponent* GetHealthComponent_Implementation() const override;

	// Applies AttackDamage to Target. Ignored if Target is not valid.
	void Attack(AActor* Target);

	UFUNCTION(BlueprintPure, Category = "Combat")
	float GetAttackRange() const;

	UFUNCTION(BlueprintPure, Category = "Combat")
	float GetAttackInterval() const;

protected:
	// Register in / unregister from the UServantSubsystem (task registry)
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleDeath();

// -----------PROPERTIES-----------
private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHealthComponent> HealthComponent;

	UPROPERTY(EditAnywhere, Category = "Combat", meta = (AllowPrivateAccess = "true"))
	float AttackDamage = 15.0f;

	// Reach from the servant's capsule (closest point of the target's collision, 2D), like the enemy soldier.
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float AttackRange = 100.0f;

	// Time between two hits
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (AllowPrivateAccess = "true", ClampMin = "0.1"))
	float AttackInterval = 1.2f;
};
