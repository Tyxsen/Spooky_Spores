// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "EnemyAIController.generated.h"

class AEnemyPathSpline;
class AEnemySoldier;
class UEnemyTargetSubsystem;
struct FPathFollowingResult;

UCLASS()
class SPOOKY_SPORES_API AEnemyAIController : public AAIController
{
	GENERATED_BODY()

// -----------FUNCTIONS-----------
public:
	AEnemyAIController();

	// Read-only access to the chosen target (nullptr outside Engage or while waiting), for tests.
	AActor* GetCurrentTarget() const;

	// Also called synchronously by MoveToLocation (Failed, AlreadyAtGoal) and by StopMovement (Aborted).
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	// Projects the target on the NavMesh and starts the move. Virtual so tests can replace it (test worlds have no NavMesh).
	// Returns Failed when the target cannot be projected.
	virtual EPathFollowingRequestResult::Type RequestMoveToTarget(AActor* Target);

private:
	void TickTransit(float DeltaSeconds);
	void TickEngage(float DeltaSeconds);

	// Releases the old claim, then picks among the living, non-excluded destructibles of the zone and claims the winner.
	// Leaves CurrentTarget empty (and logs once) when there is none.
	void ChooseTarget();

	void RequestMoveToCurrentTarget();

	// Excludes the current target for UnreachableRetryDelay, releases its claim and clears it: the next tick picks another.
	void HandleTargetUnreachable();

	// Same measure as the attack test: closest point of the target collision, minus the capsule radius, in 2D.
	bool IsInAttackRange(const AEnemySoldier& Soldier, const AActor& Target) const;

	UEnemyTargetSubsystem* GetTargetSubsystem() const;

	UFUNCTION()
	void HandleAttackTimer();

	enum class EEnemyState : uint8
	{
		Transit,
		Engage
	};

// -----------PROPERTIES-----------
private:
	EEnemyState CurrentState = EEnemyState::Transit;
	TWeakObjectPtr<AEnemyPathSpline> PathSplineActor;
	TWeakObjectPtr<AActor> CurrentTarget;
	float DistanceAlongSpline = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float MoveSpeed = 300.0f;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float LookAheadDistance = 100.0f;

	// Search box used to project the target on the NavMesh. Too small: silent failure near an obstacle
	// (the NavMesh is cut around it). Too large: the soldier may stop out of attack range.
	UPROPERTY(EditAnywhere, Category = "Movement|Navigation")
	FVector TargetProjectionExtent = FVector(300.0, 300.0, 750.0);

	// How long a target that could not be reached is ignored by this soldier.
	UPROPERTY(EditAnywhere, Category = "Movement|Navigation")
	float UnreachableRetryDelay = 3.0f;

	// Without the capsule radius: bStopOnOverlap adds it.
	UPROPERTY(EditAnywhere, Category = "Movement|Navigation")
	float ApproachAcceptanceRadius = 50.0f;

	// Set once the "no target in the base zone" error has been logged, so it is not repeated every tick.
	bool bHasLoggedNoTarget = false;

	// Same, for the "every target is unreachable" error.
	bool bHasLoggedNoReachableTarget = false;

	// A move request is in progress: no new one is sent while true; OnMoveCompleted ignores any end while this is false.
	bool bIsMovingToTarget = false;

	// Unreachable targets and the world time at which they become eligible again.
	TMap<TWeakObjectPtr<AActor>, double> ExcludedTargets;

	FTimerHandle AttackTimerHandle;
};
