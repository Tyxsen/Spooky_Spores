// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "EnemyAIController.generated.h"

class AEnemyPathSpline;

UCLASS()
class SPOOKY_SPORES_API AEnemyAIController : public AAIController
{
	GENERATED_BODY()
	
public:
	AEnemyAIController();

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void TickTransit(float DeltaSeconds);
	void TickEngage(float DeltaSeconds);
	AActor* FindNearestDestructible() const;

	UFUNCTION()
	void HandleAttackTimer();

	enum class EEnemyState : uint8
	{
		Transit,
		Engage
	};

	EEnemyState CurrentState = EEnemyState::Transit;
	TWeakObjectPtr<AEnemyPathSpline> PathSplineActor;
	TWeakObjectPtr<AActor> CurrentTarget;
	float DistanceAlongSpline = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float MoveSpeed = 300.0f;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float LookAheadDistance = 100.0f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float TargetSearchRadius = 2000.0f;

	FTimerHandle AttackTimerHandle;
};
