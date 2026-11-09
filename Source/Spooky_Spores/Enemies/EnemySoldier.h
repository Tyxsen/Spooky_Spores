// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Resources/ResourceTypes.h"
#include "EnemySoldier.generated.h"

class UHealthComponent;
class AEnemyPathSpline;

UCLASS()
class SPOOKY_SPORES_API AEnemySoldier : public ACharacter
{
	GENERATED_BODY()

// -----------FUNCTIONS-----------
public:
	AEnemySoldier();

	void Attack(AActor* Target);

	UFUNCTION(BlueprintPure, Category = "Combat")
	float GetAttackRange() const;

	UFUNCTION(BlueprintPure, Category = "Combat")
	float GetAttackInterval() const;

	UFUNCTION(BlueprintPure, Category = "Movement")
	AEnemyPathSpline* GetPathSplineActor() const;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleDeath();

// -----------PROPERTIES-----------
private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHealthComponent> HealthComponent;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float AttackDamage = 10.0f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float AttackRange = 100.0f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float AttackInterval = 1.5f;

	UPROPERTY(EditAnywhere, Category = "Loot")
	TArray<FResourceDropEntry> Drops;

	UPROPERTY(EditAnywhere, Category = "Loot")
	float DropSpawnRadius = 30.0f;

	UPROPERTY(EditInstanceOnly, Category = "Movement")
	TObjectPtr<AEnemyPathSpline> PathSplineActor;
};
